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

#include "service/ipc/ipc_handler_linux.h"

// TODO(keybuk): This is a crappy workaround to link IPCHandlerLinux into
// host-native unit tests. IPCManager shouldn't explicitly reference these
// classes.

namespace ipc {

IPCHandlerLinux::IPCHandlerLinux(
    bluetooth::Adapter* adapter,
    IPCManager::Delegate* delegate)
    : IPCHandler(adapter, delegate),
      running_(false),
      thread_("IPCHandlerLinux"),
      keep_running_(true) {
  // Stub
}

IPCHandlerLinux::~IPCHandlerLinux() {
  // Stub
}

bool IPCHandlerLinux::Run() {
  // Stub
  return false;
}

void IPCHandlerLinux::Stop() {
  // Stub
}

void IPCHandlerLinux::StartListeningOnThread() {
  // Stub
}

void IPCHandlerLinux::ShutDownOnOriginThread() {
  // Stub
}

void IPCHandlerLinux::NotifyStartedOnOriginThread() {
  // Stub
}

void IPCHandlerLinux::NotifyStartedOnCurrentThread() {
  // Stub
}

void IPCHandlerLinux::NotifyStoppedOnOriginThread() {
  // Stub
}

void IPCHandlerLinux::NotifyStoppedOnCurrentThread() {
  // Stub
}

}  // namespace
