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

#include <map>
#include <string>
#include <vector>

#include <base/logging.h>
#include <base/stringprintf.h>

#include "command.h"
#include "event_attr.h"
#include "record.h"
#include "record_file.h"

using namespace PerfFileFormat;

class DumpRecordCommandImpl {
 public:
  DumpRecordCommandImpl() : record_filename_("perf.data") {
  }

  bool Run(const std::vector<std::string>& args);

 private:
  bool ParseOptions(const std::vector<std::string>& args);
  void DumpFileHeader();
  void DumpAttrSection();
  void DumpDataSection();
  void DumpFeatureSection();

  std::string record_filename_;
  std::unique_ptr<RecordFileReader> record_file_reader_;

  std::vector<int> features_;
};

bool DumpRecordCommandImpl::Run(const std::vector<std::string>& args) {
  if (!ParseOptions(args)) {
    return false;
  }
  record_file_reader_ = RecordFileReader::CreateInstance(record_filename_);
  if (record_file_reader_ == nullptr) {
    return false;
  }
  DumpFileHeader();
  DumpAttrSection();
  DumpDataSection();
  DumpFeatureSection();

  return true;
}

bool DumpRecordCommandImpl::ParseOptions(const std::vector<std::string>& args) {
  if (args.size() == 2) {
    record_filename_ = args[1];
  }
  return true;
}

static const std::string GetFeatureName(int feature);

void DumpRecordCommandImpl::DumpFileHeader() {
  const FileHeader* header = record_file_reader_->FileHeader();
  printf("magic: ");
  for (size_t i = 0; i < 8; ++i) {
    printf("%c", header->magic[i]);
  }
  printf("\n");
  printf("header_size: %" PRId64 "\n", header->header_size);
  if (header->header_size != sizeof(*header)) {
    PLOG(WARNING) << "record file header size doesn't match expected header size "
                  << sizeof(*header);
  }
  printf("attr_size: %" PRId64 "\n", header->attr_size);
  if (header->attr_size != sizeof(FileAttr)) {
    PLOG(WARNING) << "record file attr size doesn't match expected attr size " << sizeof(FileAttr);
  }
  printf("attrs[file section]: offset %" PRId64 ", size %" PRId64 "\n", header->attrs.offset,
         header->attrs.size);
  printf("data[file section]: offset %" PRId64 ", size %" PRId64 "\n", header->data.offset,
         header->data.size);
  printf("event_types[file section]: offset %" PRId64 ", size %" PRId64 "\n",
         header->event_types.offset, header->event_types.size);

  features_.clear();
  for (size_t i = 0; i < FEAT_MAX_NUM; ++i) {
    size_t j = i / 8;
    size_t k = i % 8;
    if ((header->features[j] & (1 << k)) != 0) {
      features_.push_back(i);
    }
  }
  for (auto& feature : features_) {
    printf("feature: %s\n", GetFeatureName(feature).c_str());
  }
}

static const std::string GetFeatureName(int feature) {
  static std::map<int, std::string> feature_name_map = {
      {FEAT_TRACING_DATA, "tracing_data"},
      {FEAT_BUILD_ID, "build_id"},
      {FEAT_HOSTNAME, "hostname"},
      {FEAT_OSRELEASE, "osrelease"},
      {FEAT_VERSION, "version"},
      {FEAT_ARCH, "arch"},
      {FEAT_NRCPUS, "nrcpus"},
      {FEAT_CPUDESC, "cpudesc"},
      {FEAT_CPUID, "cpuid"},
      {FEAT_TOTAL_MEM, "total_mem"},
      {FEAT_CMDLINE, "cmdline"},
      {FEAT_EVENT_DESC, "event_desc"},
      {FEAT_CPU_TOPOLOGY, "cpu_topology"},
      {FEAT_NUMA_TOPOLOGY, "numa_topology"},
      {FEAT_BRANCH_STACK, "branck_stack"},
      {FEAT_PMU_MAPPINGS, "pmu_mappings"},
      {FEAT_GROUP_DESC, "group_desc"},
  };
  auto it = feature_name_map.find(feature);
  if (it != feature_name_map.end()) {
    return it->second;
  }
  return android::base::StringPrintf("unknown_feature(%d)", feature);
}

void DumpRecordCommandImpl::DumpAttrSection() {
  std::vector<const FileAttr*> attrs = record_file_reader_->AttrSection();
  for (size_t i = 0; i < attrs.size(); ++i) {
    auto& attr = attrs[i];
    printf("file_attr %zu:\n", i + 1);
    DumpPerfEventAttr(attr->attr, 1);
    printf("  ids[file_section]: offset %" PRId64 ", size %" PRId64 "\n", attr->ids.offset,
           attr->ids.size);
    std::vector<uint64_t> ids = record_file_reader_->IdsForAttr(attr);
    if (ids.size() > 0) {
      printf("  ids:");
      for (auto& id : ids) {
        printf(" %" PRId64, id);
      }
      printf("\n");
    }
  }
}

void DumpRecordCommandImpl::DumpDataSection() {
  std::vector<std::unique_ptr<const Record>> records = record_file_reader_->DataSection();
  for (auto& record : records) {
    record->Dump();
  }
}

void DumpRecordCommandImpl::DumpFeatureSection() {
  std::vector<SectionDesc> sections = record_file_reader_->FeatureSectionDescriptors();
  CHECK_EQ(sections.size(), features_.size());
  for (size_t i = 0; i < features_.size(); ++i) {
    int feature = features_[i];
    SectionDesc& section = sections[i];
    printf("feature section for %s: offset %" PRId64 ", size %" PRId64 "\n",
           GetFeatureName(feature).c_str(), section.offset, section.size);
    if (feature == FEAT_BUILD_ID) {
      const char* p = record_file_reader_->DataAtOffset(section.offset);
      const char* end = p + section.size;
      while (p < end) {
        const perf_event_header* header = reinterpret_cast<const perf_event_header*>(p);
        CHECK_LE(p + header->size, end);
        CHECK_EQ(PERF_RECORD_BUILD_ID, header->type);
        BuildIdRecord record(header);
        record.Dump(1);
        p += header->size;
      }
    }
  }
}

class DumpRecordCommand : public Command {
 public:
  DumpRecordCommand()
      : Command("dump", "dump perf record file",
                "Usage: simpleperf dumprecord [options] [perf_record_file]\n"
                "    Dump different parts of a perf record file. Default file is perf.data.\n") {
  }

  bool Run(const std::vector<std::string>& args) override {
    DumpRecordCommandImpl impl;
    return impl.Run(args);
  }
};

DumpRecordCommand dumprecord_cmd;
