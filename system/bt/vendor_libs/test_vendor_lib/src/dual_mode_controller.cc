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

#define LOG_TAG "dual_mode_controller"

#include "vendor_libs/test_vendor_lib/include/dual_mode_controller.h"

#include "base/logging.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/values.h"
#include "vendor_libs/test_vendor_lib/include/event_packet.h"
#include "vendor_libs/test_vendor_lib/include/hci_transport.h"

extern "C" {
#include "stack/include/hcidefs.h"
#include "osi/include/log.h"
}  // extern "C"

namespace {

// Included in certain events to indicate success (specific to the event
// context).
const uint8_t kSuccessStatus = 0;

// The default number encoded in event packets to indicate to the HCI how many
// command packets it can send to the controller.
const uint8_t kNumHciCommandPackets = 1;

// The location of the config file loaded to populate controller attributes.
const std::string kControllerPropertiesFile =
    "/etc/bluetooth/controller_properties.json";

// Inquiry modes for specifiying inquiry result formats.
const uint8_t kStandardInquiry = 0x00;
const uint8_t kRssiInquiry = 0x01;
const uint8_t kExtendedOrRssiInquiry = 0x02;

// The bd address of another (fake) device.
const std::vector<uint8_t> kOtherDeviceBdAddress = {6, 5, 4, 3, 2, 1};

// Fake inquiry response for a fake device.
const std::vector<uint8_t> kPageScanRepetitionMode = {0};
const std::vector<uint8_t> kPageScanPeriodMode = {0};
const std::vector<uint8_t> kPageScanMode = {0};
const std::vector<uint8_t> kClassOfDevice = {1, 2, 3};
const std::vector<uint8_t> kClockOffset = {1, 2};

void LogCommand(const char* command) {
  LOG_INFO(LOG_TAG, "Controller performing command: %s", command);
}

// Functions used by JSONValueConverter to read stringified JSON into Properties
// object.
bool ParseUint8t(const base::StringPiece& value, uint8_t* field) {
  *field = std::stoi(value.as_string());
  return true;
}

bool ParseUint16t(const base::StringPiece& value, uint16_t* field) {
  *field = std::stoi(value.as_string());
  return true;
}

bool ParseUint8tVector(const base::StringPiece& value,
                              std::vector<uint8_t>* field) {
  for (char& c : value.as_string())
    field->push_back(c - '0');
  return true;
}

}  // namespace

