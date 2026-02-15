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

#include "service/hal/bluetooth_interface.h"

#include <mutex>
#define _LIBCPP_BUILDING_SHARED_MUTEX
#include <shared_mutex>
#undef _LIBCPP_BUILDING_SHARED_MUTEX

#include <base/logging.h>
#include <base/observer_list.h>

#include "service/logging_helpers.h"

extern "C" {
#include "btcore/include/hal_util.h"
}  // extern "C"

using std::lock_guard;
using std::unique_lock;
using std::shared_lock;
using std::mutex;
using std::shared_timed_mutex;

namespace bluetooth {
namespace hal {

namespace {

// The global BluetoothInterface instance.
BluetoothInterface* g_bluetooth_interface = nullptr;

// Mutex used by callbacks to access |g_interface|. If we initialize or clean it
// use unique_lock. If only accessing |g_interface| use shared lock.
//TODO(jpawlowski): this should be just shared_mutex, as we currently don't use
// timed methods. Change to shared_mutex when we upgrade to C++14
shared_timed_mutex g_instance_lock;

// Helper for obtaining the observer list. This is forward declared here and
// defined below since it depends on BluetoothInterfaceImpl.
base::ObserverList<BluetoothInterface::Observer>* GetObservers();

#define FOR_EACH_BLUETOOTH_OBSERVER(func) \
  FOR_EACH_OBSERVER(BluetoothInterface::Observer, *GetObservers(), func)

#define VERIFY_INTERFACE_OR_RETURN() \
  do { \
    if (!g_bluetooth_interface) { \
      LOG(WARNING) << "Callback received while |g_interface| is NULL"; \
      return; \
    } \
  } while (0)

void AdapterStateChangedCallback(bt_state_t state) {
  shared_lock<shared_timed_mutex> lock(g_instance_lock);
  VERIFY_INTERFACE_OR_RETURN();
  VLOG(1) << "Adapter state changed: " << BtStateText(state);
  FOR_EACH_BLUETOOTH_OBSERVER(AdapterStateChangedCallback(state));
}

void AdapterPropertiesCallback(bt_status_t status,
                               int num_properties,
                               bt_property_t* properties) {
  shared_lock<shared_timed_mutex> lock(g_instance_lock);
  VERIFY_INTERFACE_OR_RETURN();
  VLOG(1) << "Adapter properties changed - status: " << BtStatusText(status)
          << ", num_properties: " << num_properties;
  FOR_EACH_BLUETOOTH_OBSERVER(
      AdapterPropertiesCallback(status, num_properties, properties));
}

void RemoteDevicePropertiesCallback(bt_status_t status,
                               bt_bdaddr_t *remote_bd_addr,
                               int num_properties,
                               bt_property_t* properties) {
  shared_lock<shared_timed_mutex> lock(g_instance_lock);
  VERIFY_INTERFACE_OR_RETURN();
  VLOG(1) << " Remote device properties changed - status: " << BtStatusText(status)
          << " - BD_ADDR: " << BtAddrString(remote_bd_addr)
          << ", num_properties: " << num_properties;
  FOR_EACH_BLUETOOTH_OBSERVER(
      RemoteDevicePropertiesCallback(status, remote_bd_addr, num_properties,
                                     properties));
}

void DiscoveryStateChangedCallback(bt_discovery_state_t state) {
  shared_lock<shared_timed_mutex> lock(g_instance_lock);
  VERIFY_INTERFACE_OR_RETURN();
  VLOG(1) << "Discovery state changed - state: " << BtDiscoveryStateText(state);
  FOR_EACH_BLUETOOTH_OBSERVER(DiscoveryStateChangedCallback(state));
}

void PinRequestCallback(bt_bdaddr_t *remote_bd_addr, bt_bdname_t *bd_name,
    uint32_t cod, bool min_16_digit) {
  shared_lock<shared_timed_mutex> lock(g_instance_lock);
  VERIFY_INTERFACE_OR_RETURN();
  VLOG(2) << __func__
          << " - remote_bd_addr: " << remote_bd_addr
          << " - bd_name: " << bd_name
          << " - cod: " << cod
          << " - min_16_digit: " << min_16_digit;
  FOR_EACH_BLUETOOTH_OBSERVER(PinRequestCallback(remote_bd_addr, bd_name, cod, min_16_digit));
}

void SSPRequestCallback(bt_bdaddr_t *remote_bd_addr,
    bt_bdname_t *bd_name, uint32_t cod, bt_ssp_variant_t pairing_variant, uint32_t pass_key) {
  shared_lock<shared_timed_mutex> lock(g_instance_lock);
  VERIFY_INTERFACE_OR_RETURN();
  VLOG(2) << __func__
          << " - remote_bd_addr: " << remote_bd_addr
          << " - bd_name: " << bd_name
          << " - cod: " << cod
          << " - pairing_variant: " << pairing_variant;
  FOR_EACH_BLUETOOTH_OBSERVER(SSPRequestCallback(remote_bd_addr, bd_name, cod,
      pairing_variant, pass_key));
}

void BondStateChangedCallback(
    bt_status_t status,
    bt_bdaddr_t *remote_bd_addr,
    bt_bond_state_t state) {
  shared_lock<shared_timed_mutex> lock(g_instance_lock);
  VERIFY_INTERFACE_OR_RETURN();
  VLOG(2) << __func__
          << " - remote_bd_addr: " << BtAddrString(remote_bd_addr)
          << " - status: " << status
          << " - state: " << state;
  FOR_EACH_BLUETOOTH_OBSERVER(BondStateChangedCallback(status, remote_bd_addr, state));
}

void AclStateChangedCallback(bt_status_t status,
                             bt_bdaddr_t *remote_bd_addr,
                             bt_acl_state_t state) {
  shared_lock<shared_timed_mutex> lock(g_instance_lock);
  VERIFY_INTERFACE_OR_RETURN();
  CHECK(remote_bd_addr);
  VLOG(1) << "Remote device ACL state changed - status: "
          << BtStatusText(status)
          << " - BD_ADDR: " << BtAddrString(remote_bd_addr)
          << " - state: "
          << ((state == BT_ACL_STATE_CONNECTED) ? "CONNECTED" : "DISCONNECTED");
  FOR_EACH_BLUETOOTH_OBSERVER(
      AclStateChangedCallback(status, *remote_bd_addr, state));
}

void ThreadEventCallback(bt_cb_thread_evt evt) {
  VLOG(1) << "ThreadEventCallback" << BtEventText(evt);

  // TODO(armansito): This callback is completely useless to us but btif borks
  // out if this is not set. Consider making this optional.
}

bool SetWakeAlarmCallout(uint64_t /* delay_millis */,
                         bool /* should_wake */,
                         alarm_cb /* cb */,
                         void* /* data */) {
  // TODO(armansito): According to sharvil@, this interface doesn't even need to
  // exist and can be done entirely from within osi by interfacing directly with
  // the kernel. Remove these stubs once that's fixed. (See http://b/23390297)
  return false;
}

int AcquireWakeLockCallout(const char* /* lock_name */) {
  // TODO(armansito): According to sharvil@, this interface doesn't even need to
  // exist and can be done entirely from within osi by interfacing directly with
  // the kernel. Remove these stubs once that's fixed. (See http://b/23390297)
  // Lie here and return success so that enabling and disabling the controller
  // works before this is properly implemented.
  return BT_STATUS_SUCCESS;
}

int ReleaseWakeLockCallout(const char* /* lock_name */) {
  // TODO(armansito): According to sharvil@, this interface doesn't even need to
  // exist and can be done entirely from within osi by interfacing directly with
  // the kernel. Remove these stubs once that's fixed. (See http://b/23390297)
  // Lie here and return success so that enabling and disabling the controller
  // works before this is properly implemented.
  return BT_STATUS_SUCCESS;
}

// The HAL Bluetooth DM callbacks.
bt_callbacks_t bt_callbacks = {
  sizeof(bt_callbacks_t),
  AdapterStateChangedCallback,
  AdapterPropertiesCallback,
  RemoteDevicePropertiesCallback,
  nullptr, /* device_found_cb */
  DiscoveryStateChangedCallback,
  PinRequestCallback,
  SSPRequestCallback,
  BondStateChangedCallback,
  AclStateChangedCallback,
  ThreadEventCallback,
  nullptr, /* dut_mode_recv_cb */
  nullptr, /* le_test_mode_cb */
  nullptr, /* energy_info_cb */
  nullptr  /*hci_event_recv_cb */
};

bt_os_callouts_t bt_os_callouts = {
  sizeof(bt_os_callouts_t),
  SetWakeAlarmCallout,
  AcquireWakeLockCallout,
  ReleaseWakeLockCallout
};

}  // namespace

// BluetoothInterface implementation for production.
class BluetoothInterfaceImpl : public BluetoothInterface {
 public:
  BluetoothInterfaceImpl()
      : hal_iface_(nullptr),
        hal_adapter_(nullptr) {
  }

