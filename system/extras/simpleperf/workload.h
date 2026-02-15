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

#ifndef SIMPLE_PERF_WORKLOAD_H_
#define SIMPLE_PERF_WORKLOAD_H_

#include <sys/types.h>
#include <chrono>
#include <string>
#include <vector>

#include <base/macros.h>

class Workload {
 private:
  enum WorkState {
    NotYetCreateNewProcess,
    NotYetStartNewProcess,
    Started,
    Finished,
  };

 public:
  static std::unique_ptr<Workload> CreateWorkload(const std::vector<std::string>& args);

  ~Workload() {
    if (start_signal_fd_ != -1) {
      close(start_signal_fd_);
    }
    if (exec_child_fd_ != -1) {
      close(exec_child_fd_);
    }
  }

  bool Start();
  bool IsFinished();
  void WaitFinish();
  pid_t GetPid() {
    return work_pid_;
  }

 private:
  Workload(const std::vector<std::string>& args)
      : work_state_(NotYetCreateNewProcess),
        args_(args),
        work_pid_(-1),
        start_signal_fd_(-1),
        exec_child_fd_(-1) {
  }

  bool CreateNewProcess();
  void WaitChildProcess(bool no_hang);

  WorkState work_state_;
  std::vector<std::string> args_;
  pid_t work_pid_;
  int start_signal_fd_;  // The parent process writes 1 to start workload in the child process.
  int exec_child_fd_;    // The child process writes 1 to notify that execvp() failed.

  DISALLOW_COPY_AND_ASSIGN(Workload);
};

#endif  // SIMPLE_PERF_WORKLOAD_H_