namespace test_vendor_lib {

void DualModeController::SendCommandComplete(
    uint16_t command_opcode,
    const std::vector<uint8_t>& return_parameters) const {
  std::unique_ptr<EventPacket> command_complete =
      EventPacket::CreateCommandCompleteEvent(
          kNumHciCommandPackets, command_opcode, return_parameters);
  send_event_(std::move(command_complete));
}

void DualModeController::SendCommandCompleteSuccess(
    uint16_t command_opcode) const {
  SendCommandComplete(command_opcode, {kSuccessStatus});
}

void DualModeController::SendCommandStatus(uint8_t status,
                                           uint16_t command_opcode) const {
  std::unique_ptr<EventPacket> command_status =
      EventPacket::CreateCommandStatusEvent(status, kNumHciCommandPackets,
                                            command_opcode);
  send_event_(std::move(command_status));
}

void DualModeController::SendCommandStatusSuccess(
    uint16_t command_opcode) const {
  SendCommandStatus(kSuccessStatus, command_opcode);
}

void DualModeController::SendInquiryResult() const {
  std::unique_ptr<EventPacket> inquiry_result =
      EventPacket::CreateInquiryResultEvent(
          1, kOtherDeviceBdAddress, kPageScanRepetitionMode,
          kPageScanPeriodMode, kPageScanMode, kClassOfDevice, kClockOffset);
  send_event_(std::move(inquiry_result));
}

void DualModeController::SendExtendedInquiryResult(
    const std::string& name, const std::string& address) const {
  std::vector<uint8_t> rssi = {0};
  std::vector<uint8_t> extended_inquiry_data = {name.length() + 1, 0x09};
  std::copy(name.begin(), name.end(),
            std::back_inserter(extended_inquiry_data));
  std::vector<uint8_t> bd_address(address.begin(), address.end());
  // TODO(dennischeng): Use constants for parameter sizes, here and elsewhere.
  while (extended_inquiry_data.size() < 240) {
    extended_inquiry_data.push_back(0);
  }
  std::unique_ptr<EventPacket> extended_inquiry_result =
      EventPacket::CreateExtendedInquiryResultEvent(
          bd_address, kPageScanRepetitionMode, kPageScanPeriodMode,
          kClassOfDevice, kClockOffset, rssi, extended_inquiry_data);
  send_event_(std::move(extended_inquiry_result));
}

DualModeController::DualModeController()
    : state_(kStandby),
      test_channel_state_(kNone),
      properties_(kControllerPropertiesFile) {
#define SET_HANDLER(opcode, method) \
  active_hci_commands_[opcode] =    \
      std::bind(&DualModeController::method, this, std::placeholders::_1);
  SET_HANDLER(HCI_RESET, HciReset);
  SET_HANDLER(HCI_READ_BUFFER_SIZE, HciReadBufferSize);
  SET_HANDLER(HCI_HOST_BUFFER_SIZE, HciHostBufferSize);
  SET_HANDLER(HCI_READ_LOCAL_VERSION_INFO, HciReadLocalVersionInformation);
  SET_HANDLER(HCI_READ_BD_ADDR, HciReadBdAddr);
  SET_HANDLER(HCI_READ_LOCAL_SUPPORTED_CMDS, HciReadLocalSupportedCommands);
  SET_HANDLER(HCI_READ_LOCAL_EXT_FEATURES, HciReadLocalExtendedFeatures);
  SET_HANDLER(HCI_WRITE_SIMPLE_PAIRING_MODE, HciWriteSimplePairingMode);
  SET_HANDLER(HCI_WRITE_LE_HOST_SUPPORT, HciWriteLeHostSupport);
  SET_HANDLER(HCI_SET_EVENT_MASK, HciSetEventMask);
  SET_HANDLER(HCI_WRITE_INQUIRY_MODE, HciWriteInquiryMode);
  SET_HANDLER(HCI_WRITE_PAGESCAN_TYPE, HciWritePageScanType);
  SET_HANDLER(HCI_WRITE_INQSCAN_TYPE, HciWriteInquiryScanType);
  SET_HANDLER(HCI_WRITE_CLASS_OF_DEVICE, HciWriteClassOfDevice);
  SET_HANDLER(HCI_WRITE_PAGE_TOUT, HciWritePageTimeout);
  SET_HANDLER(HCI_WRITE_DEF_POLICY_SETTINGS, HciWriteDefaultLinkPolicySettings);
  SET_HANDLER(HCI_READ_LOCAL_NAME, HciReadLocalName);
  SET_HANDLER(HCI_CHANGE_LOCAL_NAME, HciWriteLocalName);
  SET_HANDLER(HCI_WRITE_EXT_INQ_RESPONSE, HciWriteExtendedInquiryResponse);
  SET_HANDLER(HCI_WRITE_VOICE_SETTINGS, HciWriteVoiceSetting);
  SET_HANDLER(HCI_WRITE_CURRENT_IAC_LAP, HciWriteCurrentIacLap);
  SET_HANDLER(HCI_WRITE_INQUIRYSCAN_CFG, HciWriteInquiryScanActivity);
  SET_HANDLER(HCI_WRITE_SCAN_ENABLE, HciWriteScanEnable);
  SET_HANDLER(HCI_SET_EVENT_FILTER, HciSetEventFilter);
  SET_HANDLER(HCI_INQUIRY, HciInquiry);
  SET_HANDLER(HCI_INQUIRY_CANCEL, HciInquiryCancel);
  SET_HANDLER(HCI_DELETE_STORED_LINK_KEY, HciDeleteStoredLinkKey);
  SET_HANDLER(HCI_RMT_NAME_REQUEST, HciRemoteNameRequest);
#undef SET_HANDLER

#define SET_TEST_HANDLER(command_name, method)  \
  active_test_channel_commands_[command_name] = \
      std::bind(&DualModeController::method, this, std::placeholders::_1);
  SET_TEST_HANDLER("CLEAR", TestChannelClear);
  SET_TEST_HANDLER("CLEAR_EVENT_DELAY", TestChannelClearEventDelay);
  SET_TEST_HANDLER("DISCOVER", TestChannelDiscover);
  SET_TEST_HANDLER("SET_EVENT_DELAY", TestChannelSetEventDelay);
  SET_TEST_HANDLER("TIMEOUT_ALL", TestChannelTimeoutAll);
#undef SET_TEST_HANDLER
}

void DualModeController::RegisterHandlersWithHciTransport(
    HciTransport& transport) {
  transport.RegisterCommandHandler(std::bind(&DualModeController::HandleCommand,
                                             this, std::placeholders::_1));
}

void DualModeController::RegisterHandlersWithTestChannelTransport(
    TestChannelTransport& transport) {
  transport.RegisterCommandHandler(
      std::bind(&DualModeController::HandleTestChannelCommand, this,
                std::placeholders::_1, std::placeholders::_2));
}

void DualModeController::HandleTestChannelCommand(
    const std::string& name, const std::vector<std::string>& args) {
  if (active_test_channel_commands_.count(name) == 0)
    return;
  active_test_channel_commands_[name](args);
}

void DualModeController::HandleCommand(
    std::unique_ptr<CommandPacket> command_packet) {
  uint16_t opcode = command_packet->GetOpcode();
  LOG_INFO(LOG_TAG, "Command opcode: 0x%04X, OGF: 0x%04X, OCF: 0x%04X", opcode,
           command_packet->GetOGF(), command_packet->GetOCF());

  // The command hasn't been registered with the handler yet. There is nothing
  // to do.
  if (active_hci_commands_.count(opcode) == 0)
    return;
  else if (test_channel_state_ == kTimeoutAll)
    return;
  active_hci_commands_[opcode](command_packet->GetPayload());
}

void DualModeController::RegisterEventChannel(
    std::function<void(std::unique_ptr<EventPacket>)> callback) {
  send_event_ = callback;
}

void DualModeController::RegisterDelayedEventChannel(
    std::function<void(std::unique_ptr<EventPacket>, base::TimeDelta)>
        callback) {
  send_delayed_event_ = callback;
  SetEventDelay(0);
}

void DualModeController::SetEventDelay(int64_t delay) {
  if (delay < 0)
    delay = 0;
  send_event_ = std::bind(send_delayed_event_, std::placeholders::_1,
                          base::TimeDelta::FromMilliseconds(delay));
}

void DualModeController::TestChannelClear(
    const std::vector<std::string>& args) {
  LogCommand("TestChannel Clear");
  test_channel_state_ = kNone;
  SetEventDelay(0);
}

void DualModeController::TestChannelDiscover(
    const std::vector<std::string>& args) {
  LogCommand("TestChannel Discover");
  for (size_t i = 0; i < args.size()-1; i+=2)
    SendExtendedInquiryResult(args[i], args[i+1]);
}

void DualModeController::TestChannelTimeoutAll(
    const std::vector<std::string>& args) {
  LogCommand("TestChannel Timeout All");
  test_channel_state_ = kTimeoutAll;
}

void DualModeController::TestChannelSetEventDelay(
    const std::vector<std::string>& args) {
  LogCommand("TestChannel Set Event Delay");
  test_channel_state_ = kDelayedResponse;
  SetEventDelay(std::stoi(args[0]));
}

void DualModeController::TestChannelClearEventDelay(
    const std::vector<std::string>& args) {
  LogCommand("TestChannel Clear Event Delay");
  test_channel_state_ = kNone;
  SetEventDelay(0);
}

void DualModeController::HciReset(const std::vector<uint8_t>& /* args */) {
  LogCommand("Reset");
  state_ = kStandby;
  SendCommandCompleteSuccess(HCI_RESET);
}

void DualModeController::HciReadBufferSize(
    const std::vector<uint8_t>& /* args */) {
  LogCommand("Read Buffer Size");
  SendCommandComplete(HCI_READ_BUFFER_SIZE, properties_.GetBufferSize());
}

void DualModeController::HciHostBufferSize(
    const std::vector<uint8_t>& /* args */) {
  LogCommand("Host Buffer Size");
  SendCommandCompleteSuccess(HCI_HOST_BUFFER_SIZE);
}

void DualModeController::HciReadLocalVersionInformation(
                 const std::vector<uint8_t>& /* args */) {
  LogCommand("Read Local Version Information");
  SendCommandComplete(HCI_READ_LOCAL_VERSION_INFO,
                      properties_.GetLocalVersionInformation());
}

void DualModeController::HciReadBdAddr(const std::vector<uint8_t>& /* args */) {
  LogCommand("Read Bd Addr");
  std::vector<uint8_t> bd_address_with_status = properties_.GetBdAddress();
  bd_address_with_status.insert(bd_address_with_status.begin(),
                                kSuccessStatus);
  SendCommandComplete(HCI_READ_BD_ADDR, bd_address_with_status);
}

void DualModeController::HciReadLocalSupportedCommands(
    const std::vector<uint8_t>& /* args */) {
  LogCommand("Read Local Supported Commands");
  SendCommandComplete(HCI_READ_LOCAL_SUPPORTED_CMDS,
                      properties_.GetLocalSupportedCommands());
}

void DualModeController::HciReadLocalExtendedFeatures(
    const std::vector<uint8_t>& args) {
  LogCommand("Read Local Extended Features");
  SendCommandComplete(HCI_READ_LOCAL_EXT_FEATURES,
                      properties_.GetLocalExtendedFeatures(args[0]));
}

void DualModeController::HciWriteSimplePairingMode(
    const std::vector<uint8_t>& /* args */) {
  LogCommand("Write Simple Pairing Mode");
  SendCommandCompleteSuccess(HCI_WRITE_SIMPLE_PAIRING_MODE);
}

void DualModeController::HciWriteLeHostSupport(
    const std::vector<uint8_t>& /* args */) {
  LogCommand("Write Le Host Support");
  SendCommandCompleteSuccess(HCI_WRITE_LE_HOST_SUPPORT);
}

void DualModeController::HciSetEventMask(
    const std::vector<uint8_t>& /* args */) {
  LogCommand("Set Event Mask");
  SendCommandCompleteSuccess(HCI_SET_EVENT_MASK);
}

void DualModeController::HciWriteInquiryMode(const std::vector<uint8_t>& args) {
  LogCommand("Write Inquiry Mode");
  CHECK(args.size() == 1);
  inquiry_mode_ = args[0];
  SendCommandCompleteSuccess(HCI_WRITE_INQUIRY_MODE);
}

void DualModeController::HciWritePageScanType(
    const std::vector<uint8_t>& /* args */) {
  LogCommand("Write Page Scan Type");
  SendCommandCompleteSuccess(HCI_WRITE_PAGESCAN_TYPE);
}

void DualModeController::HciWriteInquiryScanType(
    const std::vector<uint8_t>& /* args */) {
  LogCommand("Write Inquiry Scan Type");
  SendCommandCompleteSuccess(HCI_WRITE_INQSCAN_TYPE);
}

void DualModeController::HciWriteClassOfDevice(
    const std::vector<uint8_t>& /* args */) {
  LogCommand("Write Class Of Device");
  SendCommandCompleteSuccess(HCI_WRITE_CLASS_OF_DEVICE);
}

void DualModeController::HciWritePageTimeout(
    const std::vector<uint8_t>& /* args */) {
  LogCommand("Write Page Timeout");
  SendCommandCompleteSuccess(HCI_WRITE_PAGE_TOUT);
}

void DualModeController::HciWriteDefaultLinkPolicySettings(
    const std::vector<uint8_t>& /* args */) {
  LogCommand("Write Default Link Policy Settings");
  SendCommandCompleteSuccess(HCI_WRITE_DEF_POLICY_SETTINGS);
}

void DualModeController::HciReadLocalName(
    const std::vector<uint8_t>& /* args */) {
  LogCommand("Get Local Name");
  SendCommandComplete(HCI_READ_LOCAL_NAME, properties_.GetLocalName());
}

void DualModeController::HciWriteLocalName(
    const std::vector<uint8_t>& /* args */) {
  LogCommand("Write Local Name");
  SendCommandCompleteSuccess(HCI_CHANGE_LOCAL_NAME);
}

void DualModeController::HciWriteExtendedInquiryResponse(
    const std::vector<uint8_t>& /* args */) {
  LogCommand("Write Extended Inquiry Response");
  SendCommandCompleteSuccess(HCI_WRITE_EXT_INQ_RESPONSE);
}

void DualModeController::HciWriteVoiceSetting(
    const std::vector<uint8_t>& /* args */) {
  LogCommand("Write Voice Setting");
  SendCommandCompleteSuccess(HCI_WRITE_VOICE_SETTINGS);
}

void DualModeController::HciWriteCurrentIacLap(
    const std::vector<uint8_t>& /* args */) {
  LogCommand("Write Current IAC LAP");
  SendCommandCompleteSuccess(HCI_WRITE_CURRENT_IAC_LAP);
}

void DualModeController::HciWriteInquiryScanActivity(
    const std::vector<uint8_t>& /* args */) {
  LogCommand("Write Inquiry Scan Activity");
  SendCommandCompleteSuccess(HCI_WRITE_INQUIRYSCAN_CFG);
}

void DualModeController::HciWriteScanEnable(
    const std::vector<uint8_t>& /* args */) {
  LogCommand("Write Scan Enable");
  SendCommandCompleteSuccess(HCI_WRITE_SCAN_ENABLE);
}

void DualModeController::HciSetEventFilter(
    const std::vector<uint8_t>& /* args */) {
  LogCommand("Set Event Filter");
  SendCommandCompleteSuccess(HCI_SET_EVENT_FILTER);
}

void DualModeController::HciInquiry(const std::vector<uint8_t>& /* args */) {
  LogCommand("Inquiry");
  state_ = kInquiry;
  SendCommandStatusSuccess(HCI_INQUIRY);
  switch (inquiry_mode_) {
    case (kStandardInquiry):
      SendInquiryResult();
      break;

    case (kRssiInquiry):
      LOG_INFO(LOG_TAG, "RSSI Inquiry Mode currently not supported.");
      break;

    case (kExtendedOrRssiInquiry):
      SendExtendedInquiryResult("FooBar", "123456");
      break;
  }
}

void DualModeController::HciInquiryCancel(
    const std::vector<uint8_t>& /* args */) {
  LogCommand("Inquiry Cancel");
  CHECK(state_ == kInquiry);
  state_ = kStandby;
  SendCommandCompleteSuccess(HCI_INQUIRY_CANCEL);
}

void DualModeController::HciDeleteStoredLinkKey(
    const std::vector<uint8_t>& args) {
  LogCommand("Delete Stored Link Key");
  /* Check the last octect in |args|. If it is 0, delete only the link key for
   * the given BD_ADDR. If is is 1, delete all stored link keys. */
  SendCommandComplete(HCI_DELETE_STORED_LINK_KEY, {1});
}

void DualModeController::HciRemoteNameRequest(
    const std::vector<uint8_t>& args) {
  LogCommand("Remote Name Request");
  SendCommandStatusSuccess(HCI_RMT_NAME_REQUEST);
}

DualModeController::Properties::Properties(const std::string& file_name)
    : local_supported_commands_size_(64), local_name_size_(248) {
  std::string properties_raw;
  if (!base::ReadFileToString(base::FilePath(file_name), &properties_raw))
    LOG_INFO(LOG_TAG, "Error reading controller properties from file.");

  scoped_ptr<base::Value> properties_value_ptr =
      base::JSONReader::Read(properties_raw);
  if (properties_value_ptr.get() == nullptr)
    LOG_INFO(LOG_TAG,
             "Error controller properties may consist of ill-formed JSON.");

  // Get the underlying base::Value object, which is of type
  // base::Value::TYPE_DICTIONARY, and read it into member variables.
  base::Value& properties_dictionary = *(properties_value_ptr.get());
  base::JSONValueConverter<DualModeController::Properties> converter;

  if (!converter.Convert(properties_dictionary, this))
    LOG_INFO(LOG_TAG,
             "Error converting JSON properties into Properties object.");
}

const std::vector<uint8_t> DualModeController::Properties::GetBufferSize() {
  return std::vector<uint8_t>(
      {kSuccessStatus, acl_data_packet_size_, acl_data_packet_size_ >> 8,
       sco_data_packet_size_, num_acl_data_packets_, num_acl_data_packets_ >> 8,
       num_sco_data_packets_, num_sco_data_packets_ >> 8});
}

const std::vector<uint8_t>
DualModeController::Properties::GetLocalVersionInformation() {
  return std::vector<uint8_t>({kSuccessStatus, version_, revision_,
                               revision_ >> 8, lmp_pal_version_,
                               manufacturer_name_, manufacturer_name_ >> 8,
                               lmp_pal_subversion_, lmp_pal_subversion_ >> 8});
}

const std::vector<uint8_t> DualModeController::Properties::GetBdAddress() {
  return bd_address_;
}

const std::vector<uint8_t>
DualModeController::Properties::GetLocalExtendedFeatures(uint8_t page_number) {
  return std::vector<uint8_t>({kSuccessStatus, page_number,
                               maximum_page_number_, 0xFF, 0xFF, 0xFF, 0xFF,
                               0xFF, 0xFF, 0xFF, 0xFF});
}

const std::vector<uint8_t>
DualModeController::Properties::GetLocalSupportedCommands() {
  std::vector<uint8_t> local_supported_commands;
  local_supported_commands.push_back(kSuccessStatus);
  for (uint8_t i = 0; i < local_supported_commands_size_; ++i)
    local_supported_commands.push_back(0xFF);
  return local_supported_commands;
}

const std::vector<uint8_t> DualModeController::Properties::GetLocalName() {
  std::vector<uint8_t> local_name;
  local_name.push_back(kSuccessStatus);
  for (uint8_t i = 0; i < local_name_size_; ++i)
    local_name.push_back(0xFF);
  return local_name;
}

// static
void DualModeController::Properties::RegisterJSONConverter(
    base::JSONValueConverter<DualModeController::Properties>* converter) {
  // TODO(dennischeng): Use RegisterIntField() here?
#define REGISTER_UINT8_T(field_name, field) \
  converter->RegisterCustomField<uint8_t>(  \
      field_name, &DualModeController::Properties::field, &ParseUint8t);
#define REGISTER_UINT16_T(field_name, field) \
  converter->RegisterCustomField<uint16_t>(  \
      field_name, &DualModeController::Properties::field, &ParseUint16t);
  REGISTER_UINT16_T("AclDataPacketSize", acl_data_packet_size_);
  REGISTER_UINT8_T("ScoDataPacketSize", sco_data_packet_size_);
  REGISTER_UINT16_T("NumAclDataPackets", num_acl_data_packets_);
  REGISTER_UINT16_T("NumScoDataPackets", num_sco_data_packets_);
  REGISTER_UINT8_T("Version", version_);
  REGISTER_UINT16_T("Revision", revision_);
  REGISTER_UINT8_T("LmpPalVersion", lmp_pal_version_);
  REGISTER_UINT16_T("ManufacturerName", manufacturer_name_);
  REGISTER_UINT16_T("LmpPalSubversion", lmp_pal_subversion_);
  REGISTER_UINT8_T("MaximumPageNumber", maximum_page_number_);
  converter->RegisterCustomField<std::vector<uint8_t>>(
      "BdAddress", &DualModeController::Properties::bd_address_,
      &ParseUint8tVector);
#undef REGISTER_UINT8_T
#undef REGISTER_UINT16_T
}

}  // namespace test_vendor_lib
