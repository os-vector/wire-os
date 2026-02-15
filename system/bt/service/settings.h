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

#include <base/files/file_path.h>
#include <base/macros.h>

namespace bluetooth {

// The Settings class stores global runtime configurations, such as IPC domain
// namespace, configuration file locations, and other system properties and
// flags.
class Settings {
 public:
  // Constant for the "--help" command-line switches.
  static const char kHelp[];

  Settings();
  ~Settings();

  // TODO(armansito): Write an instance method for storing things into a file.

  // Initializes the Settings object. This reads the command-line options for
  // the current process (which must have been initialized using
  // base::CommandLine) and sets up the initial global settings. Returns false
  // if there is an error, e.g. if the parameters/switches are malformed.
  bool Init();

  // If Android init created a server socket for the daemon,
  // we can retrieve it through this suffix.
  const std::string& android_ipc_socket_suffix() const {
    return android_ipc_socket_suffix_;
  }

  // Path to create a Unix domain socket server for Bluetooth IPC.
  const base::FilePath& create_ipc_socket_path() const {
    return create_ipc_socket_path_;
  }

  // Returns true if domain-socket based IPC should be used. If false, then
  // Binder IPC must be used.
  inline bool UseSocketIPC() const {
    return !android_ipc_socket_suffix().empty() ||
        !create_ipc_socket_path().empty();
  }

 private:
  bool initialized_;
  std::string android_ipc_socket_suffix_;
  base::FilePath create_ipc_socket_path_;

  DISALLOW_COPY_AND_ASSIGN(Settings);
};

}  // namespace bluetooth
