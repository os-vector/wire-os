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

#ifdef BT_LIBCHROME_NDEBUG
#define NDEBUG 1
#endif

#include <base/at_exit.h>
#include <base/bind.h>
#include <base/command_line.h>
#include <base/location.h>
#include <base/logging.h>
#include <base/message_loop/message_loop.h>
#include <base/run_loop.h>

#include <binder/IPCThreadState.h>
#include <binder/ProcessState.h>

#include <bluetooth/binder/IBluetooth.h>

#include "service/example/heart_rate/heart_rate_server.h"

using android::sp;
using ipc::binder::IBluetooth;

namespace {

void QuitMessageLoop() {
  // I don't know why both of these calls are necessary but the message loop
  // doesn't stop unless I call both. Bug in base::MessageLoop?
  base::RunLoop().Quit();
  base::MessageLoop::current()->QuitNow();
}

// Handles the case where the Bluetooth process dies.
class BluetoothDeathRecipient : public android::IBinder::DeathRecipient {
 public:
  BluetoothDeathRecipient(
      scoped_refptr<base::SingleThreadTaskRunner> main_task_runner)
      : main_task_runner_(main_task_runner) {
  }

  ~BluetoothDeathRecipient() override = default;

  // android::IBinder::DeathRecipient override:
  void binderDied(const android::wp<android::IBinder>& /* who */) override {
    LOG(ERROR) << "The Bluetooth daemon has died. Aborting.";

    // binderDied executes on a dedicated thread. We need to stop the main loop
    // on the main thread so we post a message to it here. The main loop only
    // runs on the main thread.
    main_task_runner_->PostTask(FROM_HERE, base::Bind(&QuitMessageLoop));

    android::IPCThreadState::self()->stopProcess();
  }

 private:
  scoped_refptr<base::SingleThreadTaskRunner> main_task_runner_;
};

}  // namespace

int main(int argc, char* argv[]) {
  base::AtExitManager exit_manager;
  base::CommandLine::Init(argc, argv);
  logging::LoggingSettings log_settings;

  // Initialize global logging based on command-line parameters (this is a
  // libchrome pattern).
  if (!logging::InitLogging(log_settings)) {
    LOG(ERROR) << "Failed to set up logging";
    return EXIT_FAILURE;
  }

  // Set up a message loop so that we can schedule timed Heart Rate
  // notifications.
  base::MessageLoop main_loop;

  LOG(INFO) << "Starting GATT Heart Rate Service sample";

  // Obtain the IBluetooth binder from the service manager service.
  sp<IBluetooth> bluetooth = IBluetooth::getClientInterface();
  if (!bluetooth.get()) {
    LOG(ERROR) << "Failed to obtain a handle on IBluetooth";
    return EXIT_FAILURE;
  }

  // Bluetooth needs to be enabled for our demo to work.
  if (!bluetooth->IsEnabled()) {
    LOG(ERROR) << "Bluetooth is not enabled.";
    return EXIT_FAILURE;
  }

  // Register for death notifications on the IBluetooth binder. This let's us
  // handle the case where the Bluetooth daemon process (bluetoothtbd) dies
  // outside of our control.
  sp<BluetoothDeathRecipient> dr(
      new BluetoothDeathRecipient(main_loop.task_runner()));
  if (android::IInterface::asBinder(bluetooth.get())->linkToDeath(dr) !=
      android::NO_ERROR) {
    LOG(ERROR) << "Failed to register DeathRecipient for IBluetooth";
    return EXIT_FAILURE;
  }

  // Initialize the Binder process thread pool. We have to set this up,
  // otherwise, incoming callbacks from the Bluetooth daemon would block the
  // main thread (in other words, we have to do this as we are a "Binder
  // server").
  android::ProcessState::self()->startThreadPool();

  // heart_rate::HeartRateServer notifies success or failure asynchronously
  // using a closure, so we set up a lambda for that here.
  auto callback = [&](bool success) {
    if (success) {
      LOG(INFO) << "Heart Rate service started successfully";
      return;
    }

    LOG(ERROR) << "Starting Heart Rate server failed asynchronously";
    main_loop.QuitWhenIdle();
  };

  bool advertise = base::CommandLine::ForCurrentProcess()->HasSwitch("advertise");

  // Create the Heart Rate server.
  std::unique_ptr<heart_rate::HeartRateServer> hr(
      new heart_rate::HeartRateServer(bluetooth, main_loop.task_runner(), advertise));
  if (!hr->Run(callback)) {
    LOG(ERROR) << "Failed to start Heart Rate server";
    return EXIT_FAILURE;
  }

  // Run the main loop on the main process thread. Binder callbacks will be
  // received in dedicated threads set up by the ProcessState::startThreadPool
  // call above but we use this main loop for sending out heart rate
  // notifications.
  main_loop.Run();

  LOG(INFO) << "Exiting";
  return EXIT_SUCCESS;
}
