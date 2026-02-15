/*
 * Copyright (C) 2015 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <libgen.h>
#include <poll.h>
#include <signal.h>
#include <string>
#include <vector>

#include <base/logging.h>
#include <base/strings.h>

#include "command.h"
#include "environment.h"
#include "event_selection_set.h"
#include "event_type.h"
#include "read_elf.h"
#include "record.h"
#include "record_file.h"
#include "utils.h"
#include "workload.h"

static std::string default_measured_event_type = "cpu-cycles";

class RecordCommandImpl {
 public:
  RecordCommandImpl()
      : use_sample_freq_(true),
        sample_freq_(1000),
        system_wide_collection_(false),
        measured_event_type_(nullptr),
        perf_mmap_pages_(256),
        record_filename_("perf.data") {
    // We need signal SIGCHLD to break poll().
    saved_sigchild_handler_ = signal(SIGCHLD, [](int) {});
  }

  ~RecordCommandImpl() {
    signal(SIGCHLD, saved_sigchild_handler_);
  }

  bool Run(const std::vector<std::string>& args);

  static bool ReadMmapDataCallback(const char* data, size_t size);

 private:
  bool ParseOptions(const std::vector<std::string>& args, std::vector<std::string>* non_option_args);
  bool SetMeasuredEventType(const std::string& event_type_name);
  void SetEventSelection();
  bool WriteData(const char* data, size_t size);
  bool DumpKernelAndModuleMmaps();
  bool DumpThreadCommAndMmaps();
  bool DumpAdditionalFeatures();
  bool DumpBuildIdFeature();

  bool use_sample_freq_;    // Use sample_freq_ when true, otherwise using sample_period_.
  uint64_t sample_freq_;    // Sample 'sample_freq_' times per second.
  uint64_t sample_period_;  // Sample once when 'sample_period_' events occur.

  bool system_wide_collection_;
  const EventType* measured_event_type_;
  EventSelectionSet event_selection_set_;

  // mmap pages used by each perf event file, should be power of 2.
  const size_t perf_mmap_pages_;

  std::string record_filename_;
  std::unique_ptr<RecordFileWriter> record_file_writer_;

  sighandler_t saved_sigchild_handler_;
};

bool RecordCommandImpl::Run(const std::vector<std::string>& args) {
  // 1. Parse options, and use default measured event type if not given.
  std::vector<std::string> workload_args;
  if (!ParseOptions(args, &workload_args)) {
    return false;
  }
  if (measured_event_type_ == nullptr) {
    if (!SetMeasuredEventType(default_measured_event_type)) {
      return false;
    }
  }
  SetEventSelection();

  // 2. Create workload.
  if (workload_args.empty()) {
    // TODO: change default workload to sleep 99999, and run record until Ctrl-C.
    workload_args = std::vector<std::string>({"sleep", "1"});
  }
  std::unique_ptr<Workload> workload = Workload::CreateWorkload(workload_args);
  if (workload == nullptr) {
    return false;
  }

  // 3. Open perf_event_files, create memory mapped buffers for perf_event_files, add prepare poll
  //    for perf_event_files.
  if (system_wide_collection_) {
    if (!event_selection_set_.OpenEventFilesForAllCpus()) {
      return false;
    }
  } else {
    event_selection_set_.EnableOnExec();
    if (!event_selection_set_.OpenEventFilesForProcess(workload->GetPid())) {
      return false;
    }
  }
  if (!event_selection_set_.MmapEventFiles(perf_mmap_pages_)) {
    return false;
  }
  std::vector<pollfd> pollfds;
  event_selection_set_.PreparePollForEventFiles(&pollfds);

  // 4. Open record file writer, and dump kernel/modules/threads mmap information.
  record_file_writer_ = RecordFileWriter::CreateInstance(
      record_filename_, event_selection_set_.FindEventAttrByType(*measured_event_type_),
      event_selection_set_.FindEventFdsByType(*measured_event_type_));
  if (record_file_writer_ == nullptr) {
    return false;
  }
  if (!DumpKernelAndModuleMmaps()) {
    return false;
  }
  if (system_wide_collection_ && !DumpThreadCommAndMmaps()) {
    return false;
  }

  // 5. Write records in mmap buffers of perf_event_files to output file while workload is running.

  // If monitoring only one process, we use the enable_on_exec flag, and don't need to start
  // recording manually.
  if (system_wide_collection_) {
    if (!event_selection_set_.EnableEvents()) {
      return false;
    }
  }
  if (!workload->Start()) {
    return false;
  }
  auto callback =
      std::bind(&RecordCommandImpl::WriteData, this, std::placeholders::_1, std::placeholders::_2);
  while (true) {
    if (!event_selection_set_.ReadMmapEventData(callback)) {
      return false;
    }
    if (workload->IsFinished()) {
      break;
    }
    poll(&pollfds[0], pollfds.size(), -1);
  }

  // 6. Dump additional features, and close record file.
  if (!DumpAdditionalFeatures()) {
    return false;
  }
  if (!record_file_writer_->Close()) {
    return false;
  }
  return true;
}

bool RecordCommandImpl::ParseOptions(const std::vector<std::string>& args,
                                     std::vector<std::string>* non_option_args) {
  size_t i;
  for (i = 1; i < args.size() && args[i].size() > 0 && args[i][0] == '-'; ++i) {
    if (args[i] == "-a") {
      system_wide_collection_ = true;
    } else if (args[i] == "-c") {
      if (!NextArgumentOrError(args, &i)) {
        return false;
      }
      char* endptr;
      sample_period_ = strtoull(args[i].c_str(), &endptr, 0);
      if (*endptr != '\0' || sample_period_ == 0) {
        LOG(ERROR) << "Invalid sample period: '" << args[i] << "'";
        return false;
      }
      use_sample_freq_ = false;
    } else if (args[i] == "-e") {
      if (!NextArgumentOrError(args, &i)) {
        return false;
      }
      if (!SetMeasuredEventType(args[i])) {
        return false;
      }
    } else if (args[i] == "-f" || args[i] == "-F") {
      if (!NextArgumentOrError(args, &i)) {
        return false;
      }
      char* endptr;
      sample_freq_ = strtoull(args[i].c_str(), &endptr, 0);
      if (*endptr != '\0' || sample_freq_ == 0) {
        LOG(ERROR) << "Invalid sample frequency: '" << args[i] << "'";
        return false;
      }
      use_sample_freq_ = true;
    } else if (args[i] == "-o") {
      if (!NextArgumentOrError(args, &i)) {
        return false;
      }
      record_filename_ = args[i];
    } else {
      LOG(ERROR) << "Unknown option for record command: '" << args[i] << "'\n";
      LOG(ERROR) << "Try `simpleperf help record`";
      return false;
    }
  }

  if (non_option_args != nullptr) {
    non_option_args->clear();
    for (; i < args.size(); ++i) {
      non_option_args->push_back(args[i]);
    }
  }
  return true;
}

bool RecordCommandImpl::SetMeasuredEventType(const std::string& event_type_name) {
  const EventType* event_type = EventTypeFactory::FindEventTypeByName(event_type_name);
  if (event_type == nullptr) {
    return false;
  }
  measured_event_type_ = event_type;
  return true;
}

void RecordCommandImpl::SetEventSelection() {
  event_selection_set_.AddEventType(*measured_event_type_);
  if (use_sample_freq_) {
    event_selection_set_.SetSampleFreq(sample_freq_);
  } else {
    event_selection_set_.SetSamplePeriod(sample_period_);
  }
  event_selection_set_.SampleIdAll();
}

bool RecordCommandImpl::WriteData(const char* data, size_t size) {
  return record_file_writer_->WriteData(data, size);
}

bool RecordCommandImpl::DumpKernelAndModuleMmaps() {
  KernelMmap kernel_mmap;
  std::vector<ModuleMmap> module_mmaps;
  if (!GetKernelAndModuleMmaps(&kernel_mmap, &module_mmaps)) {
    return false;
  }
  const perf_event_attr& attr = event_selection_set_.FindEventAttrByType(*measured_event_type_);
  MmapRecord mmap_record = CreateMmapRecord(attr, true, UINT_MAX, 0, kernel_mmap.start_addr,
                                            kernel_mmap.len, kernel_mmap.pgoff, kernel_mmap.name);
  if (!record_file_writer_->WriteData(mmap_record.BinaryFormat())) {
    return false;
  }
  for (auto& module_mmap : module_mmaps) {
    std::string filename = module_mmap.filepath;
    if (filename.empty()) {
      filename = "[" + module_mmap.name + "]";
    }
    MmapRecord mmap_record = CreateMmapRecord(attr, true, UINT_MAX, 0, module_mmap.start_addr,
                                              module_mmap.len, 0, filename);
    if (!record_file_writer_->WriteData(mmap_record.BinaryFormat())) {
      return false;
    }
  }
  return true;
}

bool RecordCommandImpl::DumpThreadCommAndMmaps() {
  std::vector<ThreadComm> thread_comms;
  if (!GetThreadComms(&thread_comms)) {
    return false;
  }
  const perf_event_attr& attr = event_selection_set_.FindEventAttrByType(*measured_event_type_);
  for (auto& thread : thread_comms) {
    CommRecord record = CreateCommRecord(attr, thread.tgid, thread.tid, thread.comm);
    if (!record_file_writer_->WriteData(record.BinaryFormat())) {
      return false;
    }
    if (thread.is_process) {
      std::vector<ThreadMmap> thread_mmaps;
      if (!GetThreadMmapsInProcess(thread.tgid, &thread_mmaps)) {
        // The thread may exit before we get its info.
        continue;
      }
      for (auto& thread_mmap : thread_mmaps) {
        if (thread_mmap.executable == 0) {
          continue;  // No need to dump non-executable mmap info.
        }
        MmapRecord record =
            CreateMmapRecord(attr, false, thread.tgid, thread.tid, thread_mmap.start_addr,
                             thread_mmap.len, thread_mmap.pgoff, thread_mmap.name);
        if (!record_file_writer_->WriteData(record.BinaryFormat())) {
          return false;
        }
      }
    }
  }
  return true;
}

bool RecordCommandImpl::DumpAdditionalFeatures() {
  if (!record_file_writer_->WriteFeatureHeader(1)) {
    return false;
  }
  return DumpBuildIdFeature();
}

bool RecordCommandImpl::DumpBuildIdFeature() {
  std::vector<std::string> hit_kernel_modules;
  std::vector<std::string> hit_user_files;
  if (!record_file_writer_->GetHitModules(&hit_kernel_modules, &hit_user_files)) {
    return false;
  }
  std::vector<BuildIdRecord> build_id_records;
  BuildId build_id;
  // Add build_ids for kernel/modules.
  for (auto& filename : hit_kernel_modules) {
    if (filename == DEFAULT_KERNEL_MMAP_NAME) {
      if (!GetKernelBuildId(&build_id)) {
        LOG(DEBUG) << "can't read build_id for kernel";
        continue;
      }
      build_id_records.push_back(
          CreateBuildIdRecord(true, UINT_MAX, build_id, DEFAULT_KERNEL_FILENAME_FOR_BUILD_ID));
    } else {
      std::string module_name = basename(&filename[0]);
      if (android::base::EndsWith(module_name, ".ko")) {
        module_name = module_name.substr(0, module_name.size() - 3);
      }
      if (!GetModuleBuildId(module_name, &build_id)) {
        LOG(DEBUG) << "can't read build_id for module " << module_name;
        continue;
      }
      build_id_records.push_back(CreateBuildIdRecord(true, UINT_MAX, build_id, filename));
    }
  }
  // Add build_ids for user elf files.
  for (auto& filename : hit_user_files) {
    if (filename == DEFAULT_EXECNAME_FOR_THREAD_MMAP) {
      continue;
    }
    if (!GetBuildIdFromElfFile(filename, &build_id)) {
      LOG(DEBUG) << "can't read build_id from file " << filename;
      continue;
    }
    build_id_records.push_back(CreateBuildIdRecord(false, UINT_MAX, build_id, filename));
  }
  if (!record_file_writer_->WriteBuildIdFeature(build_id_records)) {
    return false;
  }
  return true;
}

class RecordCommand : public Command {
 public:
  RecordCommand()
      : Command("record", "record sampling info in perf.data",
                "Usage: simpleperf record [options] [command [command-args]]\n"
                "    Gather sampling information when running [command]. If [command]\n"
                "    is not specified, sleep 1 is used instead.\n"
                "    -a           System-wide collection.\n"
                "    -c count     Set event sample period.\n"
                "    -e event     Select the event to sample (Use `simpleperf list`)\n"
                "                 to find all possible event names.\n"
                "    -f freq      Set event sample frequency.\n"
                "    -F freq      Same as '-f freq'.\n"
                "    -o record_file_name    Set record file name, default is perf.data.\n") {
  }

  bool Run(const std::vector<std::string>& args) override {
    RecordCommandImpl impl;
    return impl.Run(args);
  }
};

RecordCommand record_command;
