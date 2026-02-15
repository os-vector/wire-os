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

#include <base/macros.h>
#include <binder/IBinder.h>
#include <binder/IInterface.h>

#include <bluetooth/advertise_data.h>
#include <bluetooth/advertise_settings.h>
#include <bluetooth/scan_filter.h>
#include <bluetooth/scan_settings.h>
#include <bluetooth/binder/IBluetoothLowEnergyCallback.h>

namespace ipc {
namespace binder {

// This class defines the Binder IPC interface for interacting with Bluetooth
// Low-Energy features.
// TODO(armansito): This class was written based on a new design doc proposal.
// We need to add an AIDL for this to the framework code.
//
// NOTE: KEEP THIS FILE UP-TO-DATE with the corresponding AIDL, otherwise this
// won't be compatible with the Android framework.
class IBluetoothLowEnergy : public android::IInterface {
 public:
  DECLARE_META_INTERFACE(BluetoothLowEnergy);

  static const char kServiceName[];

  // Transaction codes for interface methods.
  enum {
    GET_DEVICES_MATCHING_CONNECTION_STATE_TRANSACTION =
        android::IBinder::FIRST_CALL_TRANSACTION,

    REGISTER_CLIENT_TRANSACTION,
    UNREGISTER_CLIENT_TRANSACTION,
    UNREGISTER_ALL_TRANSACTION,

    START_SCAN_TRANSACTION,
    STOP_SCAN_TRANSACTION,
    FLUSH_PENDING_BATCH_RESULTS_TRANSACTION,
    START_MULTI_ADVERTISING_TRANSACTION,
    STOP_MULTI_ADVERTISING_TRANSACTION,

    CONNECT_TRANSACTION,
    DISCONNECT_TRANSACTION,
    SET_MTU_TRANSACTION,
    DISCOVER_SERVICES_TRANSACTION,
    READ_REMOTE_RSSI_TRANSACTION,
    CONFIGURE_ATT_MTU_TRANSACTION,
    CONNECTION_PARAMETER_UPDATE_TRANSACTION,
    DISCONNECT_ALL_TRANSACTION,
    GET_GATT_DB_TRANSACTION,
    READ_CHARACTERISTIC_TRANSACTION,
    WRITE_CHARACTERISTIC_TRANSACTION,
    WRITE_DESCRIPTOR_TRANSACTION,

    NUM_HW_TRACK_FILTERS_AVAILABLE,
  };

  virtual bool RegisterClient(
      const android::sp<IBluetoothLowEnergyCallback>& callback) = 0;
  virtual void UnregisterClient(int client_if) = 0;
  virtual void UnregisterAll() = 0;

  virtual bool Connect(int client_id, const char* address, bool is_direct) = 0;
  virtual bool Disconnect(int client_id, const char* address) = 0;

  virtual bool SetMtu(int client_id, const char* address, int mtu) = 0;

  virtual bool DiscoverServices(int client_id, const char* address) = 0;
  virtual bool GetGattDb(int client_id, const char* address) = 0;
  virtual bool ReadCharacteristic(int client_id, const char* address, int handle) = 0;
  virtual bool WriteCharacteristic(int client_id, const char* address, int handle,
                                   int write_type, const std::vector<uint8_t>& value) = 0;
  virtual bool WriteDescriptor(int client_id, const char* address, int handle,
                               int write_type, const std::vector<uint8_t>& value) = 0;


  virtual bool StartScan(
      int client_id,
      const bluetooth::ScanSettings& settings,
      const std::vector<bluetooth::ScanFilter>& filters) = 0;
  virtual bool StopScan(int client_id) = 0;

  virtual bool StartMultiAdvertising(
      int client_if,
      const bluetooth::AdvertiseData& advertise_data,
      const bluetooth::AdvertiseData& scan_response,
      const bluetooth::AdvertiseSettings& settings) = 0;
  virtual bool StopMultiAdvertising(int client_if) = 0;

  // TODO(armansito): Complete the API definition.

 private:
  DISALLOW_COPY_AND_ASSIGN(IBluetoothLowEnergy);
};

// The Binder server interface to IBluetoothLowEnergy. A class that implements
// IBluetoothLowEnergy must inherit from this class.
class BnBluetoothLowEnergy : public android::BnInterface<IBluetoothLowEnergy> {
 public:
  BnBluetoothLowEnergy() = default;
  virtual ~BnBluetoothLowEnergy() = default;

 private:
  virtual android::status_t onTransact(
      uint32_t code,
      const android::Parcel& data,
      android::Parcel* reply,
      uint32_t flags = 0);

  DISALLOW_COPY_AND_ASSIGN(BnBluetoothLowEnergy);
};

// The Binder client interface to IBluetoothLowEnergy.
class BpBluetoothLowEnergy : public android::BpInterface<IBluetoothLowEnergy> {
 public:
  explicit BpBluetoothLowEnergy(const android::sp<android::IBinder>& impl);
  virtual ~BpBluetoothLowEnergy() = default;

  // IBluetoothLowEnergy overrides:
  bool RegisterClient(
      const android::sp<IBluetoothLowEnergyCallback>& callback) override;
  void UnregisterClient(int client_if) override;
  void UnregisterAll() override;

  bool Connect(int client_id, const char* address, bool is_direct) override;
  bool Disconnect(int client_id, const char* address) override;

  bool SetMtu(int client_id, const char* address, int mtu) override;

  bool DiscoverServices(int client_id, const char* address) override;
  bool GetGattDb(int client_id, const char* address) override;
  bool ReadCharacteristic(int client_id, const char* address, int handle) override;
  bool WriteCharacteristic(int client_id, const char* address, int handle,
                           int write_type, const std::vector<uint8_t>& value) override;
  bool WriteDescriptor(int client_id, const char* address, int handle,
                       int write_type, const std::vector<uint8_t>& value) override;
  bool StartScan(
      int client_id,
      const bluetooth::ScanSettings& settings,
      const std::vector<bluetooth::ScanFilter>& filters) override;
  bool StopScan(int client_id) override;
  bool StartMultiAdvertising(
      int client_if,
      const bluetooth::AdvertiseData& advertise_data,
      const bluetooth::AdvertiseData& scan_response,
      const bluetooth::AdvertiseSettings& settings) override;
  bool StopMultiAdvertising(int client_if) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(BpBluetoothLowEnergy);
};

}  // namespace binder
}  // namespace ipc
