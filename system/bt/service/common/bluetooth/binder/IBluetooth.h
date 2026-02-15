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

#pragma once

#include <string>
#include <vector>

#include <base/macros.h>
#include <binder/IBinder.h>
#include <binder/IInterface.h>

#include <bluetooth/binder/IBluetoothCallback.h>
#include <bluetooth/binder/IBluetoothGattClient.h>
#include <bluetooth/binder/IBluetoothGattServer.h>
#include <bluetooth/binder/IBluetoothLowEnergy.h>
#include <bluetooth/uuid.h>

namespace ipc {
namespace binder {

// This class defines the Binder IPC interface for accessing the Bluetooth
// service. This class was written based on the corresponding AIDL file at
// /frameworks/base/core/java/android/bluetooth/IBluetooth.aidl.
//
// NOTE: KEEP THIS FILE UP-TO-DATE with the corresponding AIDL, otherwise this
// won't be compatible with the Android framework.
class IBluetooth : public android::IInterface {
 public:
  DECLARE_META_INTERFACE(Bluetooth);

  static const char kServiceName[];

  // Transaction codes for interface methods.
  enum {
    IS_ENABLED_TRANSACTION = android::IBinder::FIRST_CALL_TRANSACTION,
    GET_STATE_TRANSACTION,
    ENABLE_TRANSACTION,
    ENABLE_NO_AUTO_CONNECT_TRANSACTION,
    DISABLE_TRANSACTION,

    GET_ADDRESS_TRANSACTION,
    GET_UUIDS_TRANSACTION,  // TODO(armansito): Support this
    SET_NAME_TRANSACTION,
    GET_NAME_TRANSACTION,

    // TODO(armansito): Support the functions below.

    GET_SCAN_MODE_TRANSACTION,
    SET_SCAN_MODE_TRANSACTION,

    GET_DISCOVERABLE_TIMEOUT_TRANSACTION,
    SET_DISCOVERABLE_TIMEOUT_TRANSACTION,

    START_DISCOVERY_TRANSACTION,
    CANCEL_DISCOVERY_TRANSACTION,
    IS_DISCOVERING_TRANSACTION,

    GET_ADAPTER_CONNECTION_STATE_TRANSACTION,
    GET_PROFILE_CONNECTION_STATE_TRANSACTION,

    GET_BONDED_DEVICES_TRANSACTION,
    CREATE_BOND_TRANSACTION,
    CANCEL_BOND_PROCESS_TRANSACTION,
    REMOVE_BOND_TRANSACTION,
    GET_BOND_STATE_TRANSACTION,
    GET_CONNECTION_STATE_TRANSACTION,

    GET_REMOTE_NAME_TRANSACTION,
    GET_REMOTE_TYPE_TRANSACTION,
    GET_REMOTE_ALIAS_TRANSACTION,
    SET_REMOTE_ALIAS_TRANSACTION,
    GET_REMOTE_CLASS_TRANSACTION,
    GET_REMOTE_UUIDS_TRANSACTION,
    FETCH_REMOTE_UUIDS_TRANSACTION,
    SDP_SEARCH_TRANSACTION,

    SET_PIN_TRANSACTION,
    SET_PASSKEY_TRANSACTION,
    SET_PAIRING_CONFIRMATION_TRANSACTION,

    GET_PHONEBOOK_ACCESS_PERMISSION_TRANSACTION,
    SET_PHONEBOOK_ACCESS_PERMISSION_TRANSACTION,
    GET_MESSAGE_ACCESS_PERMISSION_TRANSACTION,
    SET_MESSAGE_ACCESS_PERMISSION_TRANSACTION,
    GET_SIM_ACCESS_PERMISSION_TRANSACTION,
    SET_SIM_ACCESS_PERMISSION_TRANSACTION,

    SEND_CONNECTION_STATE_CHANGE_TRANSACTION,

    REGISTER_CALLBACK_TRANSACTION,
    UNREGISTER_CALLBACK_TRANSACTION,

    CONNECT_SOCKET_TRANSACTION,
    CREATE_SOCKET_CHANNEL_TRANSACTION,

