//
//  Copyright (C) 2015 Google, Inc.
//
//  Licensed under the Apache License, Version 2.0 (the "License");
//  you may not use this file except in compliance with the License.
//  You may obtain a copy of the License at:
//
//  http://www.apache.org/licenses/LICENSE-2.0
//
//  Unless required by applicable law or agreed to in writing, software
//  distributed under the License is distributed on an "AS IS" BASIS,
//  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//  See the License for the specific language governing permissions and
//  limitations under the License.
//

#pragma once

#include <atomic>

#include <base/files/file_path.h>
#include <base/files/scoped_file.h>
#include <base/macros.h>
#include <base/threading/thread.h>

#include "service/ipc/ipc_handler.h"
#include "service/ipc/ipc_manager.h"

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace ipc {

// Implements a Linux sequential packet domain-socket based IPCHandler
class IPCHandlerLinux : public IPCHandler {
 public:
  IPCHandlerLinux(bluetooth::Adapter* adapter,
                 IPCManager::Delegate* delegate);
  ~IPCHandlerLinux() override;

  // IPCHandler overrides:
  bool Run() override;
  void Stop() override;

 private:
  IPCHandlerLinux() = default;

  // Starts listening for incoming connections. Posted on |thread_| by Run().
  void StartListeningOnThread();

  // Stops the IPC thread. This helper is needed since base::Thread requires
  // threads to be stopped on the thread that started them.
  void ShutDownOnOriginThread();

  // Notifies the delegate that we started or stoppedlistening for incoming
  // connections.
  void NotifyStartedOnOriginThread();
  void NotifyStartedOnCurrentThread();
  void NotifyStoppedOnOriginThread();
  void NotifyStoppedOnCurrentThread();

  // True, if the IPC mechanism is running.
  bool running_;

  // The server socket on which we listen to incoming connections.
  base::ScopedFD socket_;

  // The file path to |socket_|. This is only set if we create and manage the
  // life time of the socket.
  base::FilePath socket_path_;

  // We use a dedicated thread for listening to incoming connections and
  // polling from the socket to avoid blocking the main thread.
  base::Thread thread_;

  // Whether or not the listening thread should continue to run.
  std::atomic<bool> keep_running_;

  // The origin thread's task runner.
  scoped_refptr<base::SingleThreadTaskRunner> origin_task_runner_;

  DISALLOW_COPY_AND_ASSIGN(IPCHandlerLinux);
};

}  // namespace ipc
