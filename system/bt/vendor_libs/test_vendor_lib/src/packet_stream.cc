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

#define LOG_TAG "packet_stream"

#include "vendor_libs/test_vendor_lib/include/packet_stream.h"

#include "base/logging.h"

extern "C" {
#include <errno.h>
#include <unistd.h>

#include "osi/include/log.h"
}  // extern "C"

namespace test_vendor_lib {

std::unique_ptr<CommandPacket> PacketStream::ReceiveCommand(int fd) const {
  std::vector<uint8_t> header;
  std::vector<uint8_t> payload;

  if (!ReceiveAll(header, CommandPacket::kCommandHeaderSize, fd)) {
    LOG_ERROR(LOG_TAG, "Error: receiving command header.");
    return std::unique_ptr<CommandPacket>(nullptr);
  }

  if (!ReceiveAll(payload, header.back(), fd)) {
    LOG_ERROR(LOG_TAG, "Error: receiving command payload.");
    return std::unique_ptr<CommandPacket>(nullptr);
  }

  std::unique_ptr<CommandPacket> command(new CommandPacket());
  if (!command->Encode(header, payload)) {
    LOG_ERROR(LOG_TAG, "Error: encoding command packet.");
    command.reset(nullptr);
  }
  return command;
}

serial_data_type_t PacketStream::ReceivePacketType(int fd) const {
  LOG_INFO(LOG_TAG, "Receiving packet type.");

  std::vector<uint8_t> raw_type_octet;

  if (!ReceiveAll(raw_type_octet, 1, fd)) {
    // TODO(dennischeng): Proper error handling.
    LOG_ERROR(LOG_TAG, "Error: Could not receive packet type.");
  }

  // Check that the type octet received is in the valid range, i.e. the packet
  // must be a command or data packet.
  const serial_data_type_t type =
      static_cast<serial_data_type_t>(raw_type_octet[0]);
  if (!ValidateTypeOctet(type)) {
    // TODO(dennischeng): Proper error handling.
    LOG_ERROR(LOG_TAG, "Error: Received invalid packet type.");
  }
  return type;
}

bool PacketStream::SendEvent(const EventPacket& event, int fd) const {
  LOG_INFO(LOG_TAG, "Sending event with event code: 0x%04X",
           event.GetEventCode());
  LOG_INFO(LOG_TAG, "Sending event with size: %zu octets",
           event.GetPacketSize());

  if (!SendAll({static_cast<uint8_t>(event.GetType())}, 1, fd)) {
    LOG_ERROR(LOG_TAG, "Error: Could not send event type.");
    return false;
  }

  if (!SendAll(event.GetHeader(), event.GetHeaderSize(), fd)) {
    LOG_ERROR(LOG_TAG, "Error: Could not send event header.");
    return false;
  }

  if (!SendAll(event.GetPayload(), event.GetPayloadSize(), fd)) {
    LOG_ERROR(LOG_TAG, "Error: Could not send event payload.");
    return false;
  }
  return true;
}

bool PacketStream::ValidateTypeOctet(serial_data_type_t type) const {
  LOG_INFO(LOG_TAG, "Signal octet is 0x%02X.", type);
  // The only types of packets that should be received from the HCI are command
  // packets and data packets.
  return (type >= DATA_TYPE_COMMAND) && (type <= DATA_TYPE_SCO);
}

bool PacketStream::ReceiveAll(std::vector<uint8_t>& destination,
                              size_t num_octets_to_receive, int fd) const {
  destination.resize(num_octets_to_receive);
  size_t octets_remaining = num_octets_to_receive;
  while (octets_remaining > 0) {
    const int num_octets_received =
        read(fd, &destination[num_octets_to_receive - octets_remaining],
             octets_remaining);
    if (num_octets_received < 0)
      return false;
    octets_remaining -= num_octets_received;
  }
  return true;
}

bool PacketStream::SendAll(const std::vector<uint8_t>& source,
                           size_t num_octets_to_send, int fd) const {
  CHECK(source.size() >= num_octets_to_send);
  size_t octets_remaining = num_octets_to_send;
  while (octets_remaining > 0) {
    const int num_octets_sent = write(
        fd, &source[num_octets_to_send - octets_remaining], octets_remaining);
    if (num_octets_sent < 0)
      return false;
    octets_remaining -= num_octets_sent;
  }
  return true;
}

}  // namespace test_vendor_lib
