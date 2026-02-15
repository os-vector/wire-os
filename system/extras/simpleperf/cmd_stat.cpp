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

#include <inttypes.h>
#include <stdio.h>
#include <chrono>
#include <string>
#include <vector>

#include <base/logging.h>
#include <base/strings.h>

#include "command.h"
#include "environment.h"
#include "event_selection_set.h"
#include "event_type.h"
#include "perf_event.h"
#include "utils.h"
#include "workload.h"

static std::vector<std::string> default_measured_event_types{
    "cpu-cycles", "stalled-cycles-frontend", "stalled-cycles-backend", "instructions",
    "branch-instructions", "branch-misses", "task-clock", "context-switches", "page-faults",
};

class StatCommandImpl {
 public:
  StatCommandImpl() : verbose_mode_(false), system_wide_collection_(false) {
  }

  bool Run(const std::vector<std::string>& args);

 private:
  bool ParseOptions(const std::vector<std::string>& args, std::vector<std::string>* non_option_args);
  bool AddMeasuredEventType(const std::string& event_type_name, bool report_unsupported_type = true);
  bool AddDefaultMeasuredEventTypes();
  bool ShowCounters(const std::map<const EventType*, std::vector<PerfCounter>>& counters_map,
                    std::chrono::steady_clock::duration counting_duration);

  EventSelectionSet event_selection_set_;
  bool verbose_mode_;
  bool system_wide_collection_;
};

bool StatCommandImpl::Run(const std::vector<std::string>& args) {
  // 1. Parse options.
  std::vector<std::string> workload_args;
  if (!ParseOptions(args, &workload_args)) {
    return false;
  }

  // 2. Add default measured event types.
  if (event_selection_set_.Empty()) {
    if (!AddDefaultMeasuredEventTypes()) {
      return false;
    }
  }

  // 3. Create workload.
  if (workload_args.empty()) {
    // TODO: change default workload to sleep 99999, and run stat until Ctrl-C.
    workload_args = std::vector<std::string>({"sleep", "1"});
  }
  std::unique_ptr<Workload> workload = Workload::CreateWorkload(workload_args);
  if (workload == nullptr) {
    return false;
  }

  // 4. Open perf_event_files.
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

  // 5. Count events while workload running.
  auto start_time = std::chrono::steady_clock::now();
  // If monitoring only one process, we use the enable_on_exec flag, and don't need to start
  // counting manually.
  if (system_wide_collection_) {
    if (!event_selection_set_.EnableEvents()) {
      return false;
    }
  }
  if (!workload->Start()) {
    return false;
  }
  workload->WaitFinish();
  auto end_time = std::chrono::steady_clock::now();

  // 6. Read and print counters.
  std::map<const EventType*, std::vector<PerfCounter>> counters_map;
  if (!event_selection_set_.ReadCounters(&counters_map)) {
    return false;
  }
  if (!ShowCounters(counters_map, end_time - start_time)) {
    return false;
  }
  return true;
}

bool StatCommandImpl::ParseOptions(const std::vector<std::string>& args,
                                   std::vector<std::string>* non_option_args) {
  size_t i;
  for (i = 1; i < args.size() && args[i].size() > 0 && args[i][0] == '-'; ++i) {
    if (args[i] == "-a") {
      system_wide_collection_ = true;
    } else if (args[i] == "-e") {
      if (!NextArgumentOrError(args, &i)) {
        return false;
      }
      std::vector<std::string> event_types = android::base::Split(args[i], ",");
      for (auto& event_type : event_types) {
        if (!AddMeasuredEventType(event_type)) {
          return false;
        }
      }
    } else if (args[i] == "--verbose") {
      verbose_mode_ = true;
    } else {
      LOG(ERROR) << "Unknown option for stat command: " << args[i];
      LOG(ERROR) << "Try `simpleperf help stat`";
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

bool StatCommandImpl::AddMeasuredEventType(const std::string& event_type_name,
                                           bool report_unsupported_type) {
  const EventType* event_type =
      EventTypeFactory::FindEventTypeByName(event_type_name, report_unsupported_type);
  if (event_type == nullptr) {
    return false;
  }
  event_selection_set_.AddEventType(*event_type);
  return true;
}

bool StatCommandImpl::AddDefaultMeasuredEventTypes() {
  for (auto& name : default_measured_event_types) {
    // It is not an error when some event types in the default list are not supported by the kernel.
    AddMeasuredEventType(name, false);
  }
  if (event_selection_set_.Empty()) {
    LOG(ERROR) << "Failed to add any supported default measured types";
    return false;
  }
  return true;
}

bool StatCommandImpl::ShowCounters(
    const std::map<const EventType*, std::vector<PerfCounter>>& counters_map,
    std::chrono::steady_clock::duration counting_duration) {
  printf("Performance counter statistics:\n\n");

  for (auto& pair : counters_map) {
    auto& event_type = pair.first;
    auto& counters = pair.second;
    if (verbose_mode_) {
      for (auto& counter : counters) {
        printf("%s: value %'" PRId64 ", time_enabled %" PRId64 ", time_running %" PRId64
               ", id %" PRId64 "\n",
               event_selection_set_.FindEventFileNameById(counter.id).c_str(), counter.value,
               counter.time_enabled, counter.time_running, counter.id);
      }
    }

    PerfCounter sum_counter;
    memset(&sum_counter, 0, sizeof(sum_counter));
    for (auto& counter : counters) {
      sum_counter.value += counter.value;
      sum_counter.time_enabled += counter.time_enabled;
      sum_counter.time_running += counter.time_running;
    }
    bool scaled = false;
    int64_t scaled_count = sum_counter.value;
    if (sum_counter.time_running < sum_counter.time_enabled) {
      if (sum_counter.time_running == 0) {
        scaled_count = 0;
      } else {
        scaled = true;
        scaled_count = static_cast<int64_t>(static_cast<double>(sum_counter.value) *
                                            sum_counter.time_enabled / sum_counter.time_running);
      }
    }
    printf("%'30" PRId64 "%s  %s\n", scaled_count, scaled ? "(scaled)" : "       ",
           event_type->name);
  }
  printf("\nTotal test time: %lf seconds.\n",
         std::chrono::duration_cast<std::chrono::duration<double>>(counting_duration).count());
  return true;
}

class StatCommand : public Command {
 public:
  StatCommand()
      : Command("stat", "gather performance counter information",
                "Usage: simpleperf stat [options] [command [command-args]]\n"
                "    Gather performance counter information of running [command]. If [command]\n"
                "    is not specified, sleep 1 is used instead.\n\n"
                "    -a           Collect system-wide information.\n"
                "    -e event1,event2,... Select the event list to count. Use `simpleperf list`\n"
                "                         to find all possible event names.\n"
                "    --verbose    Show result in verbose mode.\n") {
  }

  bool Run(const std::vector<std::string>& args) override {
    // Keep the implementation in StatCommandImpl, so the resources used are cleaned up when the
    // command finishes. This is useful when we need to call some commands multiple times, like
    // in unit tests.
    StatCommandImpl impl;
    return impl.Run(args);
  }
};

StatCommand stat_command;
