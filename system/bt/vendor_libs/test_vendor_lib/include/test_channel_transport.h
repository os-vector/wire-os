//
// Copyright 2015 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#pragma once

#include <string>
#include <memory>

#include "base/files/scoped_file.h"
#include "base/message_loop/message_loop.h"

namespace test_vendor_lib {

// Manages communications between test channel and the controller. Mirrors the
// HciTransport for the test channel.
class TestChannelTransport : public base::MessageLoopForIO::Watcher {
 public:
  TestChannelTransport(bool enabled, int port);

  ~TestChannelTransport() = default;

  // Waits for a connection request from the test channel program and
  // allocates the file descriptor to watch for run-time parameters at. This
  // file descriptor gets stored in |fd_|.
  bool SetUp();

  int GetFd();

  // Because it imposes a different flow of work, the test channel must be
  // actively enabled to be used. |enabled_| is set by the vendor manager.
  bool IsEnabled();

  // Turns the test channel off for use in circumstances where an error occurs
  // and leaving the channel on would crash Bluetooth (e.g. if the test channel
  // is unable to bind to its socket, Bluetooth should still start without the
  // channel enabled).
  void Disable();

  // Sets the callback that fires when data is read in
  // |OnFileCanReadWithoutBlocking|.
  void RegisterCommandHandler(
      std::function<void(const std::string&, const std::vector<std::string>&)>
          callback);

 private:
  // base::MessageLoopForIO::Watcher overrides:
  void OnFileCanReadWithoutBlocking(int fd) override;

  void OnFileCanWriteWithoutBlocking(int fd) override;

  std::function<void(const std::string&, const std::vector<std::string>&)>
      command_handler_;

  // File descriptor to watch for test hook data.
  std::unique_ptr<base::ScopedFD> fd_;

  // TODO(dennischeng): Get port and enabled flag from a config file.
  int port_;
  bool enabled_;

  DISALLOW_COPY_AND_ASSIGN(TestChannelTransport);
};

}  // namespace test_vendor_lib
