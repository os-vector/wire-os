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
#include <string>
#include <vector>
#include <unordered_map>

#include "base/json/json_value_converter.h"
#include "base/time/time.h"
#include "vendor_libs/test_vendor_lib/include/command_packet.h"
#include "vendor_libs/test_vendor_lib/include/hci_transport.h"
#include "vendor_libs/test_vendor_lib/include/test_channel_transport.h"

namespace test_vendor_lib {

// Emulates a dual mode BR/EDR + LE controller by maintaining the link layer
// state machine detailed in the Bluetooth Core Specification Version 4.2,
// Volume 6, Part B, Section 1.1 (page 30). Provides methods corresponding to
// commands sent by the HCI. These methods will be registered as callbacks from
// a controller instance with the HciHandler. To implement a new Bluetooth
// command, simply add the method declaration below, with return type void and a
// single const std::vector<uint8_t>& argument. After implementing the
// method, simply register it with the HciHandler using the SET_HANDLER macro in
// the controller's default constructor. Be sure to name your method after the
// corresponding Bluetooth command in the Core Specification with the prefix
// "Hci" to distinguish it as a controller command.
class DualModeController {
 public:
  class Properties {
   public:
    // TODO(dennischeng): Add default initialization and use that to instantiate
    // a default configured controller if the config file is invalid or not
    // provided.
    Properties(const std::string& file_name);

    // Aggregates and returns the result for the Read Local Extended Features
    // command. This result contains the |maximum_page_number_| property (among
    // other things not in the Properties object). See the Bluetooth Core
    // Specification Version 4.2, Volume 2, Part E, Section 7.4.4 (page 792).
    const std::vector<uint8_t> GetBdAddress();

    // Aggregates and returns the result for the Read Buffer Size command. This
    // result consists of the |acl_data_packet_size_|, |sco_data_packet_size_|,
    // |num_acl_data_packets_|, and |num_sco_data_packets_| properties. See the
    // Bluetooth Core Specification Version 4.2, Volume 2, Part E, Section 7.4.5
    // (page 794).
    const std::vector<uint8_t> GetBufferSize();

    // Returns the result for the Read BD_ADDR command. This result is the
    // |bd_address_| property. See the Bluetooth Core Specification Version
    // 4.2, Volume 2, Part E, Section 7.4.6 (page 796).
    const std::vector<uint8_t> GetLocalExtendedFeatures(uint8_t page_number);

    // Returns the result for the Read Local Name command. See the Bluetooth
    // Core Specification Version 4.2, Volume 2, Part E, Section 7.3.12
    // (page 664).
    const std::vector<uint8_t> GetLocalName();

    // Returns the result for the Read Local Supported Commands command. See the
    // Bluetooth Core Specification Version 4.2, Volume 2, Part E, Section 7.4.2
    // (page 790).
    const std::vector<uint8_t> GetLocalSupportedCommands();

    // Aggregates and returns the Read Local Version Information result. This
    // consists of the |version_|, |revision_|, |lmp_pal_version_|,
    // |manufacturer_name_|, and |lmp_pal_subversion_|. See the Bluetooth Core
    // Specification Version 4.2, Volume 2, Part E, Section 7.4.1 (page 788).
    const std::vector<uint8_t> GetLocalVersionInformation();

    static void RegisterJSONConverter(
        base::JSONValueConverter<Properties>* converter);

   private:
    uint16_t acl_data_packet_size_;
    uint8_t sco_data_packet_size_;
    uint16_t num_acl_data_packets_;
    uint16_t num_sco_data_packets_;
    uint8_t version_;
    uint16_t revision_;
    uint8_t lmp_pal_version_;
    uint16_t manufacturer_name_;
    uint16_t lmp_pal_subversion_;
    uint8_t maximum_page_number_;
    uint8_t local_supported_commands_size_;
    uint8_t local_name_size_;
    std::vector<uint8_t> bd_address_;
  };

  // Sets all of the methods to be used as callbacks in the HciHandler.
  DualModeController();

  ~DualModeController() = default;

  // Preprocesses the command, primarily checking testh channel hooks. If
  // possible, dispatches the corresponding controller method corresponding to
  // carry out the command.
  void HandleCommand(std::unique_ptr<CommandPacket> command_packet);

  // Dispatches the test channel action corresponding to the command specified
  // by |name|.
  void HandleTestChannelCommand(const std::string& name,
                                const std::vector<std::string>& args);

  // Sets the controller Handle* methods as callbacks for the transport to call
  // when data is received.
  void RegisterHandlersWithHciTransport(HciTransport& transport);

  // Sets the test channel handler with the transport dedicated to test channel
  // communications.
  void RegisterHandlersWithTestChannelTransport(
      TestChannelTransport& transport);

  // Sets the callback to be used for sending events back to the HCI.
  // TODO(dennischeng): Once PostDelayedTask works, get rid of this and only use
  // |RegisterDelayedEventChannel|.
  void RegisterEventChannel(
      std::function<void(std::unique_ptr<EventPacket>)> send_event);

