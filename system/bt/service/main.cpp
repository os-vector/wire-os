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
#include <base/command_line.h>
#include <base/files/scoped_file.h>
#include <base/logging.h>

// For system properties
// TODO(icoolidge): abstraction or non-cutils stub.
#if !defined(OS_GENERIC)
#include <cutils/properties.h>
#endif  // !defined(OS_GENERIC)

#include "service/daemon.h"
#include "service/switches.h"

namespace {

// TODO(armansito): None of these should be hardcoded here. Instead, pass these
// via commandline.
const char kDisableProperty[] = "persist.bluetooth.disable";

}  // namespace

int main(int argc, char *argv[]) {
  base::AtExitManager exit_manager;
  base::CommandLine::Init(argc, argv);

  logging::LoggingSettings log_settings;
  if (!logging::InitLogging(log_settings)) {
    LOG(ERROR) << "Failed to set up logging";
    return EXIT_FAILURE;
  }

  // TODO(armansito): Initialize base/logging. By default it will dump to stdout
  // but we might want to change that based on a command-line switch. Figure out
  // how to route the logging to Android's syslog. Once that's done, we won't
  // need to use osi/include/log.h anymore.

  // TODO(armansito): Register exit-time clean-up handlers for the IPC sockets.
  // Register signal handlers.
  auto command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(bluetooth::switches::kHelpLong) ||
      command_line->HasSwitch(bluetooth::switches::kHelpShort)) {
    LOG(INFO) << bluetooth::switches::kHelpMessage;
    return EXIT_SUCCESS;
  }

#if !defined(OS_GENERIC)
  // TODO(armansito): Remove Chromecast specific property out of here. This
  // should just be obtained from global config.
  char disable_value[PROPERTY_VALUE_MAX];
#ifdef ANDROID
  int status = property_get(kDisableProperty, disable_value, nullptr);
#else
  int status = property_get_bt(kDisableProperty, disable_value, nullptr);
#endif
  if (status && !strcmp(disable_value, "1")) {
    LOG(INFO) << "service disabled";
    return EXIT_SUCCESS;
  }
#endif  // !defined(OS_GENERIC)

  if (!bluetooth::Daemon::Initialize()) {
    LOG(ERROR) << "Failed to initialize Daemon";
    return EXIT_FAILURE;
  }

  // Start the main event loop.
  bluetooth::Daemon::Get()->StartMainLoop();

  // The main message loop has exited; clean up the Daemon.
  bluetooth::Daemon::Get()->ShutDown();

  return EXIT_SUCCESS;
}
