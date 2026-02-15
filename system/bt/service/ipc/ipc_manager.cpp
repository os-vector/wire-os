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

#include "service/ipc/ipc_manager.h"

#include "service/ipc/binder/ipc_handler_binder.h"
#include "service/ipc/ipc_handler_linux.h"

namespace ipc {

IPCManager::IPCManager(bluetooth::Adapter* adapter)
    : adapter_(adapter) {
  CHECK(adapter_);
}

IPCManager::~IPCManager() {
  // Don't rely on the handlers getting destroyed since another thread might be
  // holding a reference to them. Instead, explicitly stop them here.
  if (BinderStarted())
    binder_handler_->Stop();
  if (LinuxStarted())
    linux_handler_->Stop();
}

bool IPCManager::Start(Type type, Delegate* delegate) {
  switch (type) {
  case TYPE_LINUX:
    if (LinuxStarted()) {
      LOG(ERROR) << "IPCManagerLinux already started.";
      return false;
    }

    linux_handler_ = new IPCHandlerLinux(adapter_, delegate);
    if (!linux_handler_->Run()) {
      linux_handler_ = nullptr;
      return false;
    }
    return true;

  case TYPE_BINDER:
    if (BinderStarted()) {
      LOG(ERROR) << "IPCManagerBinder already started.";
      return false;
    }

    binder_handler_ = new IPCHandlerBinder(adapter_, delegate);
    if (!binder_handler_->Run()) {
      binder_handler_ = nullptr;
      return false;
    }
    return true;

  default:
    LOG(ERROR) << "Unsupported IPC type given: " << type;
  }

  return false;
}

bool IPCManager::BinderStarted() const {
  return binder_handler_.get();
}

bool IPCManager::LinuxStarted() const {
  return linux_handler_.get();
}

}  // namespace ipc
