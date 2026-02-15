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

#include "service/common/bluetooth/binder/IBluetoothLowEnergy.h"

#include <base/logging.h>
#include <binder/Parcel.h>

#include "service/common/bluetooth/binder/parcel_helpers.h"

using android::IBinder;
using android::interface_cast;
using android::Parcel;
using android::sp;
using android::status_t;

using bluetooth::AdvertiseData;
using bluetooth::AdvertiseSettings;

namespace ipc {
namespace binder {

// static
const char IBluetoothLowEnergy::kServiceName[] =
    "bluetooth-low-energy-service";

// BnBluetoothLowEnergy (server) implementation
// ========================================================

status_t BnBluetoothLowEnergy::onTransact(
    uint32_t code, const Parcel& data, Parcel* reply, uint32_t flags) {
  VLOG(2) << "IBluetoothLowEnergy: " << code;
  if (!data.checkInterface(this))
    return android::PERMISSION_DENIED;

  switch (code) {
  case REGISTER_CLIENT_TRANSACTION: {
    sp<IBinder> callback = data.readStrongBinder();
    bool result = RegisterClient(
        interface_cast<IBluetoothLowEnergyCallback>(callback));

    reply->writeInt32(result);

    return android::NO_ERROR;
  }
  case UNREGISTER_CLIENT_TRANSACTION: {
    int client_id = data.readInt32();
    UnregisterClient(client_id);
    return android::NO_ERROR;
  }
  case UNREGISTER_ALL_TRANSACTION: {
    UnregisterAll();
    return android::NO_ERROR;
  }
  case CONNECT_TRANSACTION: {
    int client_id = data.readInt32();
    const char* address = data.readCString();
    bool is_direct = data.readBool();

    bool result = Connect(client_id, address, is_direct);
    reply->writeInt32(result);

    return android::NO_ERROR;
  }
  case DISCONNECT_TRANSACTION: {
    int client_id = data.readInt32();
    const char* address = data.readCString();

    bool result = Disconnect(client_id, address);
    reply->writeInt32(result);

    return android::NO_ERROR;
  }
  case SET_MTU_TRANSACTION: {
    int client_id = data.readInt32();
    const char* address = data.readCString();
    int mtu = data.readInt32();

    bool result = SetMtu(client_id, address, mtu);
    reply->writeInt32(result);

    return android::NO_ERROR;
  }
  case DISCOVER_SERVICES_TRANSACTION: {
    int client_id = data.readInt32();
    const char* address = data.readCString();

    bool result = DiscoverServices(client_id, address);
    reply->writeInt32(result);

    return android::NO_ERROR;
  }
  case GET_GATT_DB_TRANSACTION: {
    int client_id = data.readInt32();
    const char* address = data.readCString();

    bool result = GetGattDb(client_id, address);
    reply->writeInt32(result);

    return android::NO_ERROR;
  }
  case READ_CHARACTERISTIC_TRANSACTION: {
    int client_id = data.readInt32();
    const char* address = data.readCString();
    int handle = data.readInt32();

    bool result = ReadCharacteristic(client_id, address, handle);
    reply->writeInt32(result);

    return android::NO_ERROR;
  }
  case WRITE_CHARACTERISTIC_TRANSACTION: {
    int client_id = data.readInt32();
    const char* address = data.readCString();
    int handle = data.readInt32();
    int write_type = data.readInt32();
    std::vector<uint8_t> value;
    (void) data.readByteVector(&value);

    bool result = WriteCharacteristic(client_id, address, handle, write_type, value);
    reply->writeInt32(result);

    return android::NO_ERROR;
  }
  case WRITE_DESCRIPTOR_TRANSACTION: {
    int client_id = data.readInt32();
    const char* address = data.readCString();
    int handle = data.readInt32();
    int write_type = data.readInt32();
    std::vector<uint8_t> value;
    (void) data.readByteVector(&value);

    bool result = WriteDescriptor(client_id, address, handle, write_type, value);
    reply->writeInt32(result);

    return android::NO_ERROR;
  }
  case START_SCAN_TRANSACTION: {
    int client_id = data.readInt32();
    auto settings = CreateScanSettingsFromParcel(data);
    CHECK(settings);
    std::vector<bluetooth::ScanFilter> filters;

    int list_meta_data = data.readInt32();
    CHECK(list_meta_data == kParcelValList);

    int filter_count = data.readInt32();
    if (filter_count >= 0) {  // Make sure |filter_count| isn't negative.
      for (int i = 0; i < filter_count; i++) {
        auto filter = CreateScanFilterFromParcel(data);
        CHECK(filter);
        filters.push_back(*filter);
      }
    }

    bool result = StartScan(client_id, *settings, filters);
    reply->writeInt32(result);

    return android::NO_ERROR;
  }
  case STOP_SCAN_TRANSACTION: {
    int client_id = data.readInt32();
    bool result = StopScan(client_id);
    reply->writeInt32(result);
    return android::NO_ERROR;
  }
  case START_MULTI_ADVERTISING_TRANSACTION: {
    int client_id = data.readInt32();
    std::unique_ptr<AdvertiseData> adv_data =
        CreateAdvertiseDataFromParcel(data);
    std::unique_ptr<AdvertiseData> scan_rsp =
        CreateAdvertiseDataFromParcel(data);
    std::unique_ptr<AdvertiseSettings> adv_settings =
        CreateAdvertiseSettingsFromParcel(data);

    bool result = StartMultiAdvertising(
        client_id, *adv_data, *scan_rsp, *adv_settings);

    reply->writeInt32(result);

    return android::NO_ERROR;
  }
  case STOP_MULTI_ADVERTISING_TRANSACTION: {
    int client_id = data.readInt32();
    bool result = StopMultiAdvertising(client_id);

    reply->writeInt32(result);

    return android::NO_ERROR;
  }
  default:
    return BBinder::onTransact(code, data, reply, flags);
  }
}

// BpBluetoothLowEnergy (client) implementation
// ========================================================

BpBluetoothLowEnergy::BpBluetoothLowEnergy(const sp<IBinder>& impl)
    : BpInterface<IBluetoothLowEnergy>(impl) {
}

bool BpBluetoothLowEnergy::RegisterClient(
      const sp<IBluetoothLowEnergyCallback>& callback) {
  Parcel data, reply;

  data.writeInterfaceToken(IBluetoothLowEnergy::getInterfaceDescriptor());
  data.writeStrongBinder(IInterface::asBinder(callback.get()));

  remote()->transact(IBluetoothLowEnergy::REGISTER_CLIENT_TRANSACTION,
                     data, &reply);

  return reply.readInt32();
}

void BpBluetoothLowEnergy::UnregisterClient(int client_id) {
  Parcel data, reply;

  data.writeInterfaceToken(IBluetoothLowEnergy::getInterfaceDescriptor());
  data.writeInt32(client_id);

  remote()->transact(IBluetoothLowEnergy::UNREGISTER_CLIENT_TRANSACTION,
                     data, &reply);
}

void BpBluetoothLowEnergy::UnregisterAll() {
  Parcel data, reply;

  data.writeInterfaceToken(IBluetoothLowEnergy::getInterfaceDescriptor());

  remote()->transact(IBluetoothLowEnergy::UNREGISTER_ALL_TRANSACTION,
                     data, &reply);
}

bool BpBluetoothLowEnergy::Connect(int client_id, const char* address,
                                   bool is_direct) {
  Parcel data, reply;

  data.writeInterfaceToken(IBluetoothLowEnergy::getInterfaceDescriptor());
  data.writeInt32(client_id);
  data.writeCString(address);
  data.writeBool(is_direct);

  remote()->transact(IBluetoothLowEnergy::CONNECT_TRANSACTION,
                     data, &reply);

  return reply.readInt32();
}

bool BpBluetoothLowEnergy::Disconnect(int client_id, const char* address) {
  Parcel data, reply;

  data.writeInterfaceToken(IBluetoothLowEnergy::getInterfaceDescriptor());
  data.writeInt32(client_id);
  data.writeCString(address);

  remote()->transact(IBluetoothLowEnergy::DISCONNECT_TRANSACTION,
                     data, &reply);

  return reply.readInt32();
}

bool BpBluetoothLowEnergy::SetMtu(int client_id, const char* address, int mtu) {
  Parcel data, reply;

  data.writeInterfaceToken(IBluetoothLowEnergy::getInterfaceDescriptor());
  data.writeInt32(client_id);
  data.writeCString(address);
  data.writeInt32(mtu);

  remote()->transact(IBluetoothLowEnergy::SET_MTU_TRANSACTION, data, &reply);
  return reply.readInt32();
}

bool BpBluetoothLowEnergy::DiscoverServices(int client_id, const char* address) {
  Parcel data, reply;

  data.writeInterfaceToken(IBluetoothLowEnergy::getInterfaceDescriptor());
  data.writeInt32(client_id);
  data.writeCString(address);

  remote()->transact(IBluetoothLowEnergy::DISCOVER_SERVICES_TRANSACTION, data, &reply);
  return reply.readInt32();
}

bool BpBluetoothLowEnergy::GetGattDb(int client_id, const char* address) {
  Parcel data, reply;

  data.writeInterfaceToken(IBluetoothLowEnergy::getInterfaceDescriptor());
  data.writeInt32(client_id);
  data.writeCString(address);

  remote()->transact(IBluetoothLowEnergy::GET_GATT_DB_TRANSACTION, data, &reply);
  return reply.readInt32();
}

bool BpBluetoothLowEnergy::ReadCharacteristic(int client_id, const char* address, int handle) {
  Parcel data, reply;

  data.writeInterfaceToken(IBluetoothLowEnergy::getInterfaceDescriptor());
  data.writeInt32(client_id);
  data.writeCString(address);
  data.writeInt32(handle);

  remote()->transact(IBluetoothLowEnergy::READ_CHARACTERISTIC_TRANSACTION, data, &reply);
  return reply.readInt32();
}

bool BpBluetoothLowEnergy::WriteCharacteristic(int client_id, const char* address,
                                               int handle, int write_type,
                                               const std::vector<uint8_t>& value) {
  Parcel data, reply;

  data.writeInterfaceToken(IBluetoothLowEnergy::getInterfaceDescriptor());
  data.writeInt32(client_id);
  data.writeCString(address);
  data.writeInt32(handle);
  data.writeInt32(write_type);
  data.writeByteVector(value);

  remote()->transact(IBluetoothLowEnergy::WRITE_CHARACTERISTIC_TRANSACTION, data, &reply);
  return reply.readInt32();
}

bool BpBluetoothLowEnergy::WriteDescriptor(int client_id, const char* address,
                                           int handle, int write_type,
                                           const std::vector<uint8_t>& value) {
  Parcel data, reply;

  data.writeInterfaceToken(IBluetoothLowEnergy::getInterfaceDescriptor());
  data.writeInt32(client_id);
  data.writeCString(address);
  data.writeInt32(handle);
  data.writeInt32(write_type);
  data.writeByteVector(value);

  remote()->transact(IBluetoothLowEnergy::WRITE_DESCRIPTOR_TRANSACTION, data, &reply);
  return reply.readInt32();
}

bool BpBluetoothLowEnergy::StartScan(
    int client_id,
    const bluetooth::ScanSettings& settings,
    const std::vector<bluetooth::ScanFilter>& filters) {
  Parcel data, reply;

  data.writeInterfaceToken(IBluetoothLowEnergy::getInterfaceDescriptor());
  data.writeInt32(client_id);
  WriteScanSettingsToParcel(settings, &data);

  // The Java equivalent of |filters| is a List<ScanFilter>. Parcel.java inserts
  // a metadata value of VAL_LIST (11) for this so I'm doing it here for
  // compatibility.
  data.writeInt32(kParcelValList);
  data.writeInt32(filters.size());
  for (const auto& filter : filters)
    WriteScanFilterToParcel(filter, &data);

  remote()->transact(IBluetoothLowEnergy::START_SCAN_TRANSACTION,
                     data, &reply);

  return reply.readInt32();
}

bool BpBluetoothLowEnergy::StopScan(int client_id) {
  Parcel data, reply;

  data.writeInterfaceToken(IBluetoothLowEnergy::getInterfaceDescriptor());
  data.writeInt32(client_id);

  remote()->transact(IBluetoothLowEnergy::STOP_SCAN_TRANSACTION,
                     data, &reply);

  return reply.readInt32();
}

bool BpBluetoothLowEnergy::StartMultiAdvertising(
    int client_id,
    const AdvertiseData& advertise_data,
    const AdvertiseData& scan_response,
    const AdvertiseSettings& settings) {
  Parcel data, reply;

  data.writeInterfaceToken(IBluetoothLowEnergy::getInterfaceDescriptor());
  data.writeInt32(client_id);
  WriteAdvertiseDataToParcel(advertise_data, &data);
  WriteAdvertiseDataToParcel(scan_response, &data);
  WriteAdvertiseSettingsToParcel(settings, &data);

  remote()->transact(IBluetoothLowEnergy::START_MULTI_ADVERTISING_TRANSACTION,
                     data, &reply);

  return reply.readInt32();
}

bool BpBluetoothLowEnergy::StopMultiAdvertising(int client_id) {
  Parcel data, reply;

  data.writeInterfaceToken(IBluetoothLowEnergy::getInterfaceDescriptor());
  data.writeInt32(client_id);

  remote()->transact(IBluetoothLowEnergy::STOP_MULTI_ADVERTISING_TRANSACTION,
                     data, &reply);

  return reply.readInt32();
}

IMPLEMENT_META_INTERFACE(BluetoothLowEnergy, IBluetoothLowEnergy::kServiceName);

}  // namespace binder
}  // namespace ipc