  ~BluetoothInterfaceImpl() override {
    if (hal_iface_)
        hal_iface_->cleanup();
  }

  // BluetoothInterface overrides.
  void AddObserver(Observer* observer) override {
    shared_lock<shared_timed_mutex> lock(g_instance_lock);
    observers_.AddObserver(observer);
  }

  void RemoveObserver(Observer* observer) override {
    shared_lock<shared_timed_mutex> lock(g_instance_lock);
    observers_.RemoveObserver(observer);
  }

  const bt_interface_t* GetHALInterface() const override {
    return hal_iface_;
  }

  const bluetooth_device_t* GetHALAdapter() const override {
    return hal_adapter_;
  }

  // Initialize the interface. This loads the shared Bluetooth library and sets
  // up the callbacks.
  bool Initialize() {
    // Load the Bluetooth shared library module.
    const hw_module_t* module;
    int status = hal_util_load_bt_library(&module);
    if (status) {
      LOG(ERROR) << "Failed to load Bluetooth library: " << status;
      return false;
    }

    // Open the Bluetooth adapter.
    hw_device_t* device;
    status = module->methods->open(module, BT_HARDWARE_MODULE_ID, &device);
    if (status) {
      LOG(ERROR) << "Failed to open the Bluetooth module";
      return false;
    }

    hal_adapter_ = reinterpret_cast<bluetooth_device_t*>(device);
    hal_iface_ = hal_adapter_->get_bluetooth_interface();

    // Initialize the Bluetooth interface. Set up the adapter (Bluetooth DM) API
    // callbacks.
    status = hal_iface_->init(&bt_callbacks);
    if (status != BT_STATUS_SUCCESS) {
      LOG(ERROR) << "Failed to initialize Bluetooth stack";
      return false;
    }

    status = hal_iface_->set_os_callouts(&bt_os_callouts);
    if (status != BT_STATUS_SUCCESS) {
      LOG(ERROR) << "Failed to set up Bluetooth OS callouts";
      return false;
    }

    return true;
  }

