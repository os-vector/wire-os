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

#define LOG_TAG "test_channel_transport"

#include "vendor_libs/test_vendor_lib/include/test_channel_transport.h"

#include "base/logging.h"

extern "C" {
#include "osi/include/log.h"

#include <sys/socket.h>
#include <netinet/in.h>
}  // extern "C"

namespace test_vendor_lib {

TestChannelTransport::TestChannelTransport(bool enabled, int port)
    : enabled_(enabled), port_(port) {}

bool TestChannelTransport::SetUp() {
  CHECK(enabled_);

  struct sockaddr_in listen_address, test_channel_address;
  int sockaddr_in_size = sizeof(struct sockaddr_in);
  int listen_fd = -1;
  int accept_fd = -1;
  memset(&listen_address, 0, sockaddr_in_size);
  memset(&test_channel_address, 0, sockaddr_in_size);

  if ((listen_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    LOG_INFO(LOG_TAG, "Error creating socket for test channel.");
    return false;
  }

  LOG_INFO(LOG_TAG, "port: %d", port_);
  listen_address.sin_family = AF_INET;
  listen_address.sin_port = htons(port_);
  listen_address.sin_addr.s_addr = htonl(INADDR_ANY);

  if (bind(listen_fd, reinterpret_cast<sockaddr*>(&listen_address),
           sockaddr_in_size) < 0) {
    LOG_INFO(LOG_TAG, "Error binding test channel listener socket to address.");
    close(listen_fd);
    return false;
  }

  if (listen(listen_fd, 1) < 0) {
    LOG_INFO(LOG_TAG, "Error listening for test channel.");
    close(listen_fd);
    return false;
  }

  if ((accept_fd =
           accept(listen_fd, reinterpret_cast<sockaddr*>(&test_channel_address),
                  &sockaddr_in_size)) < 0) {
    LOG_INFO(LOG_TAG, "Error accepting test channel connection.");
    close(listen_fd);
    return false;
  }

  fd_.reset(new base::ScopedFD(accept_fd));
  return GetFd() >= 0;
}

int TestChannelTransport::GetFd() {
  return fd_->get();
}

bool TestChannelTransport::IsEnabled() {
  return enabled_;
}

// base::MessageLoopForIO::Watcher overrides:
void TestChannelTransport::OnFileCanReadWithoutBlocking(int fd) {
  CHECK(fd == GetFd());

  LOG_INFO(LOG_TAG, "Event ready in TestChannelTransport on fd: %d", fd);
  uint8_t command_name_size = 0;
  read(fd, &command_name_size, 1);
  std::vector<uint8_t> command_name_raw;
  command_name_raw.resize(command_name_size);
  read(fd, &command_name_raw[0], command_name_size);
  std::string command_name(command_name_raw.begin(), command_name_raw.end());
  LOG_INFO(LOG_TAG, "Received command from test channel: %s",
           command_name.data());

  if (command_name == "CLOSE_TEST_CHANNEL") {
    fd_.reset(nullptr);
    return;
  }

  uint8_t num_args = 0;
  read(fd, &num_args, 1);
  LOG_INFO(LOG_TAG, "num_args: %d", num_args);
  std::vector<std::string> args;
  for (uint8_t i = 0; i < num_args; ++i) {
    uint8_t arg_size = 0;
    read(fd, &arg_size, 1);
    std::vector<uint8_t> arg;
    arg.resize(arg_size);
    read(fd, &arg[0], arg_size);
    args.push_back(std::string(arg.begin(), arg.end()));
  }

  for (size_t i = 0; i < args.size(); ++i)
    LOG_INFO(LOG_TAG, "Command argument %zu: %s", i, args[i].data());

  command_handler_(command_name, args);
}

void TestChannelTransport::OnFileCanWriteWithoutBlocking(int fd) {}

void TestChannelTransport::RegisterCommandHandler(
    std::function<void(const std::string&, const std::vector<std::string>&)>
        callback) {
  command_handler_ = callback;
}

void TestChannelTransport::Disable() {
  enabled_ = false;
}

}  // namespace test_vendor_lib {
