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

#include "vendor_libs/test_vendor_lib/include/packet_stream.h"
#include "vendor_libs/test_vendor_lib/include/command_packet.h"
#include "vendor_libs/test_vendor_lib/include/event_packet.h"
#include "vendor_libs/test_vendor_lib/include/packet.h"

#include <gtest/gtest.h>
#include <cstdint>
#include <memory>
#include <vector>

extern "C" {
#include "hci/include/hci_hal.h"
#include "stack/include/hcidefs.h"

#include <sys/socket.h>
}  // extern "C"

namespace {
const char small_payload[] = "foo bar baz";
const char large_payload[] =
  "Aristotle's principles will then be no more principles to him, than those "
  "of Epicurus and the Stoics: let this diversity of opinions be propounded "
  "to, and laid before him; he will himself choose, if he be able; if not, "
  "he will remain in doubt.";
}  // namespace

namespace test_vendor_lib {

class PacketStreamTest : public ::testing::Test {
 public:
  PacketStreamTest() {
    socketpair(AF_LOCAL, SOCK_STREAM, 0, socketpair_fds_);
    CheckSocketpairInit();
  }

  ~PacketStreamTest() {
    close(socketpair_fds_[0]);
    close(socketpair_fds_[1]);
  }

  void CheckedReceiveCommand(const char* payload, uint16_t opcode) {
    uint8_t payload_size = strlen(payload);
    std::vector<uint8_t> packet;

    packet.push_back(DATA_TYPE_COMMAND);
    packet.push_back(opcode);
    packet.push_back(opcode >> 8);
    packet.push_back(payload_size);

    // Set the packet's payload.
    for (int i = 0; i < payload_size; ++i)
      packet.push_back(payload[i]);

    // Send the packet to |packet_stream_|.
    write(socketpair_fds_[1], &packet[1], packet.size());

    // Read the command packet.
    std::unique_ptr<CommandPacket> command =
        packet_stream_.ReceiveCommand(socketpair_fds_[0]);

    const std::vector<uint8_t> received_payload = command->GetPayload();

    // Validate the packet by checking that it's the appropriate size and then
    // checking each byte.
    EXPECT_EQ(packet.size(), command->GetPacketSize());
    EXPECT_EQ(DATA_TYPE_COMMAND, command->GetType());
    EXPECT_EQ(opcode, command->GetOpcode());
    EXPECT_EQ(payload_size, command->GetPayloadSize());
    for (int i = 0; i < payload_size; ++i)
      EXPECT_EQ(packet[4 + i], received_payload[i]);
  }

  void CheckedSendEvent(std::unique_ptr<EventPacket> event) {
    EXPECT_TRUE(packet_stream_.SendEvent(*(event.get()), socketpair_fds_[0]));

    // Read the packet sent by |packet_stream_|.
    uint8_t event_header[3];
    read(socketpair_fds_[1], event_header, 3);

    uint8_t return_parameters_size = event_header[2];
    uint8_t return_parameters[return_parameters_size];
    read(socketpair_fds_[1], return_parameters, sizeof(return_parameters));

    const std::vector<uint8_t> expected_payload = event->GetPayload();

    // Validate the packet by checking that it's the
    // appropriate size and then checking each byte.
    EXPECT_EQ(event->GetPacketSize(),
              sizeof(event_header) + sizeof(return_parameters));
    EXPECT_EQ(DATA_TYPE_EVENT, event_header[0]);
    EXPECT_EQ(event->GetEventCode(), event_header[1]);
    EXPECT_EQ(event->GetPayloadSize(), return_parameters_size);
    for (int i = 0; i < return_parameters_size; ++i)
      EXPECT_EQ(expected_payload[i], return_parameters[i]);
  }

 protected:
  PacketStream packet_stream_;

  int socketpair_fds_[2];

 private:
  // Workaround because ASSERT cannot be used directly in a constructor
  void CheckSocketpairInit() {
    ASSERT_TRUE(socketpair_fds_[0] > 0);
    ASSERT_TRUE(socketpair_fds_[1] > 0);
  }

};

TEST_F(PacketStreamTest, ReceivePacketType) {
  serial_data_type_t command_type = DATA_TYPE_COMMAND;
  write(socketpair_fds_[1], &command_type, 1);
  EXPECT_EQ(command_type, packet_stream_.ReceivePacketType(socketpair_fds_[0]));
}

TEST_F(PacketStreamTest, ReceiveEmptyCommand) {
  CheckedReceiveCommand("", HCI_RESET);
}

TEST_F(PacketStreamTest, ReceiveSmallCommand) {
  CheckedReceiveCommand(small_payload, HCI_RESET);
}

TEST_F(PacketStreamTest, ReceiveLargeCommand) {
  CheckedReceiveCommand(large_payload, HCI_RESET);
}

TEST_F(PacketStreamTest, SendEvent) {
  const std::vector<uint8_t> return_parameters = {0};
  CheckedSendEvent(
      EventPacket::CreateCommandCompleteEvent(1, HCI_RESET, return_parameters));
}

}  // namespace test_vendor_lib
