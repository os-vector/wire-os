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
#include <memory>
#include <vector>

#include "base/logging.h"
#include "vendor_libs/test_vendor_lib/include/packet.h"

namespace test_vendor_lib {

// The following is specified in the Bluetooth Core Specification Version 4.2,
// Volume 2, Part E, Section 5.4.4 (page 477). Event Packets begin with a 2
// octet header formatted as follows:
// - Event code: 1 octet
// - Payload size (in octets): 1 octet
// The header is followed by the payload, which contains event specific
// parameters and has a maximum size of 255 octets. Valid event codes are
// listed in stack/include/hcidefs.h. They can range from 0x00 to 0xFF, with
// 0xFF reserved for vendor specific debug events. Note the payload size
// describes the total size of the event parameters and not the number of
// parameters. The event parameters contained in the payload will be an integer
// number of octets in size. Each flavor of event packet is created via a static
// factory function that takes the event type-specific parameters and returns an
// initialized event packet from that data.
class EventPacket : public Packet {
 public:
  virtual ~EventPacket() override = default;

  uint8_t GetEventCode() const;

  // Static functions for creating event packets:

  // Creates and returns a command complete event packet. See the Bluetooth
  // Core Specification Version 4.2, Volume 2, Part E, Section 7.7.14 (page 861)
  // for more information about the command complete event.
  // Event Parameters:
  //   |num_hci_command_packets|
  //     Indicates the number of HCI command packets the host can send to the
  //     controller. If |num_hci_command_packets| is 0, the controller would
  //     like to stop receiving commands from the host (to indicate
  //     readiness again, the controller sends a command complete event with
  //     |command_opcode| to 0x0000 (no op) and |num_hci_command_packets| > 1).
  //   |command_opcode|
  //     The opcode of the command that caused this event.
  //   |return_parameters|
  //     Contains any event specific parameters that should
  //     be sent to the host.
  static std::unique_ptr<EventPacket> CreateCommandCompleteEvent(
      uint8_t num_hci_command_packets, uint16_t command_opcode,
      const std::vector<uint8_t>& event_return_parameters);

  // Creates and returns a command complete event packet. See the Bluetooth
  // Core Specification Version 4.2, Volume 2, Part E, Section 7.7.15 (page 862)
  // for more information about the command complete event.
  // Event Parameters:
  //   Status
  //     0x00: Command currently in pending.
  //     0x01-0xFF: Command failed.
  //   |num_hci_command_packets|
  //     Indicates the number of HCI command packets the host can send to the
  //     controller. If |num_hci_command_packets| is 0, the controller would
  //     like to stop receiving commands from the host (to indicate
  //     readiness again, the controller sends a command complete event with
  //     |command_opcode| to 0x0000 (no op) and |num_hci_command_packets| > 1).
  //   |command_opcode|
  //     The opcode of the command that caused this event.
  static std::unique_ptr<EventPacket> CreateCommandStatusEvent(
      uint8_t status, uint8_t num_hci_command_packets, uint16_t command_opcode);

  // Creates and returns an inquiry result event packet. See the Bluetooth
  // Core Specification Version 4.2, Volume 2, Part E, Section 7.7.2 (page 844)
  // for more information about the command complete event.
  // Event Parameters:
  //   Num Responses (1 octet)
  //     0xXX: Number of responses from the inquiry.
  //   Bd Addresses (6 octets * Num Responses)
  //     0xXXXXXXXXXXX: Bd Address for each device which responded.
  //   Page Scan Repetition Mode (1 octet * Num Responses)
  //     0x00: R0
  //     0x01: R1
  //     0x02: R2
  //     0x03-0xFF: Reserved.
  //   Reserved 1 (1 octet * Num Responses)
  //     Originally Page Scan Period Mode parameter. No longer in use.
  //   Reserved 2 (1 octet * Num Responses)
  //     Originally Page Scan Mode parameter. No longer in use.
  //   Class of Device (3 octet * Num Responses)
  //     0xXXXXXX: Class of device.
  //   Clock Offset (2 octet * Num Responses)
  //     Bits 14-0: Bits 16-2 of CLKNslave-CLK.
  //     Bits 15: Reserved.
  static std::unique_ptr<EventPacket> CreateInquiryResultEvent(
      uint8_t num_responses, const std::vector<uint8_t>& bd_addresses,
      const std::vector<uint8_t>& page_scan_repetition_mode,
      const std::vector<uint8_t>& page_scan_period_mode,
      const std::vector<uint8_t>& page_scan_mode,
      const std::vector<uint8_t>& class_of_device,
      const std::vector<uint8_t>& clock_offset);

  // Creates and returns an inquiry result event packet. See the Bluetooth
  // Core Specification Version 4.2, Volume 2, Part E, Section 7.7.38 (page 896)
  // for more information about the command complete event.
  // Event Parameters:
  //   Num Responses (1 octet)
  //     0x01: Always contains a single response.
  //   Bd Addresses (6 octets * Num Responses)
  //     0xXXXXXXXXXXX: Bd Address for each device which responded.
  //   Page Scan Repetition Mode (1 octet * Num Responses)
  //     0x00: R0
  //     0x01: R1
  //     0x02: R2
  //     0x03-0xFF: Reserved.
  //   Reserved 1 (1 octet * Num Responses)
  //     Originally Page Scan Period Mode parameter. No longer in use.
  //   Class of Device (3 octet * Num Responses)
  //     0xXXXXXX: Class of device.
  //   Clock Offset (2 octet * Num Responses)
  //     Bits 14-0: Bits 16-2 of CLKNslave-CLK.
  //     Bits 15: Reserved.
  //   RSSI (1 octet)
  //     0xXX: Ranges from -127 to +20. Units are dBm.
  //  Extended Inquiry Response (240 octets)
  //    Defined in Volumne 2, Part C, Section 8. Also see the Supplement to the
  //    Bluetooth Core Specificiation for data type definitions and formats.
  static std::unique_ptr<EventPacket> CreateExtendedInquiryResultEvent(
      const std::vector<uint8_t>& bd_address,
      const std::vector<uint8_t>& page_scan_repetition_mode,
      const std::vector<uint8_t>& page_scan_period_mode,
      const std::vector<uint8_t>& class_of_device,
      const std::vector<uint8_t>& clock_offset,
      const std::vector<uint8_t>& rssi,
      const std::vector<uint8_t>& extended_inquiry_response);

  // Size in octets of a data packet header, which consists of a 1 octet
  // event code and a 1 octet payload size.
  static const size_t kEventHeaderSize = 2;

 private:
  // Takes in the event parameters in |payload|. These parameters vary by event
  // and are detailed in the Bluetooth Core Specification.
  EventPacket(uint8_t event_code, const std::vector<uint8_t>& payload);
};

}  // namespace test_vendor_lib