    CONFIG_HCI_SNOOP_LOG,
    FACTORY_RESET_TRANSACTION,

    IS_MULTI_ADVERTISEMENT_SUPPORTED_TRANSACTION,
    IS_PERIPHERAL_MODE_SUPPORTED_TRANSACTION,
    IS_OFFLOADED_FILTERING_SUPPORTED_TRANSACTION,
    IS_OFFLOADED_SCAN_BATCHING_SUPPORTED_TRANSACTION,
    IS_ACTIVITY_AND_ENERGY_REPORTING_SUPPORTED_TRANSACTION,
    GET_ACTIVITY_ENERGY_INFO_FROM_CONTROLLER_TRANSACTION,
    REPORT_ACTIVITY_INFO_TRANSACTION,

    ON_LE_SERVICE_UP_TRANSACTION,
    ON_BR_EDR_DOWN_TRANSACTION,

    GET_LOW_ENERGY_INTERFACE_TRANSACTION,
    GET_GATT_CLIENT_INTERFACE_TRANSACTION,
    GET_GATT_SERVER_INTERFACE_TRANSACTION,
  };

  // Returns a handle to the IBluetooth Binder from the Android ServiceManager.
  // Binder client code can use this to make calls to the service.
  static android::sp<IBluetooth> getClientInterface();

  // Methods declared in IBluetooth.aidl.

  virtual bool IsEnabled() = 0;
  virtual int GetState() = 0;
  virtual bool Enable(bool start_restricted) = 0;
  virtual bool EnableNoAutoConnect() = 0;
  virtual bool Disable() = 0;

  virtual std::string GetAddress() = 0;
  virtual std::vector<bluetooth::UUID> GetUUIDs() = 0;
  virtual bool SetName(const std::string& name) = 0;
  virtual std::string GetName() = 0;

  virtual void RegisterCallback(
      const android::sp<IBluetoothCallback>& callback) = 0;
  virtual void UnregisterCallback(
      const android::sp<IBluetoothCallback>& callback) = 0;

  virtual bool IsMultiAdvertisementSupported() = 0;

  virtual android::sp<IBluetoothLowEnergy> GetLowEnergyInterface() = 0;
  virtual android::sp<IBluetoothGattClient> GetGattClientInterface() = 0;
  virtual android::sp<IBluetoothGattServer> GetGattServerInterface() = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(IBluetooth);
};

// The Binder server interface to IBluetooth. A class that implements IBluetooth
// must inherit from this class.
class BnBluetooth : public android::BnInterface<IBluetooth> {
 public:
  BnBluetooth() = default;
  virtual ~BnBluetooth() = default;

 private:
  virtual android::status_t onTransact(
      uint32_t code,
      const android::Parcel& data,
      android::Parcel* reply,
      uint32_t flags = 0);

  DISALLOW_COPY_AND_ASSIGN(BnBluetooth);
};

// The Binder client interface to IBluetooth.
class BpBluetooth : public android::BpInterface<IBluetooth> {
 public:
  explicit BpBluetooth(const android::sp<android::IBinder>& impl);
  virtual ~BpBluetooth() = default;

  // IBluetooth overrides:
  bool IsEnabled() override;
  int GetState() override;
  bool Enable(bool start_restricted) override;
  bool EnableNoAutoConnect() override;
  bool Disable() override;

  std::string GetAddress() override;
  std::vector<bluetooth::UUID> GetUUIDs() override;
  bool SetName(const std::string& name) override;
  std::string GetName() override;

  void RegisterCallback(
      const android::sp<IBluetoothCallback>& callback) override;
  void UnregisterCallback(
      const android::sp<IBluetoothCallback>& callback) override;

  bool IsMultiAdvertisementSupported() override;

  android::sp<IBluetoothLowEnergy> GetLowEnergyInterface() override;
  android::sp<IBluetoothGattClient> GetGattClientInterface() override;
  android::sp<IBluetoothGattServer> GetGattServerInterface() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(BpBluetooth);
};

}  // namespace binder
}  // namespace ipc
