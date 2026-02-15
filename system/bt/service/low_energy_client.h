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

#include <atomic>
#include <functional>
#include <map>
#include <mutex>

#include <base/macros.h>

#include "service/bluetooth_instance.h"
#include "service/common/bluetooth/advertise_data.h"
#include "service/common/bluetooth/advertise_settings.h"
#include "service/common/bluetooth/low_energy_constants.h"
#include "service/common/bluetooth/scan_filter.h"
#include "service/common/bluetooth/scan_result.h"
#include "service/common/bluetooth/scan_settings.h"
#include "service/common/bluetooth/uuid.h"
#include "service/hal/bluetooth_gatt_interface.h"

namespace bluetooth {

struct ConnComparator {
    bool operator()(const bt_bdaddr_t& a, const bt_bdaddr_t& b) const {
        return memcmp(&a, &b, sizeof(bt_bdaddr_t)) < 0;
    }
};

class Adapter;

// A LowEnergyClient represents an application's handle to perform various
// Bluetooth Low Energy GAP operations. Instances cannot be created directly and
// should be obtained through the factory.
class LowEnergyClient : private hal::BluetoothGattInterface::ClientObserver,
                        public BluetoothInstance {
 public:
  // The Delegate interface is used to notify asynchronous events related to BLE
  // GAP operations.
  class Delegate {
   public:
    Delegate() = default;
    virtual ~Delegate() = default;

    // Called asynchronously to notify the delegate of nearby BLE advertisers
    // found during a device scan.
    virtual void OnScanResult(LowEnergyClient* client,
                              const ScanResult& scan_result) = 0;

    // Called asynchronously to notify the delegate of connection state change
    virtual void OnConnectionState(LowEnergyClient* client, int status,
                                   const char* address, bool connected) = 0;

    // Called asynchronously to notify the delegate of mtu change
    virtual void OnMtuChanged(LowEnergyClient* client, int status, const char* address,
                              int mtu) = 0;

    // Called asynchronously to notify that services have been discovered
    virtual void OnServicesDiscovered(LowEnergyClient* client, int status,
				      const char* address) = 0;

    // Called asynchronously to notify that the gatt db has been updated
    virtual void OnGattDbUpdated(LowEnergyClient* client, const char* address,
				 btgatt_db_element_t* db, int size) = 0;

    // Called asynchronously to notify that the gatt characteristic has been read
    virtual void OnCharacteristicRead(LowEnergyClient* client, const char* address,
                                      int status, btgatt_read_params_t* data) = 0;

    // Called asynchronously to notify that the gatt characteristic has been written
    virtual void OnCharacteristicWrite(LowEnergyClient* client, const char* address,
                                       int status, uint16_t handle) = 0;

    // Called asynchronously to notify that the gatt descriptor has been written
    virtual void OnDescriptorWrite(LowEnergyClient* client, const char* address,
                                   int status, uint16_t handle) = 0;

    // Called asynchronously to inform whether or not the client is registered
    // for notifications on a particular characteristic
    virtual void
        OnCharacteristicNotificationRegistration(LowEnergyClient* client,
                                                 const char* address,
                                                 int registered,
                                                 int status,
                                                 uint16_t handle) = 0;

    // Called asynchronously to inform the client that the remote device
    // sent an indication or notification that the client is registered for
    virtual void
        OnCharacteristicChanged(LowEnergyClient* client,
                                const char* address,
                                btgatt_notify_params_t *p_data) = 0;

   private:
    DISALLOW_COPY_AND_ASSIGN(Delegate);
  };

  // The destructor automatically unregisters this client instance from the
  // stack.
  ~LowEnergyClient() override;

  // Assigns a delegate to this instance. |delegate| must out-live this
  // LowEnergyClient instance.
  void SetDelegate(Delegate* delegate);

  // Callback type used to return the result of asynchronous operations below.
  using StatusCallback = std::function<void(BLEStatus)>;

  // Initiates a BLE connection do device with address |address|. If
  // |is_direct| is set, use direct connect procedure. Return true on success
  //, false otherwise.
  bool Connect(std::string address, bool is_direct);

  // Disconnect from previously connected BLE device with address |address|.
  // Return true on success, false otherwise.
  bool Disconnect(std::string address);

  // Sends request to set MTU to |mtu| for device with address |address|.
  // Return true on success, false otherwise.
  bool SetMtu(std::string address, int mtu);

  // Sends request to discover services for device with address |address|.
  // Return true on success, false otherwise.
  bool DiscoverServices(std::string address);

  // Sends request to get GATT db for device with address |address|.
  // Return true on success, false otherwise.
  bool GetGattDb(std::string address);

  // Sends request to read GATT characteristic for device with address |address|
  // and handle |handle|.
  // Return true on success, false otherwise.
  bool ReadCharacteristic(std::string address, int handle);

  // Sends request to write GATT characteristic for device with address |address|
  // and handle |handle|.
  // Return true on success, false otherwise.
  bool WriteCharacteristic(std::string address, int handle, int write_type,
                           const std::vector<uint8_t>& value);

  // Sends request to write GATT descriptor for device with address |address|
  // and handle |handle|.
  // Return true on success, false otherwise.
  bool WriteDescriptor(std::string address, int handle, int write_type,
                       const std::vector<uint8_t>& value);

  // Initiates a BLE device scan for this client using the given |settings| and
  // |filters|. See the documentation for ScanSettings and ScanFilter for how
  // these parameters can be configured. Return true on success, false
  // otherwise. Please see logs for details in case of error.
  bool StartScan(const ScanSettings& settings,
                 const std::vector<ScanFilter>& filters);

  // Stops an ongoing BLE device scan for this client.
  bool StopScan();

  // Starts advertising based on the given advertising and scan response
  // data and the provided |settings|. Reports the result of the operation in
  // |callback|. Return true on success, false otherwise. Please see logs for
  // details in case of error.
  bool StartAdvertising(const AdvertiseSettings& settings,
                        const AdvertiseData& advertise_data,
                        const AdvertiseData& scan_response,
                        const StatusCallback& callback);

  // Stops advertising if it was already started. Reports the result of the
  // operation in |callback|.
  bool StopAdvertising(const StatusCallback& callback);

  // Returns true if advertising has been started.
  bool IsAdvertisingStarted() const;

  // Returns the state of pending advertising operations.
  bool IsStartingAdvertising() const;
  bool IsStoppingAdvertising() const;

  // Returns the current advertising settings.
  const AdvertiseSettings& advertise_settings() const {
    return advertise_settings_;
  }

  // Returns the current scan settings.
  const ScanSettings& scan_settings() const { return scan_settings_; }

  // BluetoothClientInstace overrides:
  const UUID& GetAppIdentifier() const override;
  int GetInstanceId() const override;

 private:
  friend class LowEnergyClientFactory;

  // Constructor shouldn't be called directly as instances are meant to be
  // obtained from the factory.
  LowEnergyClient(Adapter& adapter, const UUID& uuid, int client_id);

  // BluetoothGattInterface::ClientObserver overrides:
  void ScanResultCallback(
      hal::BluetoothGattInterface* gatt_iface,
      const bt_bdaddr_t& bda, int rssi, uint8_t* adv_data) override;

  void ConnectCallback(
      hal::BluetoothGattInterface* gatt_iface, int conn_id, int status,
      int client_id, const bt_bdaddr_t& bda) override;
  void DisconnectCallback(
      hal::BluetoothGattInterface* gatt_iface, int conn_id, int status,
      int client_id, const bt_bdaddr_t& bda) override;
  void MtuChangedCallback(
      hal::BluetoothGattInterface* gatt_iface, int conn_id, int status,
      int mtu) override;
  void SearchCompleteCallback(
      hal::BluetoothGattInterface* gatt_iface, int conn_id,
      int status) override;
  void RegisterForNotificationCallback(
      hal::BluetoothGattInterface* gatt_iface,
      int conn_id, int registered, int status, uint16_t handle) override;
  void NotifyCallback(
      hal::BluetoothGattInterface* gatt_iface,
      int conn_id, btgatt_notify_params_t *p_data) override;
  void GetGattDbCallback(
      hal::BluetoothGattInterface* gatt_iface, int conn_id,
      btgatt_db_element_t *db, int size) override;
  void ReadCharacteristicCallback(
      hal::BluetoothGattInterface* gatt_iface, int conn_id, int status,
      btgatt_read_params_t *data) override;
  void WriteCharacteristicCallback(
      hal::BluetoothGattInterface* gatt_iface, int conn_id, int status,
      uint16_t handle) override;
  void WriteDescriptorCallback(
      hal::BluetoothGattInterface* gatt_iface, int conn_id, int status,
      uint16_t handle) override;
  void MultiAdvEnableCallback(
      hal::BluetoothGattInterface* gatt_iface,
      int client_id, int status) override;
  void MultiAdvDataCallback(
      hal::BluetoothGattInterface* gatt_iface,
      int client_id, int status) override;
  void MultiAdvDisableCallback(
      hal::BluetoothGattInterface* gatt_iface,
      int client_id, int status) override;

  // Helper method called from SetAdvertiseData/SetScanResponse.
  bt_status_t SetAdvertiseData(
      hal::BluetoothGattInterface* gatt_iface,
      const AdvertiseData& data,
      bool set_scan_rsp);

  // Handles deferred advertise/scan-response data updates. We set the data if
  // there's data to be set, otherwise we either defer it if advertisements
  // aren't enabled or do nothing.
  void HandleDeferredAdvertiseData(hal::BluetoothGattInterface* gatt_iface);

  // Calls and clears the pending callbacks.
  void InvokeAndClearStartCallback(BLEStatus status);
  void InvokeAndClearStopCallback(BLEStatus status);

  // Raw pointer to the Bluetooth Adapter.
  Adapter& adapter_;

  // See getters above for documentation.
  UUID app_identifier_;
  int client_id_;

  // Protects advertising-related members below.
  std::mutex adv_fields_lock_;

  // The advertising and scan response data fields that will be sent to the
  // controller.
  AdvertiseData adv_data_;
  AdvertiseData scan_response_;
  std::atomic_bool adv_data_needs_update_;
  std::atomic_bool scan_rsp_needs_update_;

  // Latest advertising settings.
  AdvertiseSettings advertise_settings_;

  // Whether or not there is a pending call to update advertising or scan
  // response data.
  std::atomic_bool is_setting_adv_data_;

  std::atomic_bool adv_started_;
  std::unique_ptr<StatusCallback> adv_start_callback_;
  std::unique_ptr<StatusCallback> adv_stop_callback_;

  // Protects device scan related members below.
  std::mutex scan_fields_lock_;

  // Current scan settings.
  ScanSettings scan_settings_;

  // If true, then this client have a BLE device scan in progress.
  std::atomic_bool scan_started_;

  // Raw handle to the Delegate, which must outlive this LowEnergyClient
  // instance.
  std::mutex delegate_mutex_;
  Delegate* delegate_;

  // Protects device connection related members below.
  std::mutex connection_fields_lock_;

  // Maps bluetooth address to connection id
  //TODO(jpawlowski): change type to bimap
  std::map<const bt_bdaddr_t, int, ConnComparator> connection_ids_;

  DISALLOW_COPY_AND_ASSIGN(LowEnergyClient);
};

// LowEnergyClientFactory is used to register and obtain a per-application
// LowEnergyClient instance. Users should call RegisterInstance to obtain their
// own unique LowEnergyClient instance that has been registered with the
// Bluetooth stack.
class LowEnergyClientFactory
    : private hal::BluetoothGattInterface::ClientObserver,
      public BluetoothInstanceFactory {
 public:
  // Don't construct/destruct directly except in tests. Instead, obtain a handle
  // from an Adapter instance.
  LowEnergyClientFactory(Adapter& adapter);
  ~LowEnergyClientFactory() override;

  // BluetoothInstanceFactory override:
  bool RegisterInstance(const UUID& uuid,
                        const RegisterCallback& callback) override;

 private:
  friend class LowEnergyClient;

  // BluetoothGattInterface::ClientObserver overrides:
  void RegisterClientCallback(
      hal::BluetoothGattInterface* gatt_iface,
      int status, int client_id,
      const bt_uuid_t& app_uuid) override;

  // Map of pending calls to register.
  std::mutex pending_calls_lock_;
  std::map<UUID, RegisterCallback> pending_calls_;

  // Raw pointer to the Adapter that owns this factory.
  Adapter& adapter_;

  DISALLOW_COPY_AND_ASSIGN(LowEnergyClientFactory);
};

}  // namespace bluetooth
