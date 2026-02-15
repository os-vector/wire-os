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

#include "base/macros.h"
#include "vendor_libs/test_vendor_lib/include/packet.h"

namespace test_vendor_lib {

// The following is specified in the Bluetooth Core Specification Version 4.2,
// Volume 2, Part E, Section 5.4.1 (page 470). Command Packets begin with a 3
// octet header formatted as follows:
// - Opcode: 2 octets
//   - Opcode Group Field (OGF): Upper bits 10-15
//   - Opcode Command Field (OCF): Lower bits 0-9
// - Payload size (in octets): 1 octet
// The header is followed by the payload, which contains command specific
// parameters and has a maximum size of 255 octets. Valid command opcodes are
// defined in stack/include/hcidefs.h. The OGF ranges from 0x00 to 0x3F, with
// 0x3F reserved for vendor-specific debug functions. The OCF ranges from
// 0x0000 to 0x03FF. Note that the payload size is the size in octets of the
// command parameters and not the number of parameters. Finally, although the
// parameters contained in the payload are command specific (including the size
// and number of parameters), each parameter will be an integer number of octets
// in size.
class CommandPacket : public Packet {
 public:
  CommandPacket();

  virtual ~CommandPacket() override = default;

  // Returns the command opcode as defined in stack/include/hcidefs.h.
  // See the Bluetooth Core Specification Version 4.2, Volume 2, Part E,
  // Section 7 for more information about each HCI commands and for a listing
  // of their specific opcodes/OGF and OCF values.
  uint16_t GetOpcode() const;

  // Returns the 6 bit opcode group field that specifies the general category of
  // the command. The OGF can be one of seven values:
  // - 0x01: Link control commands
  // - 0x02: Link policy commands
  // - 0x03: Controller and baseband commands
  // - 0x04: Informational parameters commands
  // - 0x05: Status parameters commands
  // - 0x06: Testing commands
  // - 0x08: Low energy controller commands
  // The upper 2 bits will be zero filled.
  uint8_t GetOGF() const;

  // Returns the 10 bit opcode command field that specifies an exact command
  // within an opcode group field. The upper 6 bits will be zero filled.
  uint16_t GetOCF() const;

  // Size in octets of a command packet header, which consists of a 2 octet
  // opcode and a 1 octet payload size.
  static const size_t kCommandHeaderSize = 3;

 private:
  // Disallow any copies of the singleton to be made.
  DISALLOW_COPY_AND_ASSIGN(CommandPacket);
};

}  // namespace test_vendor_lib