  void RegisterDelayedEventChannel(
      std::function<void(std::unique_ptr<EventPacket>, base::TimeDelta)>
          send_event);

  // Controller commands. For error codes, see the Bluetooth Core Specification,
  // Version 4.2, Volume 2, Part D (page 370).

  // OGF: 0x0003
  // OCF: 0x0003
  // Bluetooth Core Specification Version 4.2 Volume 2 Part E 7.3.2
  void HciReset(const std::vector<uint8_t>& args);

  // OGF: 0x0004
  // OGF: 0x0005
  // Bluetooth Core Specification Version 4.2 Volume 2 Part E 7.4.5
  void HciReadBufferSize(const std::vector<uint8_t>& args);

  // OGF: 0x0003
  // OCF: 0x0033
  // Bluetooth Core Specification Version 4.2 Volume 2 Part E 7.3.39
  void HciHostBufferSize(const std::vector<uint8_t>& args);

  // OGF: 0x0004
  // OCF: 0x0001
  // Bluetooth Core Specification Version 4.2 Volume 2 Part E 7.4.1
  void HciReadLocalVersionInformation(const std::vector<uint8_t>& args);

  // OGF: 0x0004
  // OCF: 0x0009
  // Bluetooth Core Specification Version 4.2 Volume 2 Part E 7.4.6
  void HciReadBdAddr(const std::vector<uint8_t>& args);

  // OGF: 0x0004
  // OCF: 0x0002
  // Bluetooth Core Specification Version 4.2 Volume 2 Part E 7.4.2
  void HciReadLocalSupportedCommands(const std::vector<uint8_t>& args);

  // OGF: 0x0004
  // OCF: 0x0004
  // Bluetooth Core Specification Version 4.2 Volume 2 Part E 7.4.4
  void HciReadLocalExtendedFeatures(const std::vector<uint8_t>& args);

  // OGF: 0x0003
  // OCF: 0x0056
  // Bluetooth Core Specification Version 4.2 Volume 2 Part E 7.3.59
  void HciWriteSimplePairingMode(const std::vector<uint8_t>& args);

  // OGF: 0x0003
  // OCF: 0x006D
  // Bluetooth Core Specification Version 4.2 Volume 2 Part E 7.3.79
  void HciWriteLeHostSupport(const std::vector<uint8_t>& args);

  // OGF: 0x0003
  // OCF: 0x0001
  // Bluetooth Core Specification Version 4.2 Volume 2 Part E 7.3.1
  void HciSetEventMask(const std::vector<uint8_t>& args);

  // OGF: 0x0003
  // OCF: 0x0045
  // Bluetooth Core Specification Version 4.2 Volume 2 Part E 7.3.50
   void HciWriteInquiryMode(const std::vector<uint8_t>& args);

  // OGF: 0x0003
  // OCF: 0x0047
  // Bluetooth Core Specification Version 4.2 Volume 2 Part E 7.3.52
   void HciWritePageScanType(const std::vector<uint8_t>& args);

  // OGF: 0x0003
  // OCF: 0x0043
  // Bluetooth Core Specification Version 4.2 Volume 2 Part E 7.3.48
  void HciWriteInquiryScanType(const std::vector<uint8_t>& args);

  // OGF: 0x0003
  // OCF: 0x0024
  // Bluetooth Core Specification Version 4.2 Volume 2 Part E 7.3.26
  void HciWriteClassOfDevice(const std::vector<uint8_t>& args);

  // OGF: 0x0003
  // OCF: 0x0018
  // Bluetooth Core Specification Version 4.2 Volume 2 Part E 7.3.16
  void HciWritePageTimeout(const std::vector<uint8_t>& args);

  // OGF: 0x0002
  // OCF: 0x000F
  // Bluetooth Core Specification Version 4.2 Volume 2 Part E 7.2.12
  void HciWriteDefaultLinkPolicySettings(const std::vector<uint8_t>& args);

  // OGF: 0x0003
  // OCF: 0x0014
  // Bluetooth Core Specification Version 4.2 Volume 2 Part E 7.3.12
  void HciReadLocalName(const std::vector<uint8_t>& args);

  // OGF: 0x0003
  // OCF: 0x0013
  // Bluetooth Core Specification Version 4.2 Volume 2 Part E 7.3.11
  void HciWriteLocalName(const std::vector<uint8_t>& args);

  // OGF: 0x0003
  // OCF: 0x0052
  // Bluetooth Core Specification Version 4.2 Volume 2 Part E 7.3.56
  void HciWriteExtendedInquiryResponse(const std::vector<uint8_t>& args);

  // OGF: 0x0003
  // OCF: 0x0026
  // Bluetooth Core Specification Version 4.2 Volume 2 Part E 7.3.28
  void HciWriteVoiceSetting(const std::vector<uint8_t>& args);

