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

#define LOG_TAG "event_packet"

#define VECTOR_COPY_TO_END(source, destination) \
  std::copy(source.begin(), source.end(), std::back_inserter(destination));

#include "vendor_libs/test_vendor_lib/include/event_packet.h"

extern "C" {
#include "osi/include/log.h"
#include "stack/include/hcidefs.h"
}  // extern "C"

namespace test_vendor_lib {

EventPacket::EventPacket(uint8_t event_code,
                         const std::vector<uint8_t>& payload)
    : Packet(DATA_TYPE_EVENT) {
  Encode({event_code, static_cast<uint8_t>(payload.size())}, payload);
}

uint8_t EventPacket::GetEventCode() const {
  return GetHeader()[0];
}

// static
std::unique_ptr<EventPacket> EventPacket::CreateCommandCompleteEvent(
    uint8_t num_hci_command_packets, uint16_t command_opcode,
    const std::vector<uint8_t>& event_return_parameters) {
  size_t payload_size = sizeof(num_hci_command_packets) +
                        sizeof(command_opcode) + event_return_parameters.size();

  std::vector<uint8_t> payload;
  payload.reserve(payload_size);
  payload.push_back(num_hci_command_packets);
  payload.push_back(command_opcode);
  payload.push_back(command_opcode >> 8);
  VECTOR_COPY_TO_END(event_return_parameters, payload);

  return std::unique_ptr<EventPacket>(
      new EventPacket(HCI_COMMAND_COMPLETE_EVT, payload));
}

// static
std::unique_ptr<EventPacket> EventPacket::CreateCommandStatusEvent(
    uint8_t status, uint8_t num_hci_command_packets, uint16_t command_opcode) {
  size_t payload_size =
      sizeof(status) + sizeof(num_hci_command_packets) + sizeof(command_opcode);

  std::vector<uint8_t> payload;
  payload.reserve(payload_size);
  payload.push_back(status);
  payload.push_back(num_hci_command_packets);
  payload.push_back(command_opcode);
  payload.push_back(command_opcode >> 8);

  return std::unique_ptr<EventPacket>(
      new EventPacket(HCI_COMMAND_STATUS_EVT, payload));
}

//static
std::unique_ptr<EventPacket> EventPacket::CreateInquiryResultEvent(
    uint8_t num_responses, const std::vector<uint8_t>& bd_addresses,
    const std::vector<uint8_t>& page_scan_repetition_mode,
    const std::vector<uint8_t>& page_scan_period_mode,
    const std::vector<uint8_t>& page_scan_mode,
    const std::vector<uint8_t>& class_of_device,
    const std::vector<uint8_t>& clock_offset) {
  size_t payload_size = sizeof(num_responses) + bd_addresses.size() +
                        page_scan_repetition_mode.size() +
                        page_scan_period_mode.size() + page_scan_mode.size() +
                        class_of_device.size() + clock_offset.size();

  std::vector<uint8_t> payload;
  payload.reserve(payload_size);
  payload.push_back(num_responses);
  VECTOR_COPY_TO_END(bd_addresses, payload);
  VECTOR_COPY_TO_END(page_scan_repetition_mode, payload);
  VECTOR_COPY_TO_END(page_scan_mode, payload);
  VECTOR_COPY_TO_END(class_of_device, payload);
  VECTOR_COPY_TO_END(clock_offset, payload);

  return std::unique_ptr<EventPacket>(
      new EventPacket(HCI_INQUIRY_RESULT_EVT, payload));
}

//static
std::unique_ptr<EventPacket> EventPacket::CreateExtendedInquiryResultEvent(
    const std::vector<uint8_t>& bd_address,
    const std::vector<uint8_t>& page_scan_repetition_mode,
    const std::vector<uint8_t>& page_scan_period_mode,
    const std::vector<uint8_t>& class_of_device,
    const std::vector<uint8_t>& clock_offset,
    const std::vector<uint8_t>& rssi,
    const std::vector<uint8_t>& extended_inquiry_response) {
  size_t payload_size =
      1 + bd_address.size() + page_scan_repetition_mode.size() +
      page_scan_period_mode.size() + class_of_device.size() +
      clock_offset.size() + rssi.size() + extended_inquiry_response.size();

  std::vector<uint8_t> payload;
  payload.reserve(payload_size);
  payload.push_back(1);  // Each extended inquiry result contains one device.
  VECTOR_COPY_TO_END(bd_address, payload);
  VECTOR_COPY_TO_END(page_scan_repetition_mode, payload);
  VECTOR_COPY_TO_END(page_scan_period_mode, payload);
  VECTOR_COPY_TO_END(class_of_device, payload);
  VECTOR_COPY_TO_END(clock_offset, payload);
  VECTOR_COPY_TO_END(rssi, payload);
  VECTOR_COPY_TO_END(extended_inquiry_response, payload);

  return std::unique_ptr<EventPacket>(
      new EventPacket(HCI_EXTENDED_INQUIRY_RESULT_EVT, payload));
}

}  // namespace test_vendor_lib
