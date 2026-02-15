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

#include "service/adapter.h"

#include <atomic>
#include <mutex>
#include <string>
#include <unordered_set>

#include <base/logging.h>
#include <base/observer_list.h>

#include "service/common/bluetooth/util/atomic_string.h"
#include "service/gatt_client.h"
#include "service/gatt_server.h"
#include "service/hal/bluetooth_interface.h"
#include "service/logging_helpers.h"
#include "service/low_energy_client.h"

using std::lock_guard;
using std::mutex;

namespace bluetooth {

// static
const char Adapter::kDefaultAddress[] = "00:00:00:00:00:00";
// static
const char Adapter::kDefaultName[] = "not-initialized";

// TODO(armansito): The following constants come straight from
// packages/apps/Bluetooth/src/c/a/b/btservice/AdapterService.java. It would be
// nice to know if there were a way to obtain these values from the stack
// instead of hardcoding them here.

// The minimum number of advertising instances required for multi-advertisement
// support.
const int kMinAdvInstancesForMultiAdv = 5;

// Used when determining if offloaded scan filtering is supported.
const int kMinOffloadedFilters = 10;

// Used when determining if offloaded scan batching is supported.
const int kMinOffloadedScanStorageBytes = 1024;

void Adapter::Observer::OnAdapterStateChanged(Adapter* adapter,
                                              AdapterState prev_state,
                                              AdapterState new_state) {
  // Default implementation does nothing
}

void Adapter::Observer::OnDeviceConnectionStateChanged(
    Adapter* adapter, const std::string& device_address, bool connected) {
  // Default implementation does nothing
}

// The real Adapter implementation used in production.
class AdapterImpl : public Adapter,
                    public hal::BluetoothInterface::Observer {
 public:
  AdapterImpl()
    : state_(ADAPTER_STATE_OFF),
      address_(kDefaultAddress),
      name_(kDefaultName) {
    memset(&local_le_features_, 0, sizeof(local_le_features_));
    hal::BluetoothInterface::Get()->AddObserver(this);
    ble_client_factory_.reset(new LowEnergyClientFactory(*this));
    gatt_client_factory_.reset(new GattClientFactory());
    gatt_server_factory_.reset(new GattServerFactory());
    hal::BluetoothInterface::Get()->GetHALInterface()->get_adapter_properties();
  }

  ~AdapterImpl() override {
    hal::BluetoothInterface::Get()->RemoveObserver(this);
  }

  void AddObserver(Adapter::Observer* observer) override {
    lock_guard<mutex> lock(observers_lock_);
    observers_.AddObserver(observer);
  }

  void RemoveObserver(Adapter::Observer* observer) override {
    lock_guard<mutex> lock(observers_lock_);
    observers_.RemoveObserver(observer);
  }

  AdapterState GetState() const override {
    return state_.load();
  }

  bool IsEnabled() const override {
    return state_.load() == ADAPTER_STATE_ON;
  }

  bool Enable(bool start_restricted) override {
    AdapterState current_state = GetState();
    if (current_state != ADAPTER_STATE_OFF) {
      LOG(INFO) << "Adapter not disabled - state: "
                << AdapterStateToString(current_state);
      return false;
    }

    // Set the state before calling enable() as there might be a race between
    // here and the AdapterStateChangedCallback.
    state_ = ADAPTER_STATE_TURNING_ON;
    NotifyAdapterStateChanged(current_state, state_);

    int status = hal::BluetoothInterface::Get()->GetHALInterface()->enable(start_restricted);
    if (status != BT_STATUS_SUCCESS) {
      LOG(ERROR) << "Failed to enable Bluetooth - status: "
                 << BtStatusText((const bt_status_t)status);
      state_ = ADAPTER_STATE_OFF;
      NotifyAdapterStateChanged(ADAPTER_STATE_TURNING_ON, state_);
      return false;
    }

    return true;
  }

  bool Disable() override {
    if (!IsEnabled()) {
      LOG(INFO) << "Adapter is not enabled";
      return false;
    }

    AdapterState current_state = GetState();

    // Set the state before calling enable() as there might be a race between
    // here and the AdapterStateChangedCallback.
    state_ = ADAPTER_STATE_TURNING_OFF;
    NotifyAdapterStateChanged(current_state, state_);

    int status = hal::BluetoothInterface::Get()->GetHALInterface()->disable();
    if (status != BT_STATUS_SUCCESS) {
      LOG(ERROR) << "Failed to disable Bluetooth - status: "
                 << BtStatusText((const bt_status_t)status);
      state_ = current_state;
      NotifyAdapterStateChanged(ADAPTER_STATE_TURNING_OFF, state_);
      return false;
    }

    return true;
  }

  std::string GetName() const override {
    return name_.Get();
  }

  bool SetName(const std::string& name) override {
    bt_bdname_t hal_name;
    size_t max_name_len = sizeof(hal_name.name);

    // Include the \0 byte in size measurement.
    if (name.length() >= max_name_len) {
      LOG(ERROR) << "Given name \"" << name << "\" is larger than maximum"
                 << " allowed size: " << max_name_len;
      return false;
    }

    strncpy(reinterpret_cast<char*>(hal_name.name), name.c_str(),
            name.length() + 1);

    VLOG(1) << "Setting adapter name: " << name;

    if (!SetAdapterProperty(BT_PROPERTY_BDNAME, &hal_name, sizeof(hal_name))) {
      LOG(ERROR) << "Failed to set adapter name: " << name;
      return false;
    }

    return true;
  }

  std::string GetAddress() const override {
    return address_.Get();
  }

  bool IsMultiAdvertisementSupported() override {
    lock_guard<mutex> lock(local_le_features_lock_);
    return local_le_features_.max_adv_instance >= kMinAdvInstancesForMultiAdv;
  }

  bool IsDeviceConnected(const std::string& device_address) override {
    lock_guard<mutex> lock(connected_devices_lock_);
    return connected_devices_.find(device_address) != connected_devices_.end();
  }

  int GetTotalNumberOfTrackableAdvertisements() override {
    lock_guard<mutex> lock(local_le_features_lock_);
    return local_le_features_.total_trackable_advertisers;
  }

  bool IsOffloadedFilteringSupported() override {
    lock_guard<mutex> lock(local_le_features_lock_);
    return local_le_features_.max_adv_filter_supported >= kMinOffloadedFilters;
  }

  bool IsOffloadedScanBatchingSupported() override {
    lock_guard<mutex> lock(local_le_features_lock_);
    return local_le_features_.scan_result_storage_size >=
        kMinOffloadedScanStorageBytes;
  }

  LowEnergyClientFactory* GetLowEnergyClientFactory() const override {
    return ble_client_factory_.get();
  }

  GattClientFactory* GetGattClientFactory() const override {
    return gatt_client_factory_.get();
  }

  GattServerFactory* GetGattServerFactory() const override {
    return gatt_server_factory_.get();
  }

  // hal::BluetoothInterface::Observer overrides.
  void AdapterStateChangedCallback(bt_state_t state) override {
    VLOG(1) << "Adapter state changed: " << BtStateText(state);

    AdapterState prev_state = GetState();

    switch (state) {
    case BT_STATE_OFF:
      state_ = ADAPTER_STATE_OFF;
      break;

    case BT_STATE_ON:
      state_ = ADAPTER_STATE_ON;
      break;

    default:
      NOTREACHED();
    }

    NotifyAdapterStateChanged(prev_state, GetState());
  }

  void AdapterPropertiesCallback(bt_status_t status,
                                 int num_properties,
                                 bt_property_t* properties) override {
    VLOG(1) << "Adapter properties changed";

    if (status != BT_STATUS_SUCCESS) {
      LOG(ERROR) << "status: " << BtStatusText(status);
      return;
    }

    for (int i = 0; i < num_properties; i++) {
      bt_property_t* property = properties + i;
      switch (property->type) {
        case BT_PROPERTY_BDADDR: {
          std::string address = BtAddrString(reinterpret_cast<bt_bdaddr_t*>(
              property->val));
          VLOG(1) << "Adapter address changed: " << address;
          address_.Set(address);
          break;
        }
        case BT_PROPERTY_BDNAME: {
          bt_bdname_t* hal_name = reinterpret_cast<bt_bdname_t*>(property->val);
          std::string name = reinterpret_cast<char*>(hal_name->name);
          VLOG(1) << "Adapter name changed: " << name;
          name_.Set(name);
          break;
        }
        case BT_PROPERTY_LOCAL_LE_FEATURES: {
          lock_guard<mutex> lock(local_le_features_lock_);
          if (property->len != sizeof(bt_local_le_features_t)) {
            LOG(WARNING) << "Malformed value received for property: "
                         << "BT_PROPERTY_LOCAL_LE_FEATURES";
            break;
          }
          bt_local_le_features_t* features =
              reinterpret_cast<bt_local_le_features_t*>(property->val);
          memcpy(&local_le_features_, features, sizeof(*features));
          VLOG(1) << "Supported LE features updated";
          break;
        }
        default:
          VLOG(1) << "Unhandled adapter property: "
                  << BtPropertyText(property->type);
          break;
      }

      // TODO(armansito): notify others of the updated properties
    }
  }

  void AclStateChangedCallback(bt_status_t status,
                               const bt_bdaddr_t& remote_bdaddr,
                               bt_acl_state_t state) override {
    std::string device_address = BtAddrString(&remote_bdaddr);
    bool connected = (state == BT_ACL_STATE_CONNECTED);
    VLOG(1) << "ACL state changed: " << device_address << " - connected: "
            << (connected ? "true" : "false");

    // If this is reported with an error status, I suppose the best thing we can
    // do is to log it and ignore the event.
    if (status != BT_STATUS_SUCCESS) {
      LOG(ERROR) << "status: " << BtStatusText(status);
      return;
    }

    // Introduce a scope to manage |connected_devices_lock_| with RAII.
    {
      lock_guard<mutex> lock(connected_devices_lock_);
      if (connected)
        connected_devices_.insert(device_address);
      else
        connected_devices_.erase(device_address);
    }

    lock_guard<mutex> lock(observers_lock_);
    FOR_EACH_OBSERVER(
        Adapter::Observer, observers_,
        OnDeviceConnectionStateChanged(this, device_address, connected));
  }

  // Sends a request to set the given HAL adapter property type and value.
  bool SetAdapterProperty(bt_property_type_t type, void* value, int length) {
    CHECK(length > 0);
    CHECK(value);

    bt_property_t property;
    property.len = length;
    property.val = value;
    property.type = type;

    int status = hal::BluetoothInterface::Get()->GetHALInterface()->
        set_adapter_property(&property);
    if (status != BT_STATUS_SUCCESS) {
      VLOG(1) << "Failed to set property";
      return false;
    }

    return true;
  }

  // Helper for invoking the AdapterStateChanged observer method.
  void NotifyAdapterStateChanged(AdapterState prev_state,
                                 AdapterState new_state) {
    if (prev_state == new_state)
      return;

    lock_guard<mutex> lock(observers_lock_);
    FOR_EACH_OBSERVER(Adapter::Observer, observers_,
                      OnAdapterStateChanged(this, prev_state, new_state));
  }

 private:
  // The current adapter state.
  std::atomic<AdapterState> state_;

  // The Bluetooth device address of the local adapter in string from
  // (i.e.. XX:XX:XX:XX:XX:XX)
  util::AtomicString address_;

  // The current local adapter name.
  util::AtomicString name_;

  // The current set of supported LE features as obtained from the stack. The
  // values here are all initially set to 0 and updated when the corresponding
  // adapter property has been received from the stack.
  std::mutex local_le_features_lock_;
  bt_local_le_features_t local_le_features_;

  // List of observers that are interested in notifications from us.
  std::mutex observers_lock_;
  base::ObserverList<Adapter::Observer> observers_;

  // List of devices addresses that are currently connected.
  std::mutex connected_devices_lock_;
  std::unordered_set<std::string> connected_devices_;

  // Factory used to create per-app LowEnergyClient instances.
  std::unique_ptr<LowEnergyClientFactory> ble_client_factory_;

  // Factory used to create per-app GattClient instances.
  std::unique_ptr<GattClientFactory> gatt_client_factory_;

  // Factory used to create per-app GattServer instances.
  std::unique_ptr<GattServerFactory> gatt_server_factory_;

  DISALLOW_COPY_AND_ASSIGN(AdapterImpl);
};

// static
std::unique_ptr<Adapter> Adapter::Create() {
  return std::unique_ptr<Adapter>(new AdapterImpl());
}

}  // namespace bluetooth
