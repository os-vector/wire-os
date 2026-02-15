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

#ifndef SIMPLE_PERF_RECORD_FILE_FORMAT_H_
#define SIMPLE_PERF_RECORD_FILE_FORMAT_H_

#include "perf_event.h"

// The file structure of perf.data:
//    file_header
//    id_section
//    attr section
//    data section
//    feature section
//
//  The feature section has the following structure:
//    a section descriptor array, each element contains the section information of one add_feature.
//    data section of feature 1
//    data section of feature 2
//    ....

namespace PerfFileFormat {

enum {
  FEAT_RESERVED = 0,
  FEAT_FIRST_FEATURE = 1,
  FEAT_TRACING_DATA = 1,
  FEAT_BUILD_ID,
  FEAT_HOSTNAME,
  FEAT_OSRELEASE,
  FEAT_VERSION,
  FEAT_ARCH,
  FEAT_NRCPUS,
  FEAT_CPUDESC,
  FEAT_CPUID,
  FEAT_TOTAL_MEM,
  FEAT_CMDLINE,
  FEAT_EVENT_DESC,
  FEAT_CPU_TOPOLOGY,
  FEAT_NUMA_TOPOLOGY,
  FEAT_BRANCH_STACK,
  FEAT_PMU_MAPPINGS,
  FEAT_GROUP_DESC,
  FEAT_LAST_FEATURE,
  FEAT_MAX_NUM = 256,
};

struct SectionDesc {
  uint64_t offset;
  uint64_t size;
};

static const char* PERF_MAGIC = "PERFILE2";

struct FileHeader {
  char magic[8];
  uint64_t header_size;
  uint64_t attr_size;
  SectionDesc attrs;
  SectionDesc data;
  SectionDesc event_types;
  unsigned char features[FEAT_MAX_NUM / 8];
};

struct FileAttr {
  perf_event_attr attr;
  SectionDesc ids;
};

}  // namespace PerfFileFormat

#endif  // SIMPLE_PERF_RECORD_FILE_FORMAT_H_
