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

#include <memory>

#include <base/macros.h>

#include "service/common/bluetooth/adapter_state.h"

namespace bluetooth {

class GattClientFactory;
class GattServerFactory;
class LowEnergyClientFactory;

// Represents the local Bluetooth adapter.
class Adapter {
 public:
  // The default values returned before the Adapter is fully initialized and
  // powered. The complete values for these fields are obtained following a
  // successful call to "Enable".
  static const char kDefaultAddress[];
  static const char kDefaultName[];

  // Observer interface allows other classes to receive notifications from us.
  // All of the methods in this interface are declared as optional to allow
  // different layers to process only those events that they are interested in.
  //
  // All methods take in an |adapter| argument which points to the Adapter
  // object that the Observer instance was added to.
  class Observer {
   public:
    virtual ~Observer() = default;

    // Called when there is a change in the state of the local Bluetooth
    // |adapter| from |prev_state| to |new_state|.
    virtual void OnAdapterStateChanged(Adapter* adapter,
                                       AdapterState prev_state,
                                       AdapterState new_state);

    // Called when there is a change in the connection state between the local
    // |adapter| and a remote device with address |device_address|. If the ACL
    // state changes from disconnected to connected, then |connected| will be
    // true and vice versa.
    virtual void OnDeviceConnectionStateChanged(
        Adapter* adapter, const std::string& device_address, bool connected);
  };

  // Returns an Adapter implementation to be used in production. Don't use these
  // in tests; use MockAdapter instead.
  static std::unique_ptr<Adapter> Create();

  virtual ~Adapter() = default;

  // Add or remove an observer.
  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

  // Returns the current Adapter state.
  virtual AdapterState GetState() const = 0;

  // Returns true, if the adapter radio is current powered.
  virtual bool IsEnabled() const = 0;

  // Enables Bluetooth. This method will send a request to the Bluetooth adapter
  // to power up its radio. Returns true, if the request was successfully sent
  // to the controller, otherwise returns false. A successful call to this
  // method only means that the enable request has been sent to the Bluetooth
  // controller and does not imply that the operation itself succeeded.
  // The |start_restricted| flag enables the adapter in restricted mode. In
  // restricted mode, bonds that are created are marked as restricted in the
  // config file. These devices are deleted upon leaving restricted mode.
  virtual bool Enable(bool start_restricted) = 0;

  // Powers off the Bluetooth radio. Returns true, if the disable request was
  // successfully sent to the Bluetooth controller.
  virtual bool Disable() = 0;

  // Returns the name currently assigned to the local adapter.
  virtual std::string GetName() const = 0;

  // Sets the name assigned to the local Bluetooth adapter. This is the name
  // that the local controller will present to remote devices.
  virtual bool SetName(const std::string& name) = 0;

  // Returns the local adapter addess in string form (XX:XX:XX:XX:XX:XX).
  virtual std::string GetAddress() const = 0;

  // Returns true if the local adapter supports the Low-Energy
  // multi-advertisement feature.
  virtual bool IsMultiAdvertisementSupported() = 0;

  // Returns true if the remote device with address |device_address| is
  // currently connected. This is not a const method as it modifies the state of
  // the associated internal mutex.
  virtual bool IsDeviceConnected(const std::string& device_address) = 0;

  // Returns the total number of trackable advertisements as supported by the
  // underlying hardware.
  virtual int GetTotalNumberOfTrackableAdvertisements() = 0;

  // Returns true if hardware-backed scan filtering is supported.
  virtual bool IsOffloadedFilteringSupported() = 0;

  // Returns true if hardware-backed batch scanning is supported.
  virtual bool IsOffloadedScanBatchingSupported() = 0;

  // Returns a pointer to the LowEnergyClientFactory. This can be used to
  // register per-application LowEnergyClient instances to perform BLE GAP
  // operations.
  virtual LowEnergyClientFactory* GetLowEnergyClientFactory() const = 0;

  // Returns a pointer to the GattClientFactory. This can be used to register
  // per-application GATT server instances.
  virtual GattClientFactory* GetGattClientFactory() const = 0;

  // Returns a pointer to the GattServerFactory. This can be used to register
  // per-application GATT server instances.
  virtual GattServerFactory* GetGattServerFactory() const = 0;

 protected:
  Adapter() = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(Adapter);
};

}  // namespace bluetooth
