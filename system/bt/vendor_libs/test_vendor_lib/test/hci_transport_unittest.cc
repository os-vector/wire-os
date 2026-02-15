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

#include "vendor_libs/test_vendor_lib/include/command_packet.h"
#include "vendor_libs/test_vendor_lib/include/hci_transport.h"

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "base/threading/thread.h"

#include <gtest/gtest.h>
#include <functional>
#include <mutex>

extern "C" {
#include "stack/include/hcidefs.h"
}  // extern "C"

namespace {
const std::vector<uint8_t> stub_command({DATA_TYPE_COMMAND,
                                         static_cast<uint8_t>(HCI_RESET),
                                         static_cast<uint8_t>(HCI_RESET >> 8),
                                         0});

const int kMultiIterations = 10000;

void WriteStubCommand(int fd) {
  write(fd, &stub_command[0], stub_command.size());
}

}  // namespace

namespace test_vendor_lib {

class HciTransportTest : public ::testing::Test {
 public:
  HciTransportTest()
      : command_callback_count_(0),
        thread_("HciTransportTest"),
        weak_ptr_factory_(this) {
    SetUpTransport();
    StartThread();
    PostStartWatchingOnThread();
  }

  ~HciTransportTest() {
    transport_.CloseHciFd();
  }

  void CommandCallback(std::unique_ptr<CommandPacket> command) {
    ++command_callback_count_;
    // Ensure that the received packet matches the stub command.
    EXPECT_EQ(DATA_TYPE_COMMAND, command->GetType());
    EXPECT_EQ(HCI_RESET, command->GetOpcode());
    EXPECT_EQ(0, command->GetPayloadSize());
    transport_.CloseVendorFd();
  }

  void MultiCommandCallback(std::unique_ptr<CommandPacket> command) {
    ++command_callback_count_;
    // Ensure that the received packet matches the stub command.
    EXPECT_EQ(DATA_TYPE_COMMAND, command->GetType());
    EXPECT_EQ(HCI_RESET, command->GetOpcode());
    EXPECT_EQ(0, command->GetPayloadSize());
    if (command_callback_count_ == kMultiIterations)
      transport_.CloseVendorFd();
  }

 protected:
  // Tracks the number of commands received.
  int command_callback_count_;
  base::Thread thread_;
  HciTransport transport_;
  base::MessageLoopForIO::FileDescriptorWatcher watcher_;
  base::WeakPtrFactory<HciTransportTest> weak_ptr_factory_;

 private:
  // Workaround because ASSERT cannot be used directly in a constructor
  void SetUpTransport() {
    ASSERT_TRUE(transport_.SetUp());
  }

  void StartThread() {
    ASSERT_TRUE(thread_.StartWithOptions(
        base::Thread::Options(base::MessageLoop::TYPE_IO, 0)));
  }

  void PostStartWatchingOnThread() {
    thread_.task_runner()->PostTask(
        FROM_HERE, base::Bind(&HciTransportTest::StartWatchingOnThread,
                              weak_ptr_factory_.GetWeakPtr()));
  }

  void StartWatchingOnThread() {
    base::MessageLoopForIO* loop =
        static_cast<base::MessageLoopForIO*>(thread_.message_loop());
    ASSERT_TRUE(loop);
    ASSERT_TRUE(loop->WatchFileDescriptor(
        transport_.GetVendorFd(), true,
        base::MessageLoopForIO::WATCH_READ_WRITE, &watcher_, &transport_));
  }
};

TEST_F(HciTransportTest, SingleCommandCallback) {
  transport_.RegisterCommandHandler(std::bind(
      &HciTransportTest::CommandCallback, this, std::placeholders::_1));
  EXPECT_EQ(0, command_callback_count_);
  WriteStubCommand(transport_.GetHciFd());
  thread_.Stop();  // Wait for the command handler to finish.
  EXPECT_EQ(1, command_callback_count_);
}

TEST_F(HciTransportTest, MultiCommandCallback) {
  transport_.RegisterCommandHandler(std::bind(
      &HciTransportTest::MultiCommandCallback, this, std::placeholders::_1));
  EXPECT_EQ(0, command_callback_count_);
  WriteStubCommand(transport_.GetHciFd());
  for (int i = 1; i < kMultiIterations; ++i)
    WriteStubCommand(transport_.GetHciFd());
  thread_.Stop();  // Wait for the command handler to finish.
  EXPECT_EQ(kMultiIterations, command_callback_count_);
}

// TODO(dennischeng): Add tests for PostEventResponse and
// PostDelayedEventResponse.

}  // namespace test_vendor_lib
