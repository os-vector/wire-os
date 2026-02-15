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

#ifndef SIMPLE_PERF_ENVIRONMENT_H_
#define SIMPLE_PERF_ENVIRONMENT_H_

#include <functional>
#include <string>
#include <vector>
#include "build_id.h"

std::vector<int> GetOnlineCpus();

static const char* DEFAULT_KERNEL_MMAP_NAME = "[kernel.kallsyms]_text";

struct KernelMmap {
  std::string name;
  uint64_t start_addr;
  uint64_t len;
  uint64_t pgoff;
};

struct ModuleMmap {
  std::string name;
  uint64_t start_addr;
  uint64_t len;
  std::string filepath;
};

bool GetKernelAndModuleMmaps(KernelMmap* kernel_mmap, std::vector<ModuleMmap>* module_mmaps);

struct ThreadComm {
  pid_t tgid, tid;
  std::string comm;
  bool is_process;
};

bool GetThreadComms(std::vector<ThreadComm>* thread_comms);

static const char* DEFAULT_EXECNAME_FOR_THREAD_MMAP = "//anon";

struct ThreadMmap {
  uint64_t start_addr;
  uint64_t len;
  uint64_t pgoff;
  std::string name;
  bool executable;
};

bool GetThreadMmapsInProcess(pid_t pid, std::vector<ThreadMmap>* thread_mmaps);

static const char* DEFAULT_KERNEL_FILENAME_FOR_BUILD_ID = "[kernel.kallsyms]";

bool GetKernelBuildId(BuildId* build_id);
bool GetModuleBuildId(const std::string& module_name, BuildId* build_id);

// Expose the following functions for unit tests.
std::vector<int> GetOnlineCpusFromString(const std::string& s);

struct KernelSymbol {
  uint64_t addr;
  char type;
  const char* name;
  const char* module;  // If nullptr, the symbol is not in a kernel module.
};

bool ProcessKernelSymbols(const std::string& symbol_file,
                          std::function<bool(const KernelSymbol&)> callback);

#endif  // SIMPLE_PERF_ENVIRONMENT_H_
