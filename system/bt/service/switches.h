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

#include <string>

namespace bluetooth {
namespace switches {

// List of command-line switches used by the daemon.
const char kHelpLong[] = "help";
const char kHelpShort[] = "h";
const char kAndroidIPCSocketSuffix[] = "android-ipc-socket-suffix";
const char kCreateIPCSocketPath[] = "create-ipc-socket";

const char kHelpMessage[] =
    "\nBluetooth System Service\n"
    "\n"
    "Usage:\n"
    "\t--help,-h\t\t\tShow this help message\n"
    "\t--android-ipc-socket-suffix\tSuffix of socket created by Android init. "
    "Mutually exclusive with --create-ipc-socket.\n"
    "\t--create-ipc-socket\t\tSocket path created for Unix domain socket based "
    "IPC. Mutually exclusive with --android-ipc-socket-suffix.\n"
    "\t--v\t\t\t\tLog verbosity level (e.g. -v=1)\n";

}  // namespace switches
}  // namespace bluetooth
