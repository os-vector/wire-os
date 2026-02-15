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

extern "C" {
#include "hci/include/hci_hal.h"
}  // extern "C"

namespace test_vendor_lib {

// Abstract base class that is subclassed to provide type-specifc accessors on
// data. Manages said data's memory and guarantees the data's persistence for IO
// operations.
class Packet {
 public:
  virtual ~Packet() = default;

  // Returns the size in octets of the entire packet, which consists of the type
  // octet, the header, and the payload.
  size_t GetPacketSize() const;

  const std::vector<uint8_t>& GetPayload() const;

  uint8_t GetPayloadSize() const;

  const std::vector<uint8_t>& GetHeader() const;

  uint8_t GetHeaderSize() const;

  serial_data_type_t GetType() const;

  // Validates the packet by checking that the payload size in the header is
  // accurate. If the size is not valid, returns false. Otherwise, the data in
  // |header| and |payload| is copied into |header_| and |payload_|
  // respectively. If an error occurs while the data is being copied, the
  // contents of |header| and |payload| are guaranteed to be preserved. The
  // packet object will assume ownership of the copied data for its entire
  // lifetime.
  bool Encode(const std::vector<uint8_t>& header,
              const std::vector<uint8_t>& payload);

 protected:
  // Constructs an empty packet of type |type|. A call to Encode() shall be made
  // to check and fill in the packet's data.
  Packet(serial_data_type_t type);

 private:
  // Underlying containers for storing the actual packet, broken down into the
  // packet header and the packet payload. Data is copied into the vectors
  // during the constructor and becomes accessible (read only) to children
  // through GetHeader() and GetPayload().
  std::vector<uint8_t> header_;

  std::vector<uint8_t> payload_;

  // The packet type is one of DATA_TYPE_ACL, DATA_TYPE_COMMAND,
  // DATA_TYPE_EVENT, or DATA_TYPE_SCO.
  serial_data_type_t type_;
};

}  // namespace test_vendor_lib
