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

#include "service/ipc/binder/ipc_handler_binder.h"

// TODO(armansito): This is a crappy workaround to link IPCHandlerBinder into
// host-native unit tests. We should instead figure out a way to build Binder
// unit tests for host.

namespace ipc {

IPCHandlerBinder::IPCHandlerBinder(
    bluetooth::Adapter* adapter,
    IPCManager::Delegate* delegate)
    : IPCHandler(adapter, delegate) {
  // Stub
}

IPCHandlerBinder::~IPCHandlerBinder() {
  // Stub
}

bool IPCHandlerBinder::Run() {
  // Stub
  return false;
}

void IPCHandlerBinder::Stop() {
  // Stub
}

void IPCHandlerBinder::NotifyStarted() {
  // Stub
}

}  // namespace
