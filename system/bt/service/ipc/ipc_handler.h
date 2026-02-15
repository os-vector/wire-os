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
#include <base/memory/ref_counted.h>

#include "service/ipc/ipc_manager.h"

namespace bluetooth {
class Adapter;
}  // namespace bluetooth

namespace ipc {

// IPCHandler is an interface that classes implementing different IPC mechanisms
// must conform to.
class IPCHandler : public base::RefCountedThreadSafe<IPCHandler> {
 public:
  IPCHandler(bluetooth::Adapter* adapter, IPCManager::Delegate* delegate);
  virtual ~IPCHandler();

  // Initializes and runs the IPC mechanism. Returns true on success, false
  // otherwise.
  virtual bool Run() = 0;

  // Stops the IPC mechanism.
  virtual void Stop() = 0;

 protected:
  // Getters for private members to allow subclasses to access them in read-only
  // fashion.
  bluetooth::Adapter* adapter() const { return adapter_; }
  IPCManager::Delegate* delegate() const { return delegate_; }

 private:
  IPCHandler() = default;

  // Weak reference to the global Adapter instance.
  bluetooth::Adapter* adapter_;

  // The delegate that is interested in notifications from us.
  IPCManager::Delegate* delegate_;

  DISALLOW_COPY_AND_ASSIGN(IPCHandler);
};

}  // namespace ipc
