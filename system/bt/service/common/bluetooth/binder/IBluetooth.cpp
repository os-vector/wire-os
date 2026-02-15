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

#include "service/common/bluetooth/binder/IBluetooth.h"

#include <base/logging.h>
#include <binder/IServiceManager.h>
#include <binder/Parcel.h>

using android::defaultServiceManager;
using android::IBinder;
using android::interface_cast;
using android::IServiceManager;
using android::Parcel;
using android::PERMISSION_DENIED;
using android::sp;
using android::status_t;
using android::String16;

namespace ipc {
namespace binder {

// static
const char IBluetooth::kServiceName[] = "bluetooth-service";

// static
sp<IBluetooth> IBluetooth::getClientInterface() {
  sp<IServiceManager> sm = defaultServiceManager();
  if (!sm.get()) {
    LOG(ERROR) << "Failed to obtain a handle to the default Service Manager";
    return nullptr;
  }

  sp<IBinder> binder = sm->getService(String16(kServiceName));
  if (!binder.get()) {
    LOG(ERROR) << "Failed to obtain a handle to the Bluetooth service";
    return nullptr;
  }

  sp<IBluetooth> bt_iface = interface_cast<IBluetooth>(binder);
  if (!bt_iface.get()) {
    LOG(ERROR) << "Obtained invalid IBinder handle";
    return nullptr;
  }

  return bt_iface;
}

// BnBluetooth (server) implementation
// ========================================================

status_t BnBluetooth::onTransact(
    uint32_t code, const Parcel& data, Parcel* reply, uint32_t flags) {
  VLOG(2) << "IBluetooth transaction: " << code;

  switch (code) {
    case IS_ENABLED_TRANSACTION: {
      CHECK_INTERFACE(IBluetooth, data, reply);
      bool is_enabled = IsEnabled();
      reply->writeInt32(is_enabled);
      return android::NO_ERROR;
    }
    case GET_STATE_TRANSACTION: {
      CHECK_INTERFACE(IBluetooth, data, reply);
      int state = GetState();
      reply->writeInt32(state);
      return android::NO_ERROR;
    }
    case ENABLE_TRANSACTION: {
      CHECK_INTERFACE(IBluetooth, data, reply);
      bool start_restricted = data.readBool();
      bool result = Enable(start_restricted);
      reply->writeInt32(result);
      return android::NO_ERROR;
    }
    case DISABLE_TRANSACTION: {
      CHECK_INTERFACE(IBluetooth, data, reply);
      bool result = Disable();
      reply->writeInt32(result);
      return android::NO_ERROR;
    }
    case GET_ADDRESS_TRANSACTION: {
      CHECK_INTERFACE(IBluetooth, data, reply);
      std::string address = GetAddress();
      reply->writeCString(address.c_str());
      return android::NO_ERROR;
    }
    case GET_UUIDS_TRANSACTION:
      CHECK_INTERFACE(IBluetooth, data, reply);
      // TODO(armansito): Figure out how to handle a Java "ParcelUuid" natively.
      // (see http://b/23316698).
      return android::INVALID_OPERATION;

    case SET_NAME_TRANSACTION: {
      CHECK_INTERFACE(IBluetooth, data, reply);
      std::string name(data.readCString());
      bool result = SetName(name);
      reply->writeInt32(result);
      return android::NO_ERROR;
    }
    case GET_NAME_TRANSACTION: {
      CHECK_INTERFACE(IBluetooth, data, reply);
      std::string name = GetName();
      reply->writeCString(name.c_str());
      return android::NO_ERROR;
    }
    case REGISTER_CALLBACK_TRANSACTION: {
      CHECK_INTERFACE(IBluetooth, data, reply);
      sp<IBinder> callback = data.readStrongBinder();
      RegisterCallback(interface_cast<IBluetoothCallback>(callback));
      return android::NO_ERROR;
    }
    case UNREGISTER_CALLBACK_TRANSACTION: {
      CHECK_INTERFACE(IBluetooth, data, reply);
      sp<IBinder> callback = data.readStrongBinder();
      UnregisterCallback(interface_cast<IBluetoothCallback>(callback));
      return android::NO_ERROR;
    }
    case IS_MULTI_ADVERTISEMENT_SUPPORTED_TRANSACTION: {
      CHECK_INTERFACE(IBluetooth, data, reply);
      bool result = IsMultiAdvertisementSupported();
      reply->writeInt32(result);
      return android::NO_ERROR;
    }
    case GET_LOW_ENERGY_INTERFACE_TRANSACTION: {
      CHECK_INTERFACE(IBluetooth, data, reply);
      sp<IBluetoothLowEnergy> ble_iface = GetLowEnergyInterface();
      reply->writeStrongBinder(IInterface::asBinder(ble_iface.get()));
      return android::NO_ERROR;
    }
    case GET_GATT_CLIENT_INTERFACE_TRANSACTION: {
      CHECK_INTERFACE(IBluetooth, data, reply);
      sp<IBluetoothGattClient> gatt_client_iface = GetGattClientInterface();
      reply->writeStrongBinder(IInterface::asBinder(gatt_client_iface.get()));
      return android::NO_ERROR;
    }
    case GET_GATT_SERVER_INTERFACE_TRANSACTION: {
      CHECK_INTERFACE(IBluetooth, data, reply);
      sp<IBluetoothGattServer> gatt_server_iface = GetGattServerInterface();
      reply->writeStrongBinder(IInterface::asBinder(gatt_server_iface.get()));
      return android::NO_ERROR;
    }
    default:
      return BBinder::onTransact(code, data, reply, flags);
  }
}

// BpBluetooth (client) implementation
// ========================================================

BpBluetooth::BpBluetooth(const sp<IBinder>& impl)
    : BpInterface<IBluetooth>(impl) {
}

bool BpBluetooth::IsEnabled() {
  Parcel data, reply;

  data.writeInterfaceToken(IBluetooth::getInterfaceDescriptor());
  remote()->transact(IBluetooth::IS_ENABLED_TRANSACTION, data, &reply);

  return reply.readInt32();
}

int BpBluetooth::GetState() {
  Parcel data, reply;

  data.writeInterfaceToken(IBluetooth::getInterfaceDescriptor());
  remote()->transact(IBluetooth::GET_STATE_TRANSACTION, data, &reply);

  return reply.readInt32();
}

bool BpBluetooth::Enable(bool start_restricted) {
  Parcel data, reply;

  data.writeInterfaceToken(IBluetooth::getInterfaceDescriptor());
  data.writeBool(start_restricted);
  remote()->transact(IBluetooth::ENABLE_TRANSACTION, data, &reply);

  return reply.readInt32();
}

bool BpBluetooth::EnableNoAutoConnect() {
  Parcel data, reply;

  data.writeInterfaceToken(IBluetooth::getInterfaceDescriptor());
  remote()->transact(IBluetooth::ENABLE_NO_AUTO_CONNECT_TRANSACTION,
                     data, &reply);

  return reply.readInt32();
}

bool BpBluetooth::Disable() {
  Parcel data, reply;

  data.writeInterfaceToken(IBluetooth::getInterfaceDescriptor());
  remote()->transact(IBluetooth::DISABLE_TRANSACTION, data, &reply);

  return reply.readInt32();
}

std::string BpBluetooth::GetAddress() {
  Parcel data, reply;

  data.writeInterfaceToken(IBluetooth::getInterfaceDescriptor());
  remote()->transact(IBluetooth::GET_ADDRESS_TRANSACTION, data, &reply);

  return reply.readCString();
}

std::vector<bluetooth::UUID> BpBluetooth::GetUUIDs() {
  // TODO(armansito): need to deserialize a parceled java.util.ParcelUUID[] to
  // std::vector<bluetooth::UUID> here (see http://b/23316698).
  return std::vector<bluetooth::UUID>();
}

bool BpBluetooth::SetName(const std::string& name) {
  Parcel data, reply;

  data.writeInterfaceToken(IBluetooth::getInterfaceDescriptor());
  data.writeCString(name.c_str());
  remote()->transact(IBluetooth::SET_NAME_TRANSACTION, data, &reply);

  return reply.readInt32();
}

std::string BpBluetooth::GetName() {
  Parcel data, reply;

  data.writeInterfaceToken(IBluetooth::getInterfaceDescriptor());
  remote()->transact(IBluetooth::GET_NAME_TRANSACTION, data, &reply);

  return reply.readCString();
}

void BpBluetooth::RegisterCallback(const sp<IBluetoothCallback>& callback) {
  Parcel data, reply;

  data.writeInterfaceToken(IBluetooth::getInterfaceDescriptor());
  data.writeStrongBinder(IInterface::asBinder(callback.get()));

  remote()->transact(IBluetooth::REGISTER_CALLBACK_TRANSACTION, data, &reply);
}

void BpBluetooth::UnregisterCallback(const sp<IBluetoothCallback>& callback) {
  Parcel data, reply;

  data.writeInterfaceToken(IBluetooth::getInterfaceDescriptor());
  data.writeStrongBinder(IInterface::asBinder(callback.get()));

  remote()->transact(IBluetooth::UNREGISTER_CALLBACK_TRANSACTION, data, &reply);
}

bool BpBluetooth::IsMultiAdvertisementSupported() {
  Parcel data, reply;

  data.writeInterfaceToken(IBluetooth::getInterfaceDescriptor());

  remote()->transact(IBluetooth::IS_MULTI_ADVERTISEMENT_SUPPORTED_TRANSACTION,
                     data, &reply);

  return reply.readInt32();
}

sp<IBluetoothLowEnergy> BpBluetooth::GetLowEnergyInterface() {
  Parcel data, reply;

  data.writeInterfaceToken(IBluetooth::getInterfaceDescriptor());

  remote()->transact(IBluetooth::GET_LOW_ENERGY_INTERFACE_TRANSACTION,
                     data, &reply);

  return interface_cast<IBluetoothLowEnergy>(reply.readStrongBinder());
}

sp<IBluetoothGattClient> BpBluetooth::GetGattClientInterface() {
  Parcel data, reply;

  data.writeInterfaceToken(IBluetooth::getInterfaceDescriptor());

  remote()->transact(IBluetooth::GET_GATT_CLIENT_INTERFACE_TRANSACTION,
                     data, &reply);

  return interface_cast<IBluetoothGattClient>(reply.readStrongBinder());
}

sp<IBluetoothGattServer> BpBluetooth::GetGattServerInterface() {
  Parcel data, reply;

  data.writeInterfaceToken(IBluetooth::getInterfaceDescriptor());

  remote()->transact(IBluetooth::GET_GATT_SERVER_INTERFACE_TRANSACTION,
                     data, &reply);

  return interface_cast<IBluetoothGattServer>(reply.readStrongBinder());
}

IMPLEMENT_META_INTERFACE(Bluetooth, IBluetooth::kServiceName);

}  // namespace binder
}  // namespace ipc
