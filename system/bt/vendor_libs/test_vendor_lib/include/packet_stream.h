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

#include <cstdint>
#include <vector>
#include <memory>

#include "vendor_libs/test_vendor_lib/include/command_packet.h"
#include "vendor_libs/test_vendor_lib/include/event_packet.h"
#include "vendor_libs/test_vendor_lib/include/packet.h"

namespace test_vendor_lib {

// Provides abstractions for IO with Packet objects. Used to receive commands
// and data from the HCI and to send controller events back to the host.
class PacketStream {
 public:
  PacketStream() = default;

  virtual ~PacketStream() = default;

  // Reads a command packet from the file descriptor at |fd| and returns the
  // packet back to the caller, along with the responsibility of managing the
  // packet.
  std::unique_ptr<CommandPacket> ReceiveCommand(int fd) const;

  // Reads a single octet from |fd| and interprets it as a packet type octet.
  // Validates the type octet for correctness.
  serial_data_type_t ReceivePacketType(int fd) const;

  // Sends an event to file descriptor |fd|. The ownership of the event is left
  // with the caller.
  bool SendEvent(const EventPacket& event, int fd) const;

 private:
  // Checks if |type| is in the valid range from DATA_TYPE_COMMAND to
  // DATA_TYPE_SCO.
  bool ValidateTypeOctet(serial_data_type_t type) const;

  // Attempts to receive |num_octets_to_receive| into |destination| from |fd|,
  // returning false if an error occurs.
  bool ReceiveAll(std::vector<uint8_t>& destination,
                  size_t num_octets_to_receive, int fd) const;

  // Attempts to send |num_octets_to_send| from |source| to |fd|, returning
  // false if an error occurs.
  bool SendAll(const std::vector<uint8_t>& source, size_t num_octets_to_send,
               int fd) const;
};

}  // namespace test_vendor_lib