  // OGF: 0x0003
  // OCF: 0x003A
  // Bluetooth Core Specification Version 4.2 Volume 2 Part E 7.3.45
    void HciWriteCurrentIacLap(const std::vector<uint8_t>& args);

  // OGF: 0x0003
  // OCF: 0x001E
  // Bluetooth Core Specification Version 4.2 Volume 2 Part E 7.3.22
   void HciWriteInquiryScanActivity(const std::vector<uint8_t>& args);

  // OGF: 0x0003
  // OCF: 0x001A
  // Bluetooth Core Specification Version 4.2 Volume 2 Part E 7.3.18
  void HciWriteScanEnable(const std::vector<uint8_t>& args);

  // OGF: 0x0003
  // OCF: 0x0005
  // Bluetooth Core Specification Version 4.2 Volume 2 Part E 7.3.3
  void HciSetEventFilter(const std::vector<uint8_t>& args);

  // OGF: 0x0001
  // OCF: 0x0001
  // Bluetooth Core Specification Version 4.2 Volume 2 Part E 7.1.1
  void HciInquiry(const std::vector<uint8_t>& args);

  // OGF: 0x0001
  // OCF: 0x0002
  // Bluetooth Core Specification Version 4.2 Volume 2 Part E 7.1.2
  void HciInquiryCancel(const std::vector<uint8_t>& args);

  // OGF: 0x0003
  // OCF: 0x0012
  // Bluetooth Core Specification Version 4.2 Volume 2 Part E 7.3.10
  void HciDeleteStoredLinkKey(const std::vector<uint8_t>& args);

  // OGF: 0x0001
  // OCF: 0x0019
  // Bluetooth Core Specification Version 4.2 Volume 2 Part E 7.1.19
  void HciRemoteNameRequest(const std::vector<uint8_t>& args);

  // Test Channel commands:

  // Clears all test channel modifications.
  void TestChannelClear(const std::vector<std::string>& args);

  // Sets the response delay for events to 0.
  void TestChannelClearEventDelay(const std::vector<std::string>& args);

  // Discovers a fake device.
  void TestChannelDiscover(const std::vector<std::string>& args);

  // Causes events to be sent after a delay.
  void TestChannelSetEventDelay(const std::vector<std::string>& args);

  // Causes all future HCI commands to timeout.
  void TestChannelTimeoutAll(const std::vector<std::string>& args);

 private:
  // Current link layer state of the controller.
  enum State {
    kStandby,  // Not receiving/transmitting any packets from/to other devices.
    kInquiry,  // The controller is discovering other nearby devices.
  };

  enum TestChannelState {
    kNone,  // The controller is running normally.
    kTimeoutAll,  // All commands should time out, i.e. send no response.
    kDelayedResponse,  // Event responses are sent after a delay.
  };

  // Creates a command complete event and sends it back to the HCI.
  void SendCommandComplete(uint16_t command_opcode,
                           const std::vector<uint8_t>& return_parameters) const;

  // Sends a command complete event with no return parameters. This event is
  // typically sent for commands that can be completed immediately.
  void SendCommandCompleteSuccess(uint16_t command_opcode) const;

  // Creates a command status event and sends it back to the HCI.
  void SendCommandStatus(uint8_t status, uint16_t command_opcode) const;

  // Sends a command status event with default event parameters.
  void SendCommandStatusSuccess(uint16_t command_opcode) const;

  // Sends an inquiry response for a fake device.
  void SendInquiryResult() const;

  // Sends an extended inquiry response for a fake device.
  void SendExtendedInquiryResult(const std::string& name,
                                 const std::string& address) const;

  void SetEventDelay(int64_t delay);

  // Callback provided to send events from the controller back to the HCI.
  std::function<void(std::unique_ptr<EventPacket>)> send_event_;

  std::function<void(std::unique_ptr<EventPacket>, base::TimeDelta)>
      send_delayed_event_;

  // Maintains the commands to be registered and used in the HciHandler object.
  // Keys are command opcodes and values are the callbacks to handle each
  // command.
  std::unordered_map<uint16_t, std::function<void(const std::vector<uint8_t>&)>>
      active_hci_commands_;

  std::unordered_map<std::string,
                     std::function<void(const std::vector<std::string>&)>>
      active_test_channel_commands_;

  // Specifies the format of Inquiry Result events to be returned during the
  // Inquiry command.
  // 0x00: Standard Inquiry Result event format (default).
  // 0x01: Inquiry Result format with RSSI.
  // 0x02 Inquiry Result with RSSI format or Extended Inquiry Result format.
  // 0x03-0xFF: Reserved.
  uint8_t inquiry_mode_;

  State state_;

  Properties properties_;

  TestChannelState test_channel_state_;

  DISALLOW_COPY_AND_ASSIGN(DualModeController);
};

}  // namespace test_vendor_lib
