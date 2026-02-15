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

#define LOG_TAG "command_packet"

#include "vendor_libs/test_vendor_lib/include/command_packet.h"

extern "C" {
#include "hci/include/hci_hal.h"
#include "osi/include/log.h"
#include "stack/include/hcidefs.h"
}  // extern "C"

namespace test_vendor_lib {

CommandPacket::CommandPacket() : Packet(DATA_TYPE_COMMAND) {}

uint16_t CommandPacket::GetOpcode() const {
  return 0 | (GetHeader()[0] | (GetHeader()[1] << 8));
}

uint8_t CommandPacket::GetOGF() const {
  return HCI_OGF(GetOpcode());
}

uint16_t CommandPacket::GetOCF() const {
  return HCI_OCF(GetOpcode());
}

}  // namespace test_vendor_lib
