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

#include <base/macros.h>

#include "service/ipc/ipc_handler.h"
#include "service/ipc/ipc_manager.h"

namespace ipc {

// Implements a Binder based IPCHandler.
class IPCHandlerBinder : public IPCHandler {
 public:
  IPCHandlerBinder(bluetooth::Adapter* adapter,
                   IPCManager::Delegate* delegate);
  ~IPCHandlerBinder() override;

  // IPCHandler overrides:
  bool Run() override;
  void Stop() override;

 private:
  IPCHandlerBinder() = default;

  // Notify the delegate that IPC has started.
  void NotifyStarted();

  DISALLOW_COPY_AND_ASSIGN(IPCHandlerBinder);
};

}  // namespace ipc
