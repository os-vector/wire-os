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

#include <memory>

#include <sys/socket.h>
#include <sys/un.h>

#include <base/at_exit.h>
#include <base/command_line.h>
#include <base/files/scoped_file.h>
#include <base/macros.h>
#include <base/strings/stringprintf.h>
#include <gtest/gtest.h>

#include "service/adapter.h"
#include "service/hal/fake_bluetooth_gatt_interface.h"
#include "service/hal/fake_bluetooth_interface.h"
#include "service/ipc/ipc_manager.h"
#include "service/settings.h"
#include "service/test/mock_daemon.h"


namespace {

using testing::Return;

const char kTestSocketPath[] = "test_socket_path";

class IPCLinuxTest : public ::testing::Test {
 public:
  IPCLinuxTest() = default;
  ~IPCLinuxTest() override = default;

  void SetUp() override {
    SetUpCommandLine();
    ASSERT_TRUE(settings_.Init());

    auto mock_daemon = new bluetooth::testing::MockDaemon();

    ON_CALL(*mock_daemon, GetSettings()).WillByDefault(Return(&settings_));
    ON_CALL(*mock_daemon, GetMessageLoop())
        .WillByDefault(Return(&message_loop_));

    bluetooth::Daemon::InitializeForTesting(mock_daemon);
    bluetooth::hal::BluetoothInterface::InitializeForTesting(
        new bluetooth::hal::FakeBluetoothInterface());
    bluetooth::hal::BluetoothGattInterface::InitializeForTesting(
        new bluetooth::hal::FakeBluetoothGattInterface(nullptr, nullptr));

    adapter_ = bluetooth::Adapter::Create();
    ipc_manager_.reset(new ipc::IPCManager(adapter_.get()));
  }

  void TearDown() override {
    client_fd_.reset();
    ipc_manager_.reset();
    adapter_.reset();
    bluetooth::hal::BluetoothGattInterface::CleanUp();
    bluetooth::hal::BluetoothInterface::CleanUp();
    bluetooth::Daemon::ShutDown();
    base::CommandLine::Reset();
  }

  virtual void SetUpCommandLine() {
    std::string ipc_socket_arg =
        base::StringPrintf("--create-ipc-socket=%s", kTestSocketPath);
    const base::CommandLine::CharType* argv[] = {
      "program", ipc_socket_arg.c_str(),
    };
    base::CommandLine::Init(arraysize(argv), argv);
  }

  void ConnectToTestSocket() {
    client_fd_.reset(socket(PF_UNIX, SOCK_SEQPACKET, 0));
    ASSERT_TRUE(client_fd_.is_valid());

    struct sockaddr_un address;
    memset(&address, 0, sizeof(address));
    address.sun_family = AF_UNIX;
    strncpy(address.sun_path, kTestSocketPath, sizeof(address.sun_path) - 1);

    int status = connect(client_fd_.get(), (struct sockaddr *)&address,
                         sizeof(address));
    EXPECT_EQ(0, status);
  }

 protected:
  base::AtExitManager exit_manager_;
  base::MessageLoop message_loop_;
  bluetooth::Settings settings_;

  std::unique_ptr<bluetooth::Adapter> adapter_;
  std::unique_ptr<ipc::IPCManager> ipc_manager_;
  base::ScopedFD client_fd_;

  DISALLOW_COPY_AND_ASSIGN(IPCLinuxTest);
};

class IPCLinuxTestDisabled : public IPCLinuxTest {
 public:
  IPCLinuxTestDisabled() = default;
  ~IPCLinuxTestDisabled() override = default;

  void SetUpCommandLine() override {
    // Set up with no --ipc-socket-path
    const base::CommandLine::CharType* argv[] = { "program" };
    base::CommandLine::Init(arraysize(argv), argv);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(IPCLinuxTestDisabled);
};

class TestDelegate : public ipc::IPCManager::Delegate,
                     public base::SupportsWeakPtr<TestDelegate> {
 public:
  TestDelegate() : started_count_(0), stopped_count_(0) {
  }

  void OnIPCHandlerStarted(ipc::IPCManager::Type type) override {
    ASSERT_EQ(ipc::IPCManager::TYPE_LINUX, type);
    started_count_++;
    base::MessageLoop::current()->QuitWhenIdle();
  }

  void OnIPCHandlerStopped(ipc::IPCManager::Type type) override {
    ASSERT_EQ(ipc::IPCManager::TYPE_LINUX, type);
    stopped_count_++;
    base::MessageLoop::current()->QuitWhenIdle();
  }

  int started_count() const { return started_count_; }
  int stopped_count() const { return stopped_count_; }

 private:
  int started_count_;
  int stopped_count_;

  DISALLOW_COPY_AND_ASSIGN(TestDelegate);
};

TEST_F(IPCLinuxTestDisabled, StartWithNoSocketPath) {
  TestDelegate delegate;
  EXPECT_FALSE(ipc_manager_->Start(ipc::IPCManager::TYPE_LINUX, &delegate));
  EXPECT_FALSE(ipc_manager_->LinuxStarted());
  EXPECT_EQ(0, delegate.started_count());
  EXPECT_EQ(0, delegate.stopped_count());
}

TEST_F(IPCLinuxTest, BasicStartAndExit) {
  TestDelegate delegate;
  EXPECT_TRUE(ipc_manager_->Start(ipc::IPCManager::TYPE_LINUX, &delegate));
  EXPECT_TRUE(ipc_manager_->LinuxStarted());

  // Run the message loop. We will stop the loop when we receive a delegate
  // event.
  message_loop_.Run();

  // We should have received the started event.
  EXPECT_EQ(1, delegate.started_count());
  EXPECT_EQ(0, delegate.stopped_count());

  // At this point the thread is blocking on accept and listening for incoming
  // connections. TearDown should gracefully clean up the thread and the test
  // should succeed without hanging.
  ipc_manager_.reset();
  message_loop_.Run();
  EXPECT_EQ(1, delegate.stopped_count());
}

TEST_F(IPCLinuxTest, BasicStartAndConnect) {
  TestDelegate delegate;
  EXPECT_TRUE(ipc_manager_->Start(ipc::IPCManager::TYPE_LINUX, &delegate));
  EXPECT_TRUE(ipc_manager_->LinuxStarted());

  // Run the message loop. We will stop the loop when we receive a delegate
  // event.
  message_loop_.Run();

  // We should have received the started event.
  EXPECT_EQ(1, delegate.started_count());
  EXPECT_EQ(0, delegate.stopped_count());

  // IPC successfully started. Now attempt to connect to the socket.
  ConnectToTestSocket();

  // TODO(armansito): Test that the IPC event loop shuts down cleanly while a
  // client is connected. Currently this will fail and the fix is to use
  // MessageLoopForIO rather than a custom event loop.
}

}  // namespace
