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

#define LOG_TAG "packet"

#include "vendor_libs/test_vendor_lib/include/packet.h"

#include "base/logging.h"

#include <algorithm>

extern "C" {
#include "osi/include/log.h"
}  // extern "C"

namespace test_vendor_lib {

Packet::Packet(serial_data_type_t type) : type_(type) {}

bool Packet::Encode(const std::vector<uint8_t>& header,
                    const std::vector<uint8_t>& payload) {
  if (header.back() != payload.size())
    return false;
  header_ = header;
  payload_ = payload;
  return true;
}

const std::vector<uint8_t>& Packet::GetHeader() const {
  // Every packet must have a header.
  CHECK(GetHeaderSize() > 0);
  return header_;
}

uint8_t Packet::GetHeaderSize() const {
  return header_.size();
}

size_t Packet::GetPacketSize() const {
  // Add one for the type octet.
  return 1 + header_.size() + payload_.size();
}

const std::vector<uint8_t>& Packet::GetPayload() const {
  return payload_;
}

uint8_t Packet::GetPayloadSize() const {
  return payload_.size();
}

serial_data_type_t Packet::GetType() const {
  return type_;
}

}  // namespace test_vendor_lib
