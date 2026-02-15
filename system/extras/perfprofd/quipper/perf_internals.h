/*
**
** Copyright 2015, The Android Open Source Project
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
*/

#ifndef PERF_INTERNALS_H
#define PERF_INTERNALS_H

#include <linux/perf_event.h>
#include "kernel-headers/tools/perf/util/types.h"
#include "kernel-headers/tools/perf/util/include/linux/bitops.h"
#include "kernel-headers/tools/perf/util/include/linux/types.h"
#include "kernel-headers/tools/perf/util/build-id.h"
#include "kernel-headers/tools/perf/util/include/linux/kernel/kernel.h"
#include "kernel-headers/tools/perf/util/header.h"
#include "kernel-headers/tools/perf/util/event.h"
#include "kernel-headers/tools/perf/util/target.h"
#include "kernel-headers/tools/perf/perf.h"

// The first 64 bits of the perf header, used as a perf data file ID tag.
const uint64_t kPerfMagic = 0x32454c4946524550LL;  // "PERFILE2" little-endian

#undef max
#undef min

//
// Wrapper class to manage creation/deletion of storage associated
// with perf_sample structs.
//
class PerfSampleCustodian {
 public:
  explicit PerfSampleCustodian(struct perf_sample& sample)
      : sample_(sample) {
    sample.raw_data = NULL;
    sample.callchain = NULL;
    sample.branch_stack = NULL;
  }
  ~PerfSampleCustodian() {
    if (sample_.callchain)
      delete [] sample_.callchain;
    if (sample_.branch_stack)
          delete [] sample_.branch_stack;
    if (sample_.branch_stack)
      delete [] reinterpret_cast<char*>(sample_.raw_data);
  }
 private:
  struct perf_sample& sample_;
};

typedef perf_event event_t;

#endif
