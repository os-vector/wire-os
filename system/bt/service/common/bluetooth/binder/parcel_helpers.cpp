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

#include "service/common/bluetooth/binder/parcel_helpers.h"

#include "service/common/bluetooth/util/address_helper.h"

using android::Parcel;

using bluetooth::AdvertiseData;
using bluetooth::AdvertiseSettings;
using bluetooth::GattIdentifier;
using bluetooth::ScanFilter;
using bluetooth::ScanResult;
using bluetooth::ScanSettings;
using bluetooth::UUID;

namespace ipc {
namespace binder {

// TODO(armansito): The helpers below currently don't match the Java
// definitions. We need to change the AIDL and framework code to comply with the
// new definition and Parcel format provided here.

void WriteAdvertiseDataToParcel(const AdvertiseData& data, Parcel* parcel) {
  CHECK(parcel);
  parcel->writeByteVector(data.data());
  parcel->writeInt32(data.include_device_name());
  parcel->writeInt32(data.include_tx_power_level());
}

std::unique_ptr<AdvertiseData> CreateAdvertiseDataFromParcel(
    const Parcel& parcel) {
  std::unique_ptr<std::vector<uint8_t>> data;
  parcel.readByteVector(&data);
  CHECK(data.get());

  bool include_device_name = parcel.readInt32();
  bool include_tx_power = parcel.readInt32();

  std::unique_ptr<AdvertiseData> adv(new AdvertiseData(*data));
  adv->set_include_device_name(include_device_name);
  adv->set_include_tx_power_level(include_tx_power);

  return adv;
}

void WriteAdvertiseSettingsToParcel(const AdvertiseSettings& settings,
                                    Parcel* parcel) {
  CHECK(parcel);
  parcel->writeInt32(settings.mode());
  parcel->writeInt32(settings.tx_power_level());
  parcel->writeInt32(settings.connectable());
  parcel->writeInt64(settings.timeout().InMilliseconds());
}

std::unique_ptr<AdvertiseSettings> CreateAdvertiseSettingsFromParcel(
    const Parcel& parcel) {
  AdvertiseSettings::Mode mode =
      static_cast<AdvertiseSettings::Mode>(parcel.readInt32());
  AdvertiseSettings::TxPowerLevel tx_power =
      static_cast<AdvertiseSettings::TxPowerLevel>(parcel.readInt32());
  bool connectable = parcel.readInt32();
  base::TimeDelta timeout = base::TimeDelta::FromMilliseconds(
      parcel.readInt64());

  return std::unique_ptr<AdvertiseSettings>(
      new AdvertiseSettings(mode, timeout, tx_power, connectable));
}

void WriteUUIDToParcel(const UUID& uuid, android::Parcel* parcel) {
  // The scheme used by android.os.ParcelUuid is to wrote the most significant
  // bits first as one 64-bit integer, followed by the least significant bits in
  // a second 64-bit integer. This is the same as writing the raw-bytes in
  // sequence, but we don't want to assume any host-endianness here. So follow
  // the same scheme and use the same Parcel APIs.
  UUID::UUID128Bit bytes = uuid.GetFullBigEndian();

  uint64_t most_sig_bits =
      ((((uint64_t) bytes[0]) << 56) |
       (((uint64_t) bytes[1]) << 48) |
       (((uint64_t) bytes[2]) << 40) |
       (((uint64_t) bytes[3]) << 32) |
       (((uint64_t) bytes[4]) << 24) |
       (((uint64_t) bytes[5]) << 16) |
       (((uint64_t) bytes[6]) << 8) |
       bytes[7]);

  uint64_t least_sig_bits =
      ((((uint64_t) bytes[8]) << 56) |
       (((uint64_t) bytes[9]) << 48) |
       (((uint64_t) bytes[10]) << 40) |
       (((uint64_t) bytes[11]) << 32) |
       (((uint64_t) bytes[12]) << 24) |
       (((uint64_t) bytes[13]) << 16) |
       (((uint64_t) bytes[14]) << 8) |
       bytes[15]);

  parcel->writeUint64(most_sig_bits);
  parcel->writeUint64(least_sig_bits);
}

std::unique_ptr<UUID> CreateUUIDFromParcel(
    const android::Parcel& parcel) {
  UUID::UUID128Bit bytes;

  uint64_t most_sig_bits = parcel.readUint64();
  uint64_t least_sig_bits = parcel.readUint64();

  bytes[0] = (most_sig_bits >> 56) & 0xFF;
  bytes[1] = (most_sig_bits >> 48) & 0xFF;
  bytes[2] = (most_sig_bits >> 40) & 0xFF;
  bytes[3] = (most_sig_bits >> 32) & 0xFF;
  bytes[4] = (most_sig_bits >> 24) & 0xFF;
  bytes[5] = (most_sig_bits >> 16) & 0xFF;
  bytes[6] = (most_sig_bits >> 8) & 0xFF;
  bytes[7] = most_sig_bits & 0xFF;

  bytes[8] = (least_sig_bits >> 56) & 0xFF;
  bytes[9] = (least_sig_bits >> 48) & 0xFF;
  bytes[10] = (least_sig_bits >> 40) & 0xFF;
  bytes[11] = (least_sig_bits >> 32) & 0xFF;
  bytes[12] = (least_sig_bits >> 24) & 0xFF;
  bytes[13] = (least_sig_bits >> 16) & 0xFF;
  bytes[14] = (least_sig_bits >> 8) & 0xFF;
  bytes[15] = least_sig_bits & 0xFF;

  return std::unique_ptr<UUID>(new UUID(bytes));
}

void WriteGattIdentifierToParcel(
    const GattIdentifier& gatt_id,
    android::Parcel* parcel) {
  parcel->writeCString(gatt_id.device_address().c_str());
  parcel->writeInt32(gatt_id.is_primary());

  WriteUUIDToParcel(gatt_id.service_uuid(), parcel);
  WriteUUIDToParcel(gatt_id.characteristic_uuid(), parcel);
  WriteUUIDToParcel(gatt_id.descriptor_uuid(), parcel);

  parcel->writeInt32(gatt_id.service_instance_id());
  parcel->writeInt32(gatt_id.characteristic_instance_id());
  parcel->writeInt32(gatt_id.descriptor_instance_id());
}

std::unique_ptr<GattIdentifier> CreateGattIdentifierFromParcel(
    const android::Parcel& parcel) {
  std::string device_address = parcel.readCString();
  bool is_primary = parcel.readInt32();

  auto service_uuid = CreateUUIDFromParcel(parcel);
  auto char_uuid = CreateUUIDFromParcel(parcel);
  auto desc_uuid = CreateUUIDFromParcel(parcel);

  int service_id = parcel.readInt32();
  int char_id = parcel.readInt32();
  int desc_id = parcel.readInt32();

  return std::unique_ptr<GattIdentifier>(
      new GattIdentifier(
          device_address, is_primary,
          *service_uuid, *char_uuid, *desc_uuid,
          service_id, char_id, desc_id));
}

void WriteScanFilterToParcel(
    const ScanFilter& filter,
    android::Parcel* parcel) {
  bool has_name = !filter.device_name().empty();
  parcel->writeInt32(has_name ? 1 : 0);
  if (has_name)
    parcel->writeCString(filter.device_name().c_str());

  bool has_address = !filter.device_address().empty();
  parcel->writeInt32(has_address ? 1 : 0);
  if (has_address)
    parcel->writeCString(filter.device_address().c_str());

  parcel->writeInt32(filter.service_uuid() ? 1 : 0);
  if (filter.service_uuid()) {
    WriteUUIDToParcel(*filter.service_uuid(), parcel);
    parcel->writeInt32(filter.service_uuid_mask() ? 1 : 0);
    if (filter.service_uuid_mask())
      WriteUUIDToParcel(*filter.service_uuid_mask(), parcel);
  }

  // TODO(armansito): Support service and manufacturer data.
}

std::unique_ptr<ScanFilter> CreateScanFilterFromParcel(
    const android::Parcel& parcel) {
  std::string device_name;
  if (parcel.readInt32() == 1)
    device_name = parcel.readCString();

  std::string device_address;
  if (parcel.readInt32() == 1)
    device_address = parcel.readCString();

  std::unique_ptr<UUID> service_uuid, service_uuid_mask;
  if (parcel.readInt32() == 1) {
    service_uuid = CreateUUIDFromParcel(parcel);
    if (parcel.readInt32() == 1)
      service_uuid_mask = CreateUUIDFromParcel(parcel);
  }

  // TODO(armansito): Support service and manufacturer data.

  std::unique_ptr<ScanFilter> filter(new ScanFilter());

  filter->set_device_name(device_name);

  if (!filter->SetDeviceAddress(device_address))
    return nullptr;

  if (!service_uuid)
    return filter;

  if (service_uuid_mask)
    filter->SetServiceUuidWithMask(*service_uuid, *service_uuid_mask);
  else
    filter->SetServiceUuid(*service_uuid);

  return filter;
}

void WriteScanSettingsToParcel(
    const ScanSettings& settings,
    android::Parcel* parcel) {
  parcel->writeInt32(settings.mode());
  parcel->writeInt32(settings.callback_type());
  parcel->writeInt32(settings.result_type());
  parcel->writeInt64(settings.report_delay().InMilliseconds());
  parcel->writeInt32(settings.match_mode());
  parcel->writeInt32(settings.match_count_per_filter());
}

std::unique_ptr<ScanSettings> CreateScanSettingsFromParcel(
    const android::Parcel& parcel) {
  ScanSettings::Mode mode =
      static_cast<ScanSettings::Mode>(parcel.readInt32());
  ScanSettings::CallbackType callback_type =
      static_cast<ScanSettings::CallbackType>(parcel.readInt32());
  ScanSettings::ResultType result_type =
      static_cast<ScanSettings::ResultType>(parcel.readInt32());
  base::TimeDelta report_delay = base::TimeDelta::FromMilliseconds(
      parcel.readInt64());
  ScanSettings::MatchMode match_mode =
      static_cast<ScanSettings::MatchMode>(parcel.readInt32());
  ScanSettings::MatchCount match_count_per_filter =
      static_cast<ScanSettings::MatchCount>(parcel.readInt32());

  return std::unique_ptr<ScanSettings>(new ScanSettings(
      mode, callback_type, result_type, report_delay,
      match_mode, match_count_per_filter));
}

void WriteScanResultToParcel(
    const bluetooth::ScanResult& scan_result,
    android::Parcel* parcel) {
  // The Java framework code conditionally inserts 1 or 0 to indicate if the
  // device adress and the scan record fields are present, based on whether the
  // Java object is null. We do something similar here for consistency, although
  // the native definition of ScanResult requires a valid BD_ADDR.
  if (util::IsAddressValid(scan_result.device_address())) {
    parcel->writeInt32(1);
    parcel->writeCString(scan_result.device_address().c_str());
  } else {
    parcel->writeInt32(0);
  }

  parcel->writeByteVector(scan_result.scan_record());
  parcel->writeInt32(scan_result.rssi());
}

std::unique_ptr<bluetooth::ScanResult> CreateScanResultFromParcel(
    const android::Parcel& parcel) {
  std::string device_address;
  if (parcel.readInt32())
    device_address = parcel.readCString();

  std::unique_ptr<std::vector<uint8_t>> scan_record;
  parcel.readByteVector(&scan_record);
  CHECK(scan_record.get());

  int rssi = parcel.readInt32();

  return std::unique_ptr<ScanResult>(new ScanResult(
      device_address, *scan_record, rssi));
}

void WriteBtGattDbArrayToParcel(const btgatt_db_element_t* db, int size,
                                android::Parcel* parcel) {
  parcel->writeInt32(size);
  for (int i = 0 ; i < size; i++) {
    parcel->writeInt32(db[i].id);
    parcel->writeByteVector(std::vector<uint8_t>(&(db[i].uuid.uu[0]), &(db[i].uuid.uu[sizeof(db[i].uuid.uu)])));
    parcel->writeInt32(db[i].type);
    parcel->writeInt32(db[i].attribute_handle);
    parcel->writeInt32(db[i].start_handle);
    parcel->writeInt32(db[i].end_handle);
    parcel->writeInt32(db[i].properties);
  }
}

void CreateBtGattDbArrayFromParcel(const android::Parcel& parcel,
                                   btgatt_db_element_t** pDb, int* pSize) {
  int size = parcel.readInt32();
  *pSize = size;
  if (size <= 0) {
    *pDb = nullptr;
    return;
  }
  btgatt_db_element_t* db = (btgatt_db_element_t *) malloc(sizeof(*db) * size);
  (void) memset(db, 0, sizeof(*db) * size);
  for (int i = 0 ; i < size; i++) {
    db[i].id = (uint16_t) parcel.readInt32();
    std::vector<uint8_t> uuidBytes;
    (void) parcel.readByteVector(&uuidBytes);
    memcpy(db[i].uuid.uu, uuidBytes.data(), uuidBytes.size());
    db[i].type = (bt_gatt_db_attribute_type_t) parcel.readInt32();
    db[i].attribute_handle = (uint16_t) parcel.readInt32();
    db[i].start_handle = (uint16_t) parcel.readInt32();
    db[i].end_handle = (uint16_t) parcel.readInt32();
    db[i].properties = (uint8_t) parcel.readInt32();
  }
  *pDb = db;
}

void WriteBtGattReadParamsToParcel(const btgatt_read_params_t* data,
                                   android::Parcel* parcel)
{
  parcel->writeInt32(data->handle);
  std::vector<uint8_t> value(&(data->value.value[0]),
                             &(data->value.value[data->value.len]));
  parcel->writeByteVector(value);
  parcel->writeInt32(data->value_type);
  parcel->writeInt32(data->status);
}

void CreateBtGattReadParamsFromParcel(const android::Parcel& parcel,
                                      btgatt_read_params_t** pData)
{
  btgatt_read_params_t* data = (btgatt_read_params_t *) malloc(sizeof(*data));
  (void) memset(data, 0, sizeof(*data));
  data->handle = (uint16_t) parcel.readInt32();
  std::vector<uint8_t> value;
  (void) parcel.readByteVector(&value);
  memcpy(&(data->value.value[0]), value.data(), value.size());
  data->value.len = (uint16_t) value.size();
  data->value_type = (uint16_t) parcel.readInt32();
  data->status = (uint8_t) parcel.readInt32();
  *pData = data;
}

void WriteBtGattNotifyParamsToParcel(const btgatt_notify_params_t* notification,
                                     android::Parcel* parcel)
{
  std::vector<uint8_t> value(&(notification->value[0]),
                             &(notification->value[notification->len]));
  parcel->writeByteVector(value);
  std::vector<uint8_t> bda(&notification->bda.address[0],
                           &notification->bda.address[sizeof(notification->bda.address)]);
  parcel->writeByteVector(bda);
  parcel->writeInt32(notification->handle);
  parcel->writeInt32(notification->len);
  parcel->writeInt32(notification->is_notify);
}

void CreateBtGattNotifyParamsFromParcel(const android::Parcel& parcel,
                                        btgatt_notify_params_t** pNotification)
{
  btgatt_notify_params_t* notification =
      (btgatt_notify_params_t *) malloc(sizeof(*notification));
  (void) memset(notification, 0, sizeof(*notification));
  std::vector<uint8_t> value;
  (void) parcel.readByteVector(&value);
  size_t n = std::min(value.size(), sizeof(notification->value));
  memcpy(&(notification->value), value.data(), n);
  notification->len = n;
  std::vector<uint8_t> bda;
  (void) parcel.readByteVector(&bda);
  n = std::min(bda.size(), sizeof(notification->bda.address));
  memcpy(&(notification->bda.address), bda.data(), n);
  notification->handle = (uint16_t) parcel.readInt32();
  notification->len = (uint16_t) parcel.readInt32();
  notification->is_notify = (uint8_t) parcel.readInt32();
  *pNotification = notification;
}


}  // namespace binder
}  // namespace ipc
