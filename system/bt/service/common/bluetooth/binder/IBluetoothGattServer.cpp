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

#include "service/common/bluetooth/binder/IBluetoothGattServer.h"

#include <base/logging.h>
#include <binder/Parcel.h>

#include "service/common/bluetooth/binder/parcel_helpers.h"

using android::IBinder;
using android::interface_cast;
using android::Parcel;
using android::sp;
using android::status_t;

namespace ipc {
namespace binder {

// static
const char IBluetoothGattServer::kServiceName[] =
    "bluetooth-gatt-server-service";

// BnBluetoothGattServer (server) implementation
// ========================================================

status_t BnBluetoothGattServer::onTransact(
    uint32_t code, const Parcel& data, Parcel* reply, uint32_t flags) {
  VLOG(2) << "IBluetoothGattServer: " << code;
  if (!data.checkInterface(this))
    return android::PERMISSION_DENIED;

  switch (code) {
  case REGISTER_SERVER_TRANSACTION: {
    sp<IBinder> callback = data.readStrongBinder();
    bool result = RegisterServer(
        interface_cast<IBluetoothGattServerCallback>(callback));
    reply->writeInt32(result);
    return android::NO_ERROR;
  }
  case UNREGISTER_SERVER_TRANSACTION: {
    int server_if = data.readInt32();
    UnregisterServer(server_if);
    return android::NO_ERROR;
  }
  case UNREGISTER_ALL_TRANSACTION: {
    UnregisterAll();
    return android::NO_ERROR;
  }
  case BEGIN_SERVICE_DECLARATION_TRANSACTION: {
    int server_if = data.readInt32();
    bool is_primary = data.readInt32();
    auto uuid = CreateUUIDFromParcel(data);
    CHECK(uuid);

    std::unique_ptr<bluetooth::GattIdentifier> out_id;
    bool result = BeginServiceDeclaration(
        server_if, is_primary, *uuid, &out_id);

    reply->writeInt32(result);

    if (result) {
      CHECK(out_id);
      WriteGattIdentifierToParcel(*out_id, reply);
    }

    return android::NO_ERROR;
  }
  case ADD_CHARACTERISTIC_TRANSACTION: {
    int server_if = data.readInt32();
    auto uuid = CreateUUIDFromParcel(data);
    CHECK(uuid);
    int properties = data.readInt32();
    int permissions = data.readInt32();

    std::unique_ptr<bluetooth::GattIdentifier> out_id;
    bool result = AddCharacteristic(
        server_if, *uuid, properties, permissions, &out_id);

    reply->writeInt32(result);

    if (result) {
      CHECK(out_id);
      WriteGattIdentifierToParcel(*out_id, reply);
    }

    return android::NO_ERROR;
  }
  case ADD_DESCRIPTOR_TRANSACTION: {
    int server_if = data.readInt32();
    auto uuid = CreateUUIDFromParcel(data);
    CHECK(uuid);
    int permissions = data.readInt32();

    std::unique_ptr<bluetooth::GattIdentifier> out_id;
    bool result = AddDescriptor(server_if, *uuid, permissions, &out_id);

    reply->writeInt32(result);

    if (result) {
      CHECK(out_id);
      WriteGattIdentifierToParcel(*out_id, reply);
    }

    return android::NO_ERROR;
  }
  case END_SERVICE_DECLARATION_TRANSACTION: {
    int server_if = data.readInt32();
    bool result = EndServiceDeclaration(server_if);
    reply->writeInt32(result);
    return android::NO_ERROR;
  }
  case SEND_RESPONSE_TRANSACTION: {
    int server_if = data.readInt32();
    std::string device_address = data.readCString();
    int request_id = data.readInt32();
    int status = data.readInt32();
    int offset = data.readInt32();

    std::unique_ptr<std::vector<uint8_t>> value;
    data.readByteVector(&value);
    CHECK(value.get());

    bool result = SendResponse(
        server_if, device_address, request_id, status, offset, *value);

    reply->writeInt32(result);

    return android::NO_ERROR;
  }
  case SEND_NOTIFICATION_TRANSACTION: {
    int server_if = data.readInt32();
    std::string device_address = data.readCString();
    auto char_id = CreateGattIdentifierFromParcel(data);
    CHECK(char_id);
    bool confirm = data.readInt32();

    std::unique_ptr<std::vector<uint8_t>> value;
    data.readByteVector(&value);
    CHECK(value.get());

    bool result = SendNotification(server_if, device_address, *char_id, confirm,
                                   *value);

    reply->writeInt32(result);

    return android::NO_ERROR;
  }
  default:
    return BBinder::onTransact(code, data, reply, flags);
  }
}

// BpBluetoothGattServer (client) implementation
// ========================================================

BpBluetoothGattServer::BpBluetoothGattServer(const sp<IBinder>& impl)
    : BpInterface<IBluetoothGattServer>(impl) {
}

bool BpBluetoothGattServer::RegisterServer(
    const sp<IBluetoothGattServerCallback>& callback) {
  Parcel data, reply;

  data.writeInterfaceToken(IBluetoothGattServer::getInterfaceDescriptor());
  data.writeStrongBinder(IInterface::asBinder(callback.get()));

  remote()->transact(IBluetoothGattServer::REGISTER_SERVER_TRANSACTION,
                     data, &reply);

  return reply.readInt32();
}

void BpBluetoothGattServer::UnregisterServer(int server_if) {
  Parcel data, reply;

  data.writeInterfaceToken(IBluetoothGattServer::getInterfaceDescriptor());
  data.writeInt32(server_if);

  remote()->transact(IBluetoothGattServer::UNREGISTER_SERVER_TRANSACTION,
                     data, &reply);
}

void BpBluetoothGattServer::UnregisterAll() {
  Parcel data, reply;

  data.writeInterfaceToken(IBluetoothGattServer::getInterfaceDescriptor());

  remote()->transact(IBluetoothGattServer::UNREGISTER_ALL_TRANSACTION,
                     data, &reply);
}

bool BpBluetoothGattServer::BeginServiceDeclaration(
    int server_if, bool is_primary, const bluetooth::UUID& uuid,
    std::unique_ptr<bluetooth::GattIdentifier>* out_id) {
  CHECK(out_id);
  Parcel data, reply;

  data.writeInterfaceToken(IBluetoothGattServer::getInterfaceDescriptor());
  data.writeInt32(server_if);
  data.writeInt32(is_primary);
  WriteUUIDToParcel(uuid, &data);

  remote()->transact(
      IBluetoothGattServer::BEGIN_SERVICE_DECLARATION_TRANSACTION,
      data, &reply);

  bool result = reply.readInt32();
  if (result)
    *out_id = CreateGattIdentifierFromParcel(reply);

  return result;
}

bool BpBluetoothGattServer::AddCharacteristic(
    int server_if, const bluetooth::UUID& uuid,
    int properties, int permissions,
    std::unique_ptr<bluetooth::GattIdentifier>* out_id) {
  CHECK(out_id);
  Parcel data, reply;

  data.writeInterfaceToken(IBluetoothGattServer::getInterfaceDescriptor());
  data.writeInt32(server_if);
  WriteUUIDToParcel(uuid, &data);
  data.writeInt32(properties);
  data.writeInt32(permissions);

  remote()->transact(IBluetoothGattServer::ADD_CHARACTERISTIC_TRANSACTION,
                     data, &reply);

  bool result = reply.readInt32();
  if (result)
    *out_id = CreateGattIdentifierFromParcel(reply);

  return result;
}

bool BpBluetoothGattServer::AddDescriptor(
    int server_if, const bluetooth::UUID& uuid, int permissions,
    std::unique_ptr<bluetooth::GattIdentifier>* out_id) {
  CHECK(out_id);
  Parcel data, reply;

  data.writeInterfaceToken(IBluetoothGattServer::getInterfaceDescriptor());
  data.writeInt32(server_if);
  WriteUUIDToParcel(uuid, &data);
  data.writeInt32(permissions);

  remote()->transact(IBluetoothGattServer::ADD_DESCRIPTOR_TRANSACTION,
                     data, &reply);

  bool result = reply.readInt32();
  if (result)
    *out_id = CreateGattIdentifierFromParcel(reply);

  return result;
}

bool BpBluetoothGattServer::EndServiceDeclaration(int server_if) {
  Parcel data, reply;

  data.writeInterfaceToken(IBluetoothGattServer::getInterfaceDescriptor());
  data.writeInt32(server_if);

  remote()->transact(IBluetoothGattServer::END_SERVICE_DECLARATION_TRANSACTION,
                     data, &reply);

  return reply.readInt32();
}

bool BpBluetoothGattServer::SendResponse(
    int server_if,
    const std::string& device_address,
    int request_id,
    int status, int offset,
    const std::vector<uint8_t>& value) {
  Parcel data, reply;

  data.writeInterfaceToken(IBluetoothGattServer::getInterfaceDescriptor());
  data.writeInt32(server_if);
  data.writeCString(device_address.c_str());
  data.writeInt32(request_id);
  data.writeInt32(status);
  data.writeInt32(offset);
  data.writeByteVector(value);

  remote()->transact(IBluetoothGattServer::SEND_RESPONSE_TRANSACTION,
                     data, &reply);

  return reply.readInt32();
}

bool BpBluetoothGattServer::SendNotification(
    int server_if,
    const std::string& device_address,
    const bluetooth::GattIdentifier& characteristic_id,
    bool confirm,
    const std::vector<uint8_t>& value) {
  Parcel data, reply;

  data.writeInterfaceToken(IBluetoothGattServer::getInterfaceDescriptor());
  data.writeInt32(server_if);
  data.writeCString(device_address.c_str());
  WriteGattIdentifierToParcel(characteristic_id, &data);
  data.writeInt32(confirm);
  data.writeByteVector(value);

  remote()->transact(IBluetoothGattServer::SEND_NOTIFICATION_TRANSACTION,
                     data, &reply);

  return reply.readInt32();
}

IMPLEMENT_META_INTERFACE(BluetoothGattServer,
                         IBluetoothGattServer::kServiceName);

}  // namespace binder
}  // namespace ipc
