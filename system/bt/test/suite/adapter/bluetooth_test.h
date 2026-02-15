/******************************************************************************
 *
 *  Copyright (C) 2015 Google, Inc.
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at:
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 ******************************************************************************/

#pragma once

#include "service/hal/bluetooth_interface.h"

#include <gtest/gtest.h>
#include <map>
#include <string>

extern "C" {
#include <hardware/bluetooth.h>
#include <hardware/bt_gatt.h>
#include <hardware/bt_pan.h>
#include <hardware/bt_sock.h>
#include <hardware/hardware.h>
#include <signal.h>
#include <time.h>

#include "osi/include/semaphore.h"
}

namespace bttest {

// This class represents the Bluetooth testing framework and provides
// helpers and callbacks for GUnit to use for testing.
class BluetoothTest : public ::testing::Test,
                      public bluetooth::hal::BluetoothInterface::Observer {
 protected:
  BluetoothTest() = default;
  virtual ~BluetoothTest() = default;

  // Getter for the bt_interface
  const bt_interface_t* bt_interface();

  // Gets the current state of the Bluetooth Interface
  bt_state_t GetState();

  // Get the number of properties that have changed on the
  // Adapter Properties callback
  int GetPropertiesChangedCount();

  // Get the value of a specific property
  bt_property_t* GetProperty(bt_property_type_t type);

  // Get the current discovery state
  bt_discovery_state_t GetDiscoveryState();

  // Get the current Acl State
  bt_acl_state_t GetAclState();

  // Get the current Bond State
  bt_bond_state_t GetBondState();

  // Reset a semaphores count to 0
  void ClearSemaphore(semaphore_t* sem);

  // SetUp initializes the Bluetooth interface and registers the callbacks
  // before running every test
  void SetUp() override;

  // TearDown cleans up the stack and interface at the end of every test
  void TearDown() override;

  // A callback that is called when a property changes
  void AdapterPropertiesCallback(
      bt_status_t status,
      int num_properties,
      bt_property_t *properties);

  // A callback that is called when the adapter state changes
  void AdapterStateChangedCallback(bt_state_t state);

  // A callback that is called when the Discovery state changes
  void DiscoveryStateChangedCallback(bt_discovery_state_t state);

  // Semaphores used to wait for specific callback execution. Each callback
  // has its own semaphore associated with it.
  semaphore_t* adapter_properties_callback_sem_;
  semaphore_t* adapter_state_changed_callback_sem_;
  semaphore_t* discovery_state_changed_callback_sem_;

 private:
  // The bluetooth interface that all the tests use to interact with the HAL
  const bt_interface_t* bt_interface_;

  bt_state_t state_;
  int properties_changed_count_;
  bt_property_t *last_changed_properties_;
  bt_discovery_state_t discovery_state_;
  bt_acl_state_t acl_state_;
  bt_bond_state_t bond_state_;

  DISALLOW_COPY_AND_ASSIGN(BluetoothTest);
};

}  // bttest
