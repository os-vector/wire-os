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

#include <base/bind.h>
#include <base/logging.h>
#include <base/message_loop/message_loop.h>
#include <binder/IPCThreadState.h>
#include <binder/IServiceManager.h>
#include <binder/ProcessState.h>

#include "service/ipc/binder/bluetooth_binder_server.h"

using android::defaultServiceManager;
using android::sp;
using android::status_t;
using android::String16;

namespace ipc {

IPCHandlerBinder::IPCHandlerBinder(
    bluetooth::Adapter* adapter,
    IPCManager::Delegate* delegate)
    : IPCHandler(adapter, delegate) {
}

IPCHandlerBinder::~IPCHandlerBinder() {
}

bool IPCHandlerBinder::Run() {
  CHECK(adapter());

  // Register the IBluetooth service with the Android ServiceManager.
  android::sp<binder::BluetoothBinderServer> bt_server =
      new binder::BluetoothBinderServer(adapter());
  status_t status = defaultServiceManager()->addService(
      String16(binder::IBluetooth::kServiceName),
      bt_server);
  if (status != android::NO_ERROR) {
    LOG(ERROR) << "Failed to register Bluetooth service with ServiceManager";
    return false;
  }

  // Notify the delegate. We do this in the message loop to avoid reentrancy.
  if (delegate()) {
    base::MessageLoop::current()->task_runner()->PostTask(
        FROM_HERE,
        base::Bind(&IPCHandlerBinder::NotifyStarted, this));
  }

  android::ProcessState::self()->startThreadPool();

  return true;
}

void IPCHandlerBinder::Stop() {
  // TODO(armansito): There are several methods in android::IPCThreadState that
  // are related to shutting down the threadpool, however I haven't been able to
  // make them shut things down cleanly. Figure out the right way to stop the
  // Binder IPC here.
}

void IPCHandlerBinder::NotifyStarted() {
  if (delegate())
    delegate()->OnIPCHandlerStarted(IPCManager::TYPE_BINDER);
}

}  // namespace ipc
