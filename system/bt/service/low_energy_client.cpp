//
//  Copyright (C) 2015 Google, Inc.
//
//  Licensed under the Apache License, Version 2.0 (the "License");
//  you may not use this file except in compliance with the License.
//  You may obtain a copy of the License at:
//
//  http://www.apache.org/licenses/LICENSE-2.0
//
//  Unless required by applicable law or agreed to in writing, software
//  distributed under the License is distributed on an "AS IS" BASIS,
//  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//  See the License for the specific language governing permissions and
//  limitations under the License.
//

#include "service/low_energy_client.h"

#include <base/logging.h>

#include "service/adapter.h"
#include "service/common/bluetooth/util/address_helper.h"
#include "service/logging_helpers.h"
#include "stack/include/bt_types.h"
#include "stack/include/hcidefs.h"

using std::lock_guard;
using std::mutex;

namespace bluetooth {

namespace {

// 31 + 31 for advertising data and scan response. This is the maximum length
// TODO(armansito): Fix the HAL to return a concatenated blob that contains the
// true length of each field and also provide a length parameter so that we
// can support advertising length extensions in the future.
const size_t kScanRecordLength = 62;

BLEStatus GetBLEStatus(int status) {
  if (status == BT_STATUS_FAIL)
    return BLE_STATUS_FAILURE;

  return static_cast<BLEStatus>(status);
}

// Returns the length of the given scan record array. We have to calculate this
// based on the maximum possible data length and the TLV data. See TODO above
// |kScanRecordLength|.
size_t GetScanRecordLength(uint8_t* bytes) {
  for (size_t i = 0, field_len = 0; i < kScanRecordLength;
       i += (field_len + 1)) {
    field_len = bytes[i];

    // Assert here that the data returned from the stack is correctly formatted
    // in TLV form and that the length of the current field won't exceed the
    // total data length.
    CHECK(i + field_len < kScanRecordLength);

    // If the field length is zero and we haven't reached the maximum length,
    // then we have found the length, as the stack will pad the data with zeros
    // accordingly.
    if (field_len == 0)
      return i;
  }

  // We have reached the end.
  return kScanRecordLength;
}

// TODO(armansito): BTIF currently expects each advertising field in a
// specific format passed directly in arguments. We should fix BTIF to accept
// the advertising data directly instead.
struct HALAdvertiseData {
  std::vector<uint8_t> manufacturer_data;
  std::vector<uint8_t> service_data;
  std::vector<uint8_t> service_uuid;
};

bool ProcessUUID(const uint8_t* uuid_data, size_t uuid_len, UUID* out_uuid) {
  // BTIF expects a single 128-bit UUID to be passed in little-endian form, so
  // we need to convert into that from raw data.
  // TODO(armansito): We have three repeated if bodies below only because UUID
  // accepts std::array which requires constexpr lengths. We should just have a
  // single UUID constructor that takes in an std::vector instead.
  if (uuid_len == UUID::kNumBytes16) {
    UUID::UUID16Bit uuid_bytes;
    for (size_t i = 0; i < uuid_len; ++i)
      uuid_bytes[uuid_len - i - 1] = uuid_data[i];
    *out_uuid = UUID(uuid_bytes);
  } else if (uuid_len == UUID::kNumBytes32) {
    UUID::UUID32Bit uuid_bytes;
    for (size_t i = 0; i < uuid_len; ++i)
      uuid_bytes[uuid_len - i - 1] = uuid_data[i];
    *out_uuid = UUID(uuid_bytes);
  } else if (uuid_len == UUID::kNumBytes128) {
    UUID::UUID128Bit uuid_bytes;
    for (size_t i = 0; i < uuid_len; ++i)
      uuid_bytes[uuid_len - i - 1] = uuid_data[i];
    *out_uuid = UUID(uuid_bytes);
  } else {
    LOG(ERROR) << "Invalid UUID length";
    return false;
  }

  return true;
}

bool ProcessServiceData(const uint8_t* data,
        uint8_t uuid_len,
        HALAdvertiseData* out_data) {
  size_t field_len = data[0];

  // Minimum packet size should be equal to the uuid length + 1 to include
  // the byte for the type of packet
  if (field_len < uuid_len + 1) {
    // Invalid packet size
    return false;
  }

  if (!out_data->service_data.empty()) {
    // More than one Service Data is not allowed due to the limitations
    // of the HAL API. We error in order to make sure there
    // is no ambiguity on which data to send.
    VLOG(1) << "More than one Service Data entry not allowed";
    return false;
  }

  const uint8_t* service_uuid = data + 2;
  UUID uuid;
  if (!ProcessUUID(service_uuid, uuid_len, &uuid))
    return false;

  UUID::UUID128Bit uuid_bytes = uuid.GetFullLittleEndian();
  const std::vector<uint8_t> temp_uuid(
      uuid_bytes.data(), uuid_bytes.data() + uuid_bytes.size());

  // This section is to make sure that there is no UUID conflict
  if (out_data->service_uuid.empty()) {
    out_data->service_uuid = temp_uuid;
  } else if (out_data->service_uuid != temp_uuid) {
    // Mismatch in uuid passed through service data and uuid passed
    // through uuid field
    VLOG(1) << "More than one UUID entry not allowed";
    return false;
  }  // else do nothing as UUID is already properly assigned

  // Use + uuid_len + 2 here in order to skip over a
  // uuid contained in the beggining of the field
  const uint8_t* srv_data = data + uuid_len + 2;


  out_data->service_data.insert(
      out_data->service_data.begin(),
      srv_data, srv_data + field_len - uuid_len - 1);

  return true;
}

bool ProcessAdvertiseData(const AdvertiseData& adv,
                          HALAdvertiseData* out_data) {
  CHECK(out_data);
  CHECK(out_data->manufacturer_data.empty());
  CHECK(out_data->service_data.empty());
  CHECK(out_data->service_uuid.empty());

  const auto& data = adv.data();
  size_t len = data.size();
  for (size_t i = 0, field_len = 0; i < len; i += (field_len + 1)) {
    // The length byte is the first byte in the adv. "TLV" format.
    field_len = data[i];

    // The type byte is the next byte in the adv. "TLV" format.
    uint8_t type = data[i + 1];

    switch (type) {
    case HCI_EIR_MANUFACTURER_SPECIFIC_TYPE: {
      // TODO(armansito): BTIF doesn't allow setting more than one
      // manufacturer-specific data entry. This is something we should fix. For
      // now, fail if more than one entry was set.
      if (!out_data->manufacturer_data.empty()) {
        LOG(ERROR) << "More than one Manufacturer Specific Data entry not allowed";
        return false;
      }

      // The value bytes start at the next byte in the "TLV" format.
      const uint8_t* mnf_data = data.data() + i + 2;
      out_data->manufacturer_data.insert(
          out_data->manufacturer_data.begin(),
          mnf_data, mnf_data + field_len - 1);
      break;
    }
    case HCI_EIR_MORE_16BITS_UUID_TYPE:
    case HCI_EIR_COMPLETE_16BITS_UUID_TYPE:
    case HCI_EIR_MORE_32BITS_UUID_TYPE:
    case HCI_EIR_COMPLETE_32BITS_UUID_TYPE:
    case HCI_EIR_MORE_128BITS_UUID_TYPE:
    case HCI_EIR_COMPLETE_128BITS_UUID_TYPE: {
      const uint8_t* uuid_data = data.data() + i + 2;
      size_t uuid_len = field_len - 1;
      UUID uuid;
      if (!ProcessUUID(uuid_data, uuid_len, &uuid))
        return false;

      UUID::UUID128Bit uuid_bytes = uuid.GetFullLittleEndian();

      if (!out_data->service_uuid.empty() &&
          memcmp(out_data->service_uuid.data(),
                 uuid_bytes.data(), uuid_bytes.size()) != 0) {
        // More than one UUID is not allowed due to the limitations
        // of the HAL API. We error in order to make sure there
        // is no ambiguity on which UUID to send. Also makes sure that
        // UUID Hasn't been set by service data first
        LOG(ERROR) << "More than one UUID entry not allowed";
        return false;
      }

      out_data->service_uuid.assign(
          uuid_bytes.data(), uuid_bytes.data() + UUID::kNumBytes128);
      break;
    }
    case HCI_EIR_SERVICE_DATA_16BITS_UUID_TYPE: {
      if (!ProcessServiceData(data.data() + i, 2, out_data))
        return false;
      break;
    }
    case HCI_EIR_SERVICE_DATA_32BITS_UUID_TYPE: {
      if (!ProcessServiceData(data.data() + i, 4, out_data))
        return false;
      break;
    }
    case HCI_EIR_SERVICE_DATA_128BITS_UUID_TYPE: {
      if (!ProcessServiceData(data.data() + i, 16, out_data))
        return false;
      break;
    }
    // TODO(armansito): Support other fields.
    default:
      VLOG(1) << "Unrecognized EIR field: " << type;
      return false;
    }
  }

  return true;
}

// The Bluetooth Core Specification defines time interval (e.g. Page Scan
// Interval, Advertising Interval, etc) units as 0.625 milliseconds (or 1
// Baseband slot). The HAL advertising functions expect the interval in this
// unit. This function maps an AdvertiseSettings::Mode value to the
// corresponding time unit.
int GetAdvertisingIntervalUnit(AdvertiseSettings::Mode mode) {
  int ms;

  switch (mode) {
  case AdvertiseSettings::MODE_BALANCED:
    ms = kAdvertisingIntervalMediumMs;
    break;
  case AdvertiseSettings::MODE_LOW_LATENCY:
    ms = kAdvertisingIntervalLowMs;
    break;
  case AdvertiseSettings::MODE_LOW_POWER:
    // Fall through
  default:
    ms = kAdvertisingIntervalHighMs;
    break;
  }

  // Convert milliseconds Bluetooth units.
  return (ms * 1000) / 625;
}

struct AdvertiseParams {
  int min_interval;
  int max_interval;
  int event_type;
  int tx_power_level;
  int timeout_s;
};

void GetAdvertiseParams(const AdvertiseSettings& settings, bool has_scan_rsp,
                        AdvertiseParams* out_params) {
  CHECK(out_params);

  out_params->min_interval = GetAdvertisingIntervalUnit(settings.mode());
  out_params->max_interval =
      out_params->min_interval + kAdvertisingIntervalDeltaUnit;

  if (settings.connectable())
    out_params->event_type = kAdvertisingEventTypeConnectable;
  else if (has_scan_rsp)
    out_params->event_type = kAdvertisingEventTypeScannable;
  else
    out_params->event_type = kAdvertisingEventTypeNonConnectable;

  out_params->tx_power_level = settings.tx_power_level();
  out_params->timeout_s = settings.timeout().InSeconds();
}

}  // namespace

// LowEnergyClient implementation
// ========================================================

LowEnergyClient::LowEnergyClient(
    Adapter& adapter, const UUID& uuid, int client_id)
    : adapter_(adapter),
      app_identifier_(uuid),
      client_id_(client_id),
      adv_data_needs_update_(false),
      scan_rsp_needs_update_(false),
      is_setting_adv_data_(false),
      adv_started_(false),
      adv_start_callback_(nullptr),
      adv_stop_callback_(nullptr),
      scan_started_(false) {
}

LowEnergyClient::~LowEnergyClient() {
  // Automatically unregister the client.
  VLOG(1) << "LowEnergyClient unregistering client: " << client_id_;

  // Unregister as observer so we no longer receive any callbacks.
  hal::BluetoothGattInterface::Get()->RemoveClientObserver(this);

  // Stop advertising and ignore the result.
  hal::BluetoothGattInterface::Get()->
      GetClientHALInterface()->multi_adv_disable(client_id_);
  hal::BluetoothGattInterface::Get()->
      GetClientHALInterface()->unregister_client(client_id_);

  // Stop any scans started by this client.
  if (scan_started_.load())
    StopScan();
}

bool LowEnergyClient::Connect(std::string address, bool is_direct) {
  VLOG(2) << __func__ << "Address: " << address << " is_direct: " << is_direct;

  bt_bdaddr_t bda;
  util::BdAddrFromString(address, &bda);

  bt_status_t status = hal::BluetoothGattInterface::Get()->
      GetClientHALInterface()->connect(client_id_, &bda, is_direct,
                                       BT_TRANSPORT_LE);
  if (status != BT_STATUS_SUCCESS) {
    LOG(ERROR) << "HAL call to connect failed";
    return false;
  }

  return true;
}

bool LowEnergyClient::Disconnect(std::string address) {
  VLOG(2) << __func__ << "Address: " << address;

  bt_bdaddr_t bda;
  util::BdAddrFromString(address, &bda);

  std::map<const bt_bdaddr_t, int>::iterator conn_id;
  {
    lock_guard<mutex> lock(connection_fields_lock_);
    conn_id = connection_ids_.find(bda);
    if (conn_id == connection_ids_.end()) {
      LOG(WARNING) << "Can't disconnect, no existing connection to " << address;
      return false;
    }
  }

  bt_status_t status = hal::BluetoothGattInterface::Get()->
      GetClientHALInterface()->disconnect(client_id_, &bda, conn_id->second);
  if (status != BT_STATUS_SUCCESS) {
    LOG(ERROR) << "HAL call to disconnect failed";
    return false;
  }

  return true;
}

bool LowEnergyClient::SetMtu(std::string address, int mtu) {
  VLOG(2) << __func__ << "Address: " << address
          << " MTU: " << mtu;

  bt_bdaddr_t bda;
  util::BdAddrFromString(address, &bda);

  std::map<const bt_bdaddr_t, int>::iterator conn_id;
  {
    lock_guard<mutex> lock(connection_fields_lock_);
    conn_id = connection_ids_.find(bda);
    if (conn_id == connection_ids_.end()) {
      LOG(WARNING) << "Can't set MTU, no existing connection to " << address;
      return false;
    }
  }

  bt_status_t status = hal::BluetoothGattInterface::Get()->
      GetClientHALInterface()->configure_mtu(conn_id->second, mtu);
  if (status != BT_STATUS_SUCCESS) {
    LOG(ERROR) << "HAL call to set MTU failed";
    return false;
  }

  return true;
}

bool LowEnergyClient::DiscoverServices(std::string address) {
  VLOG(2) << __func__ << "Address: " << address;

  bt_bdaddr_t bda;
  util::BdAddrFromString(address, &bda);

  std::map<const bt_bdaddr_t, int>::iterator conn_id;
  {
    lock_guard<mutex> lock(connection_fields_lock_);
    conn_id = connection_ids_.find(bda);
    if (conn_id == connection_ids_.end()) {
      LOG(WARNING) << "Can't discover services, no existing connection to " << address;
      return false;
    }
  }

  bt_status_t status = hal::BluetoothGattInterface::Get()->
      GetClientHALInterface()->search_service(conn_id->second, nullptr);
  if (status != BT_STATUS_SUCCESS) {
    LOG(ERROR) << "HAL call to search service failed";
    return false;
  }

  return true;
}

bool LowEnergyClient::GetGattDb(std::string address) {
  VLOG(2) << __func__ << "Address: " << address;

  bt_bdaddr_t bda;
  util::BdAddrFromString(address, &bda);

  std::map<const bt_bdaddr_t, int>::iterator conn_id;
  {
    lock_guard<mutex> lock(connection_fields_lock_);
    conn_id = connection_ids_.find(bda);
    if (conn_id == connection_ids_.end()) {
      LOG(WARNING) << "Can't get gatt db, no existing connection to " << address;
      return false;
    }
  }

  bt_status_t status = hal::BluetoothGattInterface::Get()->
      GetClientHALInterface()->get_gatt_db(conn_id->second);
  if (status != BT_STATUS_SUCCESS) {
    LOG(ERROR) << "HAL call to get gatt db failed";
    return false;
  }

  return true;
}

bool LowEnergyClient::ReadCharacteristic(std::string address, int handle) {
  VLOG(2) << __func__ << "Address: " << address
          << " handle: " << handle;

  bt_bdaddr_t bda;
  util::BdAddrFromString(address, &bda);

  std::map<const bt_bdaddr_t, int>::iterator conn_id;
  {
    lock_guard<mutex> lock(connection_fields_lock_);
    conn_id = connection_ids_.find(bda);
    if (conn_id == connection_ids_.end()) {
      LOG(WARNING) << "Can't get gatt db, no existing connection to " << address;
      return false;
    }
  }

  bt_status_t status = hal::BluetoothGattInterface::Get()->
      GetClientHALInterface()->read_characteristic(conn_id->second, handle, 0 /* auth_req */);
  if (status != BT_STATUS_SUCCESS) {
    LOG(ERROR) << "HAL call to read characteristic failed";
    return false;
  }

  return true;
}

bool LowEnergyClient::WriteCharacteristic(std::string address, int handle, int write_type,
                                          const std::vector<uint8_t>& value) {
  VLOG(2) << __func__ << "Address: " << address
          << " handle: " << handle
          << " write_type: " << write_type;

  bt_bdaddr_t bda;
  util::BdAddrFromString(address, &bda);

  std::map<const bt_bdaddr_t, int>::iterator conn_id;
  {
    lock_guard<mutex> lock(connection_fields_lock_);
    conn_id = connection_ids_.find(bda);
    if (conn_id == connection_ids_.end()) {
      LOG(WARNING) << "Can't get gatt db, no existing connection to " << address;
      return false;
    }
  }

  bt_status_t status = hal::BluetoothGattInterface::Get()->
      GetClientHALInterface()->write_characteristic(conn_id->second,
                                                    handle,
                                                    write_type,
                                                    value.size(),
                                                    0 /* auth_req */,
                                                    (char *) value.data());
  if (status != BT_STATUS_SUCCESS) {
    LOG(ERROR) << "HAL call to write characteristic failed";
    return false;
  }

  return true;
}

bool LowEnergyClient::WriteDescriptor(std::string address, int handle, int write_type,
                                      const std::vector<uint8_t>& value) {
  VLOG(2) << __func__ << "Address: " << address
          << " handle: " << handle
          << " write_type: " << write_type;

  bt_bdaddr_t bda;
  util::BdAddrFromString(address, &bda);

  std::map<const bt_bdaddr_t, int>::iterator conn_id;
  {
    lock_guard<mutex> lock(connection_fields_lock_);
    conn_id = connection_ids_.find(bda);
    if (conn_id == connection_ids_.end()) {
      LOG(WARNING) << "Can't get gatt db, no existing connection to " << address;
      return false;
    }
  }

  bt_status_t status = hal::BluetoothGattInterface::Get()->
      GetClientHALInterface()->write_descriptor(conn_id->second,
                                                handle,
                                                write_type,
                                                value.size(),
                                                0 /* auth_req */,
                                                (char *) value.data());
  if (status != BT_STATUS_SUCCESS) {
    LOG(ERROR) << "HAL call to write descriptor failed";
    return false;
  }

  return true;
}

void LowEnergyClient::SetDelegate(Delegate* delegate) {
  lock_guard<mutex> lock(delegate_mutex_);
  delegate_ = delegate;
}

bool LowEnergyClient::StartScan(const ScanSettings& settings,
                                const std::vector<ScanFilter>& filters) {
  VLOG(2) << __func__;

  // Cannot start a scan if the adapter is not enabled.
  if (!adapter_.IsEnabled()) {
    LOG(ERROR) << "Cannot scan while Bluetooth is disabled";
    return false;
  }

  // TODO(jpawlowski): Push settings and filtering logic below the HAL.
  bt_status_t status = hal::BluetoothGattInterface::Get()->
      StartScan(client_id_);
  if (status != BT_STATUS_SUCCESS) {
    LOG(ERROR) << "Failed to initiate scanning for client: " << client_id_;
    return false;
  }

  scan_started_ = true;
  return true;
}

bool LowEnergyClient::StopScan() {
  VLOG(2) << __func__;

  // TODO(armansito): We don't support batch scanning yet so call
  // StopRegularScanForClient directly. In the future we will need to
  // conditionally call a batch scan API here.
  bt_status_t status = hal::BluetoothGattInterface::Get()->
      StopScan(client_id_);
  if (status != BT_STATUS_SUCCESS) {
    LOG(ERROR) << "Failed to stop scan for client: " << client_id_;
    return false;
  }

  scan_started_ = false;
  return true;
}

bool LowEnergyClient::StartAdvertising(const AdvertiseSettings& settings,
                                       const AdvertiseData& advertise_data,
                                       const AdvertiseData& scan_response,
                                       const StatusCallback& callback) {
  VLOG(2) << __func__;
  lock_guard<mutex> lock(adv_fields_lock_);

  if (IsAdvertisingStarted()) {
    LOG(WARNING) << "Already advertising";
    return false;
  }

  if (IsStartingAdvertising()) {
    LOG(WARNING) << "StartAdvertising already pending";
    return false;
  }

  if (!advertise_data.IsValid()) {
    LOG(ERROR) << "Invalid advertising data";
    return false;
  }

  if (!scan_response.IsValid()) {
    LOG(ERROR) << "Invalid scan response data";
    return false;
  }

  CHECK(!adv_data_needs_update_.load());
  CHECK(!scan_rsp_needs_update_.load());

  adv_data_ = advertise_data;
  scan_response_ = scan_response;
  advertise_settings_ = settings;

  AdvertiseParams params;
  GetAdvertiseParams(settings, !scan_response_.data().empty(), &params);

  bt_status_t status = hal::BluetoothGattInterface::Get()->
      GetClientHALInterface()->multi_adv_enable(
          client_id_,
          params.min_interval,
          params.max_interval,
          params.event_type,
          kAdvertisingChannelAll,
          params.tx_power_level,
          params.timeout_s);
  if (status != BT_STATUS_SUCCESS) {
    LOG(ERROR) << "Failed to initiate call to enable multi-advertising";
    return false;
  }

  // Always update advertising data.
  adv_data_needs_update_ = true;

  // Update scan response only if it has data, since otherwise we just won't
  // send ADV_SCAN_IND.
  if (!scan_response_.data().empty())
    scan_rsp_needs_update_ = true;

  // OK to set this at the end since we're still holding |adv_fields_lock_|.
  adv_start_callback_.reset(new StatusCallback(callback));

  return true;
}

bool LowEnergyClient::StopAdvertising(const StatusCallback& callback) {
  VLOG(2) << __func__;
  lock_guard<mutex> lock(adv_fields_lock_);

  if (!IsAdvertisingStarted()) {
    LOG(ERROR) << "Not advertising";
    return false;
  }

  if (IsStoppingAdvertising()) {
    LOG(ERROR) << "StopAdvertising already pending";
    return false;
  }

  CHECK(!adv_start_callback_);

  bt_status_t status = hal::BluetoothGattInterface::Get()->
      GetClientHALInterface()->multi_adv_disable(client_id_);
  if (status != BT_STATUS_SUCCESS) {
    LOG(ERROR) << "Failed to initiate call to disable multi-advertising";
    return false;
  }

  // OK to set this at the end since we're still holding |adv_fields_lock_|.
  adv_stop_callback_.reset(new StatusCallback(callback));

  return true;
}

bool LowEnergyClient::IsAdvertisingStarted() const {
  return adv_started_.load();
}

bool LowEnergyClient::IsStartingAdvertising() const {
  return !IsAdvertisingStarted() && adv_start_callback_;
}

bool LowEnergyClient::IsStoppingAdvertising() const {
  return IsAdvertisingStarted() && adv_stop_callback_;
}

const UUID& LowEnergyClient::GetAppIdentifier() const {
  return app_identifier_;
}

int LowEnergyClient::GetInstanceId() const {
  return client_id_;
}

void LowEnergyClient::ScanResultCallback(
    hal::BluetoothGattInterface* gatt_iface,
    const bt_bdaddr_t& bda, int rssi, uint8_t* adv_data) {
  // Ignore scan results if this client didn't start a scan.
  if (!scan_started_.load())
    return;

  lock_guard<mutex> lock(delegate_mutex_);
  if (!delegate_)
    return;

  // TODO(armansito): Apply software filters here.

  size_t record_len = GetScanRecordLength(adv_data);
  std::vector<uint8_t> scan_record(adv_data, adv_data + record_len);

  ScanResult result(BtAddrString(&bda), scan_record, rssi);

  delegate_->OnScanResult(this, result);
}

void LowEnergyClient::ConnectCallback(
      hal::BluetoothGattInterface* gatt_iface, int conn_id, int status,
      int client_id, const bt_bdaddr_t& bda) {
  if (client_id != client_id_)
    return;

  VLOG(1) << __func__ << "client_id: " << client_id
          << " conn_id: " << conn_id
          << " status: " << status;

  {
    lock_guard<mutex> lock(connection_fields_lock_);
    auto success = connection_ids_.emplace(bda, conn_id);
    if (!success.second) {
      LOG(ERROR) << __func__ << " Insertion into connection_ids_ failed!";
    }
  }

  if (delegate_)
    delegate_->OnConnectionState(this, status, BtAddrString(&bda).c_str(),
                                 true);
}

void LowEnergyClient::DisconnectCallback(
      hal::BluetoothGattInterface* gatt_iface, int conn_id, int status,
      int client_id, const bt_bdaddr_t& bda) {
  if (client_id != client_id_)
    return;

  VLOG(1) << __func__ << " client_id: " << client_id << " status: " << status;
  {
    lock_guard<mutex> lock(connection_fields_lock_);
    if (!connection_ids_.erase(bda)) {
      LOG(ERROR) << __func__ << " Erasing from connection_ids_ failed!";
    }
  }

  if (delegate_)
    delegate_->OnConnectionState(this, status, BtAddrString(&bda).c_str(),
                                 false);
}

void LowEnergyClient::MtuChangedCallback(
      hal::BluetoothGattInterface* gatt_iface, int conn_id, int status,
      int mtu) {
  VLOG(1) << __func__ << " conn_id: " << conn_id << " status: " << status
          << " mtu: " << mtu;

  const bt_bdaddr_t *bda = nullptr;
  {
    lock_guard<mutex> lock(connection_fields_lock_);
    for (auto& connection: connection_ids_) {
      if (connection.second == conn_id) {
        bda = &connection.first;
        break;
      }
    }
  }

  if (!bda)
    return;

  const char *addr = BtAddrString(bda).c_str();
  if (delegate_)
    delegate_->OnMtuChanged(this, status, addr, mtu);
}

void LowEnergyClient::SearchCompleteCallback(
  hal::BluetoothGattInterface* gatt_iface, int conn_id, int status) {

  VLOG(1) << __func__ << " conn_id: " << conn_id << " status: " << status;

  const bt_bdaddr_t *bda = nullptr;
  {
    lock_guard<mutex> lock(connection_fields_lock_);
    for (auto& connection: connection_ids_) {
      if (connection.second == conn_id) {
        bda = &connection.first;
        break;
      }
    }
  }

  if (!bda)
    return;

  const char *addr = BtAddrString(bda).c_str();
  if (delegate_)
    delegate_->OnServicesDiscovered(this, status, addr);
}


void LowEnergyClient::GetGattDbCallback(
     hal::BluetoothGattInterface* gatt_iface, int conn_id,
     btgatt_db_element_t *db, int size) {

  VLOG(1) << __func__ << " conn_id: " << conn_id << " size: " << size;

  const bt_bdaddr_t *bda = nullptr;
  {
    lock_guard<mutex> lock(connection_fields_lock_);
    for (auto& connection: connection_ids_) {
      if (connection.second == conn_id) {
        bda = &connection.first;
        break;
      }
    }
  }

  if (!bda)
    return;

  const char *addr = BtAddrString(bda).c_str();
  if (delegate_)
    delegate_->OnGattDbUpdated(this, addr, db, size);
}

void LowEnergyClient::ReadCharacteristicCallback(
    hal::BluetoothGattInterface* gatt_iface, int conn_id, int status,
    btgatt_read_params_t *data) {

  VLOG(1) << __func__ << " conn_id: " << conn_id;

  const bt_bdaddr_t *bda = nullptr;
  {
    lock_guard<mutex> lock(connection_fields_lock_);
    for (auto& connection: connection_ids_) {
      if (connection.second == conn_id) {
        bda = &connection.first;
        break;
      }
    }
  }

  if (!bda)
    return;

  const char *addr = BtAddrString(bda).c_str();
  if (delegate_)
    delegate_->OnCharacteristicRead(this, addr, status, data);
}

void LowEnergyClient::WriteCharacteristicCallback(
    hal::BluetoothGattInterface* gatt_iface, int conn_id, int status,
    uint16_t handle) {

  VLOG(1) << __func__ << " conn_id: " << conn_id;

  const bt_bdaddr_t *bda = nullptr;
  {
    lock_guard<mutex> lock(connection_fields_lock_);
    for (auto& connection: connection_ids_) {
      if (connection.second == conn_id) {
        bda = &connection.first;
        break;
      }
    }
  }

  if (!bda)
    return;

  const char *addr = BtAddrString(bda).c_str();
  if (delegate_)
    delegate_->OnCharacteristicWrite(this, addr, status, handle);
}

void LowEnergyClient::WriteDescriptorCallback(
    hal::BluetoothGattInterface* gatt_iface, int conn_id, int status,
    uint16_t handle) {

  VLOG(1) << __func__ << " conn_id: " << conn_id;

  const bt_bdaddr_t *bda = nullptr;
  {
    lock_guard<mutex> lock(connection_fields_lock_);
    for (auto& connection: connection_ids_) {
      if (connection.second == conn_id) {
        bda = &connection.first;
        break;
      }
    }
  }

  if (!bda)
    return;

  const char *addr = BtAddrString(bda).c_str();
  if (delegate_)
    delegate_->OnDescriptorWrite(this, addr, status, handle);
}

void LowEnergyClient::RegisterForNotificationCallback(
    hal::BluetoothGattInterface* gatt_iface, int conn_id, int registered,
    int status, uint16_t handle) {

  VLOG(1) << __func__ << " conn_id: " << conn_id;

  const bt_bdaddr_t *bda = nullptr;
  {
    lock_guard<mutex> lock(connection_fields_lock_);
    for (auto& connection: connection_ids_) {
      if (connection.second == conn_id) {
        bda = &connection.first;
        break;
      }
    }
  }

  if (!bda)
    return;

  const char *addr = BtAddrString(bda).c_str();
  if (delegate_)
    delegate_->OnCharacteristicNotificationRegistration(this, addr,
                                                        registered, status,
                                                        handle);
}

void LowEnergyClient::NotifyCallback(
    hal::BluetoothGattInterface* gatt_iface, int conn_id,
    btgatt_notify_params_t* notification) {

  VLOG(1) << __func__ << " conn_id: " << conn_id
          << " address: " << BtAddrString(&(notification->bda));

  const bt_bdaddr_t *bda = nullptr;
  {
    lock_guard<mutex> lock(connection_fields_lock_);
    for (auto& connection: connection_ids_) {
      if (connection.second == conn_id) {
        bda = &connection.first;
        break;
      }
    }
    if (!bda) {
      std::map<const bt_bdaddr_t, int>::iterator conn_id_itr;
      conn_id_itr = connection_ids_.find(notification->bda);
      if (conn_id_itr != connection_ids_.end()) {
        bda = &(notification->bda);
      }
    }
  }

  if (!bda)
    return;

  const char *addr = BtAddrString(bda).c_str();
  if (delegate_)
    delegate_->OnCharacteristicChanged(this, addr, notification);
}

void LowEnergyClient::MultiAdvEnableCallback(
    hal::BluetoothGattInterface* gatt_iface,
    int client_id, int status) {
  if (client_id != client_id_)
    return;

  lock_guard<mutex> lock(adv_fields_lock_);

  VLOG(1) << __func__ << "client_id: " << client_id << " status: " << status;

  CHECK(adv_start_callback_);
  CHECK(!adv_stop_callback_);

  // Terminate operation in case of error.
  if (status != BT_STATUS_SUCCESS) {
    LOG(ERROR) << "Failed to enable multi-advertising";
    InvokeAndClearStartCallback(GetBLEStatus(status));
    return;
  }

  // Now handle deferred tasks.
  HandleDeferredAdvertiseData(gatt_iface);
}

void LowEnergyClient::MultiAdvDataCallback(
    hal::BluetoothGattInterface* gatt_iface,
    int client_id, int status) {
  if (client_id != client_id_)
    return;

  lock_guard<mutex> lock(adv_fields_lock_);

  VLOG(1) << __func__ << "client_id: " << client_id << " status: " << status;

  is_setting_adv_data_ = false;

  // Terminate operation in case of error.
  if (status != BT_STATUS_SUCCESS) {
    LOG(ERROR) << "Failed to set advertising data";
    InvokeAndClearStartCallback(GetBLEStatus(status));
    return;
  }

  // Now handle deferred tasks.
  HandleDeferredAdvertiseData(gatt_iface);
}

void LowEnergyClient::MultiAdvDisableCallback(
    hal::BluetoothGattInterface* /* gatt_iface */,
    int client_id, int status) {
  if (client_id != client_id_)
    return;

  lock_guard<mutex> lock(adv_fields_lock_);

  VLOG(1) << __func__ << "client_id: " << client_id << " status: " << status;

  CHECK(!adv_start_callback_);
  CHECK(adv_stop_callback_);

  if (status == BT_STATUS_SUCCESS) {
    VLOG(1) << "Multi-advertising stopped for client_id: " << client_id;
    adv_started_ = false;
  } else {
    LOG(ERROR) << "Failed to stop multi-advertising";
  }

  InvokeAndClearStopCallback(GetBLEStatus(status));
}

bt_status_t LowEnergyClient::SetAdvertiseData(
    hal::BluetoothGattInterface* gatt_iface,
    const AdvertiseData& data,
    bool set_scan_rsp) {
  VLOG(2) << __func__;

  HALAdvertiseData hal_data;

  // TODO(armansito): The stack should check that the length is valid when other
  // fields inserted by the stack (e.g. flags, device name, tx-power) are taken
  // into account. At the moment we are skipping this check; this means that if
  // the given data is too long then the stack will truncate it.
  if (!ProcessAdvertiseData(data, &hal_data)) {
    LOG(ERROR) << "Malformed advertise data given";
    return BT_STATUS_FAIL;
  }

  if (is_setting_adv_data_.load()) {
    LOG(ERROR) << "Setting advertising data already in progress.";
    return BT_STATUS_FAIL;
  }

  // TODO(armansito): The length fields in the BTIF function below are signed
  // integers so a call to std::vector::size might get capped. This is very
  // unlikely anyway but it's safer to stop using signed-integer types for
  // length in APIs, so we should change that.
  bt_status_t status = gatt_iface->GetClientHALInterface()->
      multi_adv_set_inst_data(
          client_id_,
          set_scan_rsp,
          data.include_device_name(),
          data.include_tx_power_level(),
          0,  // This is what Bluetooth.apk current hardcodes for "appearance".
          hal_data.manufacturer_data.size(),
          reinterpret_cast<char*>(hal_data.manufacturer_data.data()),
          hal_data.service_data.size(),
          reinterpret_cast<char*>(hal_data.service_data.data()),
          hal_data.service_uuid.size(),
          reinterpret_cast<char*>(hal_data.service_uuid.data()));

  if (status != BT_STATUS_SUCCESS) {
    LOG(ERROR) << "Failed to set instance advertising data.";
    return status;
  }

  if (set_scan_rsp)
    scan_rsp_needs_update_ = false;
  else
    adv_data_needs_update_ = false;

  is_setting_adv_data_ = true;

  return status;
}

void LowEnergyClient::HandleDeferredAdvertiseData(
    hal::BluetoothGattInterface* gatt_iface) {
  VLOG(2) << __func__;

  CHECK(!IsAdvertisingStarted());
  CHECK(!IsStoppingAdvertising());
  CHECK(IsStartingAdvertising());
  CHECK(!is_setting_adv_data_.load());

  if (adv_data_needs_update_.load()) {
    bt_status_t status = SetAdvertiseData(gatt_iface, adv_data_, false);
    if (status != BT_STATUS_SUCCESS) {
      LOG(ERROR) << "Failed setting advertisement data";
      InvokeAndClearStartCallback(GetBLEStatus(status));
    }
    return;
  }

  if (scan_rsp_needs_update_.load()) {
    bt_status_t status = SetAdvertiseData(gatt_iface, scan_response_, true);
    if (status != BT_STATUS_SUCCESS) {
      LOG(ERROR) << "Failed setting scan response data";
      InvokeAndClearStartCallback(GetBLEStatus(status));
    }
    return;
  }

  // All pending tasks are complete. Report success.
  adv_started_ = true;
  InvokeAndClearStartCallback(BLE_STATUS_SUCCESS);
}

void LowEnergyClient::InvokeAndClearStartCallback(BLEStatus status) {
  adv_data_needs_update_ = false;
  scan_rsp_needs_update_ = false;

  // We allow NULL callbacks.
  if (*adv_start_callback_)
    (*adv_start_callback_)(status);

  adv_start_callback_ = nullptr;
}

void LowEnergyClient::InvokeAndClearStopCallback(BLEStatus status) {
  // We allow NULL callbacks.
  if (*adv_stop_callback_)
    (*adv_stop_callback_)(status);

  adv_stop_callback_ = nullptr;
}

// LowEnergyClientFactory implementation
// ========================================================

LowEnergyClientFactory::LowEnergyClientFactory(Adapter& adapter)
    : adapter_(adapter) {
  hal::BluetoothGattInterface::Get()->AddClientObserver(this);
}

LowEnergyClientFactory::~LowEnergyClientFactory() {
  hal::BluetoothGattInterface::Get()->RemoveClientObserver(this);
}

bool LowEnergyClientFactory::RegisterInstance(
    const UUID& uuid,
    const RegisterCallback& callback) {
  VLOG(1) << __func__ << " - UUID: " << uuid.ToString();
  lock_guard<mutex> lock(pending_calls_lock_);

  if (pending_calls_.find(uuid) != pending_calls_.end()) {
    LOG(ERROR) << "Low-Energy client with given UUID already registered - "
               << "UUID: " << uuid.ToString();
    return false;
  }

  const btgatt_client_interface_t* hal_iface =
      hal::BluetoothGattInterface::Get()->GetClientHALInterface();
  bt_uuid_t app_uuid = uuid.GetBlueDroid();

  if (hal_iface->register_client(&app_uuid) != BT_STATUS_SUCCESS)
    return false;

  pending_calls_[uuid] = callback;

  return true;
}

void LowEnergyClientFactory::RegisterClientCallback(
    hal::BluetoothGattInterface* gatt_iface,
    int status, int client_id,
    const bt_uuid_t& app_uuid) {
  UUID uuid(app_uuid);

  VLOG(1) << __func__ << " - UUID: " << uuid.ToString();
  lock_guard<mutex> lock(pending_calls_lock_);

  auto iter = pending_calls_.find(uuid);
  if (iter == pending_calls_.end()) {
    VLOG(1) << "Ignoring callback for unknown app_id: " << uuid.ToString();
    return;
  }

  // No need to construct a client if the call wasn't successful.
  std::unique_ptr<LowEnergyClient> client;
  BLEStatus result = BLE_STATUS_FAILURE;
  if (status == BT_STATUS_SUCCESS) {
    client.reset(new LowEnergyClient(adapter_, uuid, client_id));

    gatt_iface->AddClientObserver(client.get());

    result = BLE_STATUS_SUCCESS;
  }

  // Notify the result via the result callback.
  iter->second(result, uuid, std::move(client));

  pending_calls_.erase(iter);
}

}  // namespace bluetooth
