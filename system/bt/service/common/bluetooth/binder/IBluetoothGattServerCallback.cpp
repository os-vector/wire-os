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

#include "service/common/bluetooth/binder/IBluetoothGattServerCallback.h"

#include <base/logging.h>
#include <binder/Parcel.h>

#include "service/common/bluetooth/binder/parcel_helpers.h"

using android::IBinder;
using android::Parcel;
using android::sp;
using android::status_t;

namespace ipc {
namespace binder {

// static
const char IBluetoothGattServerCallback::kServiceName[] =
    "bluetooth-gatt-server-callback-service";

// BnBluetoothGattServerCallback (server) implementation
// ========================================================

status_t BnBluetoothGattServerCallback::onTransact(
    uint32_t code,
    const Parcel& data,
    Parcel* reply,
    uint32_t flags) {
  VLOG(2) << "IBluetoothGattServerCallback: " << code;
  if (!data.checkInterface(this))
    return android::PERMISSION_DENIED;

  switch (code) {
  case ON_SERVER_REGISTERED_TRANSACTION: {
    int status = data.readInt32();
    int server_if = data.readInt32();
    OnServerRegistered(status, server_if);
    return android::NO_ERROR;
  }
  case ON_SERVICE_ADDED_TRANSACTION: {
    int status = data.readInt32();
    auto gatt_id = CreateGattIdentifierFromParcel(data);
    CHECK(gatt_id);
    OnServiceAdded(status, *gatt_id);
    return android::NO_ERROR;
  }
  case ON_CHARACTERISTIC_READ_REQUEST_TRANSACTION: {
    std::string device_address = data.readCString();
    int request_id = data.readInt32();
    int offset = data.readInt32();
    bool is_long = data.readInt32();
    auto char_id = CreateGattIdentifierFromParcel(data);
    CHECK(char_id);
    OnCharacteristicReadRequest(device_address, request_id, offset, is_long,
                                *char_id);
    return android::NO_ERROR;
  }
  case ON_DESCRIPTOR_READ_REQUEST_TRANSACTION: {
    std::string device_address = data.readCString();
    int request_id = data.readInt32();
    int offset = data.readInt32();
    bool is_long = data.readInt32();
    auto desc_id = CreateGattIdentifierFromParcel(data);
    CHECK(desc_id);
    OnDescriptorReadRequest(device_address, request_id, offset, is_long,
                            *desc_id);
    return android::NO_ERROR;
  }
  case ON_CHARACTERISTIC_WRITE_REQUEST_TRANSACTION: {
    std::string device_address = data.readCString();
    int request_id = data.readInt32();
    int offset = data.readInt32();
    bool is_prep = data.readInt32();
    bool need_rsp = data.readInt32();

    std::unique_ptr<std::vector<uint8_t>> value;
    data.readByteVector(&value);
    CHECK(value.get());

    auto char_id = CreateGattIdentifierFromParcel(data);
    CHECK(char_id);

    OnCharacteristicWriteRequest(device_address, request_id, offset, is_prep,
                                 need_rsp, *value, *char_id);
    return android::NO_ERROR;
  }
  case ON_DESCRIPTOR_WRITE_REQUEST_TRANSACTION: {
    std::string device_address = data.readCString();
    int request_id = data.readInt32();
    int offset = data.readInt32();
    bool is_prep = data.readInt32();
    bool need_rsp = data.readInt32();

    std::unique_ptr<std::vector<uint8_t>> value;
    data.readByteVector(&value);
    CHECK(value.get());

    auto desc_id = CreateGattIdentifierFromParcel(data);
    CHECK(desc_id);

    OnDescriptorWriteRequest(device_address, request_id, offset, is_prep,
                             need_rsp, *value, *desc_id);
    return android::NO_ERROR;
  }
  case ON_EXECUTE_WRITE_REQUEST_TRANSACTION: {
    std::string device_address = data.readCString();
    int request_id = data.readInt32();
    bool is_exec = data.readInt32();

    OnExecuteWriteRequest(device_address, request_id, is_exec);
    return android::NO_ERROR;
  }
  case ON_NOTIFICATION_SENT_TRANSACTION: {
    std::string device_address = data.readCString();
    int status = data.readInt32();

    OnNotificationSent(device_address, status);
    return android::NO_ERROR;
  }
  default:
    return BBinder::onTransact(code, data, reply, flags);
  }
}

// BpBluetoothGattServerCallback (client) implementation
// ========================================================

BpBluetoothGattServerCallback::BpBluetoothGattServerCallback(
    const sp<IBinder>& impl)
    : BpInterface<IBluetoothGattServerCallback>(impl) {
}

void BpBluetoothGattServerCallback::OnServerRegistered(
    int status, int server_if) {
  Parcel data, reply;

  data.writeInterfaceToken(
      IBluetoothGattServerCallback::getInterfaceDescriptor());
  data.writeInt32(status);
  data.writeInt32(server_if);

  remote()->transact(
      IBluetoothGattServerCallback::ON_SERVER_REGISTERED_TRANSACTION,
      data, &reply,
      IBinder::FLAG_ONEWAY);
}

void BpBluetoothGattServerCallback::OnServiceAdded(
    int status,
    const bluetooth::GattIdentifier& service_id) {
  Parcel data, reply;

  data.writeInterfaceToken(
      IBluetoothGattServerCallback::getInterfaceDescriptor());
  data.writeInt32(status);
  WriteGattIdentifierToParcel(service_id, &data);

  remote()->transact(IBluetoothGattServerCallback::ON_SERVICE_ADDED_TRANSACTION,
                     data, &reply,
                     IBinder::FLAG_ONEWAY);
}

void BpBluetoothGattServerCallback::OnCharacteristicReadRequest(
    const std::string& device_address,
    int request_id, int offset, bool is_long,
    const bluetooth::GattIdentifier& characteristic_id) {
  Parcel data, reply;

  data.writeInterfaceToken(
      IBluetoothGattServerCallback::getInterfaceDescriptor());
  data.writeCString(device_address.c_str());
  data.writeInt32(request_id);
  data.writeInt32(offset);
  data.writeInt32(is_long);
  WriteGattIdentifierToParcel(characteristic_id, &data);

  remote()->transact(
      IBluetoothGattServerCallback::ON_CHARACTERISTIC_READ_REQUEST_TRANSACTION,
      data, &reply,
      IBinder::FLAG_ONEWAY);
}

void BpBluetoothGattServerCallback::OnDescriptorReadRequest(
    const std::string& device_address,
    int request_id, int offset, bool is_long,
    const bluetooth::GattIdentifier& descriptor_id) {
  Parcel data, reply;

  data.writeInterfaceToken(
      IBluetoothGattServerCallback::getInterfaceDescriptor());
  data.writeCString(device_address.c_str());
  data.writeInt32(request_id);
  data.writeInt32(offset);
  data.writeInt32(is_long);
  WriteGattIdentifierToParcel(descriptor_id, &data);

  remote()->transact(
      IBluetoothGattServerCallback::ON_DESCRIPTOR_READ_REQUEST_TRANSACTION,
      data, &reply,
      IBinder::FLAG_ONEWAY);
}

void BpBluetoothGattServerCallback::OnCharacteristicWriteRequest(
    const std::string& device_address,
    int request_id, int offset, bool is_prepare_write, bool need_response,
    const std::vector<uint8_t>& value,
    const bluetooth::GattIdentifier& characteristic_id) {
  Parcel data, reply;

  data.writeInterfaceToken(
      IBluetoothGattServerCallback::getInterfaceDescriptor());
  data.writeCString(device_address.c_str());
  data.writeInt32(request_id);
  data.writeInt32(offset);
  data.writeInt32(is_prepare_write);
  data.writeInt32(need_response);
  data.writeByteVector(value);
  WriteGattIdentifierToParcel(characteristic_id, &data);

  remote()->transact(
      IBluetoothGattServerCallback::ON_CHARACTERISTIC_WRITE_REQUEST_TRANSACTION,
      data, &reply,
      IBinder::FLAG_ONEWAY);
}

void BpBluetoothGattServerCallback::OnDescriptorWriteRequest(
    const std::string& device_address,
    int request_id, int offset, bool is_prepare_write, bool need_response,
    const std::vector<uint8_t>& value,
    const bluetooth::GattIdentifier& descriptor_id) {
  Parcel data, reply;

  data.writeInterfaceToken(
      IBluetoothGattServerCallback::getInterfaceDescriptor());
  data.writeCString(device_address.c_str());
  data.writeInt32(request_id);
  data.writeInt32(offset);
  data.writeInt32(is_prepare_write);
  data.writeInt32(need_response);
  data.writeByteVector(value);
  WriteGattIdentifierToParcel(descriptor_id, &data);

  remote()->transact(
      IBluetoothGattServerCallback::ON_DESCRIPTOR_WRITE_REQUEST_TRANSACTION,
      data, &reply,
      IBinder::FLAG_ONEWAY);
}

void BpBluetoothGattServerCallback::OnExecuteWriteRequest(
    const std::string& device_address,
    int request_id, bool is_execute) {
  Parcel data, reply;

  data.writeInterfaceToken(
      IBluetoothGattServerCallback::getInterfaceDescriptor());
  data.writeCString(device_address.c_str());
  data.writeInt32(request_id);
  data.writeInt32(is_execute);

  remote()->transact(
      IBluetoothGattServerCallback::ON_EXECUTE_WRITE_REQUEST_TRANSACTION,
      data, &reply,
      IBinder::FLAG_ONEWAY);
}

void BpBluetoothGattServerCallback::OnNotificationSent(
    const std::string& device_address,
    int status) {
  Parcel data, reply;

  data.writeInterfaceToken(
      IBluetoothGattServerCallback::getInterfaceDescriptor());
  data.writeCString(device_address.c_str());
  data.writeInt32(status);

  remote()->transact(
      IBluetoothGattServerCallback::ON_NOTIFICATION_SENT_TRANSACTION,
      data, &reply,
      IBinder::FLAG_ONEWAY);
}

IMPLEMENT_META_INTERFACE(BluetoothGattServerCallback,
                         IBluetoothGattServerCallback::kServiceName);

}  // namespace binder
}  // namespace ipc