  base::ObserverList<Observer>* observers() { return &observers_; }

 private:
  // List of observers that are interested in notifications from us. We're not
  // using a base::ObserverListThreadSafe, which it posts observer events
  // automatically on the origin threads, as we want to avoid that overhead and
  // simply forward the events to the upper layer.
  base::ObserverList<Observer> observers_;

  // The HAL handle obtained from the shared library. We hold a weak reference
  // to this since the actual data resides in the shared Bluetooth library.
  const bt_interface_t* hal_iface_;

  // The HAL handle that represents the underlying Bluetooth adapter. We hold a
  // weak reference to this since the actual data resides in the shared
  // Bluetooth library.
  const bluetooth_device_t* hal_adapter_;

  DISALLOW_COPY_AND_ASSIGN(BluetoothInterfaceImpl);
};

namespace {

// Helper for obtaining the observer list from the global instance. This
// function is NOT thread safe.
base::ObserverList<BluetoothInterface::Observer>* GetObservers() {
  CHECK(g_bluetooth_interface);
  return static_cast<BluetoothInterfaceImpl*>(
      g_bluetooth_interface)->observers();
}

}  // namespace

// Default observer implementations. These are provided so that the methods
// themselves are optional.
void BluetoothInterface::Observer::AdapterStateChangedCallback(
    bt_state_t /* state*/) {
  // Do nothing.
}

void BluetoothInterface::Observer::AdapterPropertiesCallback(
    bt_status_t /* status */,
    int /* num_properties */,
    bt_property_t* /* properties */) {
  // Do nothing.
}

void BluetoothInterface::Observer::RemoteDevicePropertiesCallback(
    bt_status_t /* status */,
    bt_bdaddr_t* /* remote_bd_addr */,
    int /* num_properties */,
    bt_property_t* /* properties */) {
  // Do nothing.
}

void BluetoothInterface::Observer::DiscoveryStateChangedCallback(
    bt_discovery_state_t /* state */) {
  // Do nothing.
}

void BluetoothInterface::Observer::PinRequestCallback(
    bt_bdaddr_t *remote_bd_addr,
    bt_bdname_t *bd_name,
    uint32_t cod,
    bool min_16_digit) {
  // Do nothing.
}

void BluetoothInterface::Observer::SSPRequestCallback(
    bt_bdaddr_t *remote_bd_addr,
    bt_bdname_t *bd_name,
    uint32_t cod,
    bt_ssp_variant_t pairing_variant,
    uint32_t pass_key) {
  // Do nothing.
}

void BluetoothInterface::Observer::BondStateChangedCallback(
    bt_status_t status,
    bt_bdaddr_t *remote_bd_addr,
    bt_bond_state_t state) {
  // Do nothing.
}

void BluetoothInterface::Observer::AclStateChangedCallback(
    bt_status_t /* status */,
    const bt_bdaddr_t& /* remote_bdaddr */,
    bt_acl_state_t /* state */) {
  // Do nothing.
}

// static
bool BluetoothInterface::Initialize() {
  unique_lock<shared_timed_mutex> lock(g_instance_lock);
  CHECK(!g_bluetooth_interface);

  std::unique_ptr<BluetoothInterfaceImpl> impl(new BluetoothInterfaceImpl());
  if (!impl->Initialize()) {
    LOG(ERROR) << "Failed to initialize BluetoothInterface";
    return false;
  }

  g_bluetooth_interface = impl.release();

  return true;
}

// static
void BluetoothInterface::CleanUp() {
  unique_lock<shared_timed_mutex> lock(g_instance_lock);
  CHECK(g_bluetooth_interface);

  delete g_bluetooth_interface;
  g_bluetooth_interface = nullptr;
}

// static
bool BluetoothInterface::IsInitialized() {
  shared_lock<shared_timed_mutex> lock(g_instance_lock);

  return g_bluetooth_interface != nullptr;
}

// static
BluetoothInterface* BluetoothInterface::Get() {
  shared_lock<shared_timed_mutex> lock(g_instance_lock);
  CHECK(g_bluetooth_interface);
  return g_bluetooth_interface;
}

// static
void BluetoothInterface::InitializeForTesting(
    BluetoothInterface* test_instance) {
  unique_lock<shared_timed_mutex> lock(g_instance_lock);
  CHECK(test_instance);
  CHECK(!g_bluetooth_interface);

  g_bluetooth_interface = test_instance;
}

}  // namespace hal
}  // namespace bluetooth
