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
#include <hardware/bluetooth.h>

namespace bluetooth {
namespace hal {

// This class represents the HAL Bluetooth adapter interface, wrapping around
// the underlying bt_interface_t structure, its methods, and callbacks. A single
// instance of this class exists per application and it allows multiple classes
// to interface with the global HAL interface by multiplexing callbacks among
// registered clients.
//
// This is declared as an abstract interface so that a fake implementation can
// be injected for testing the upper layer.
//
// TODO: (expose callback types directly but via redirection) methods for
// initialize, clean up, and set for testing.
class BluetoothInterface {
 public:
  // The standard Bluetooth adapter management callback interface. The HAL
  // interface doesn't allow registering "user data" that carries context beyond
  // the callback parameters, forcing implementations to deal with global
  // variables. The Observer interface is to redirect these events to interested
  // parties in an object-oriented manner.
  //
  // TODO(armansito): We should fix this in the HAL.
  class Observer {
   public:
    virtual ~Observer() = default;

    // All of the events below correspond to callbacks defined in
    // "bt_callbacks_t" in the HAL API definitions.

    virtual void AdapterStateChangedCallback(bt_state_t state);
    virtual void AdapterPropertiesCallback(bt_status_t status,
                                           int num_properties,
                                           bt_property_t* properties);
    virtual void RemoteDevicePropertiesCallback(bt_status_t status,
                                                bt_bdaddr_t *remote_bd_addr,
                                                int num_properties,
                                                bt_property_t* properties);
    virtual void DiscoveryStateChangedCallback(bt_discovery_state_t state);
    virtual void PinRequestCallback(bt_bdaddr_t *remote_bd_addr,
                                    bt_bdname_t *bd_name,
                                    uint32_t cod,
                                    bool min_16_digit);
    virtual void SSPRequestCallback(bt_bdaddr_t *remote_bd_addr,
                                    bt_bdname_t *bd_name,
                                    uint32_t cod,
                                    bt_ssp_variant_t pairing_variant,
                                    uint32_t pass_key);
    virtual void BondStateChangedCallback(bt_status_t status,
                                          bt_bdaddr_t *remote_bd_addr,
                                          bt_bond_state_t state);
    virtual void AclStateChangedCallback(bt_status_t status,
                                         const bt_bdaddr_t& remote_bdaddr,
                                         bt_acl_state_t state);

    // TODO(armansito): Complete the list of callbacks.
  };

  // Initialize and clean up the BluetoothInterface singleton. Returns false if
  // the underlying HAL interface failed to initialize, and true on success.
  static bool Initialize();

  // Shuts down and cleans up the interface. CleanUp must be called on the same
  // thread that called Initialize.
  static void CleanUp();

  // Returns true if the interface was initialized and a global singleton has
  // been created.
  static bool IsInitialized();

  // Initialize for testing. Use this to inject a test version of
  // BlueoothInterface. To be used from unit tests only.
  static void InitializeForTesting(BluetoothInterface* test_instance);

  // Returns the BluetoothInterface singleton. If the interface has not been
  // initialized, returns nullptr.
  static BluetoothInterface* Get();

  // Add or remove an observer that is interested in notifications from us.
  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

  // The HAL module pointer that represents the standard Bluetooth adapter
  // management interface. This is implemented in and provided by the shared
  // Bluetooth library, so this isn't owned by us.
  //
  // Upper layers can make bt_interface_t API calls through this structure.
  // However, DO NOT call the "init" function as this is called and managed by
  // us. The behavior is undefined if "init" is called directly by upper layers.
  virtual const bt_interface_t* GetHALInterface() const = 0;

  // The HAL module pointer that represents the underlying Bluetooth adapter.
  // This is implemented in and provided by the shared Bluetooth library, so
  // this isn't owned by us.
  virtual const bluetooth_device_t* GetHALAdapter() const = 0;

 protected:
  BluetoothInterface() = default;
  virtual ~BluetoothInterface() = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(BluetoothInterface);
};

}  // namespace hal
}  // namespace bluetooth
