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

#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include "gatt/gatt_test.h"

#define DEFAULT_RANDOM_SEED 42

namespace {

static void create_random_uuid(bt_uuid_t *uuid, int seed) {
  srand(seed < 0 ? time(NULL) : seed);
  for (int i = 0; i < 16; ++i) {
    uuid->uu[i] = (uint8_t) (rand() % 256);
  }
}

}  // namespace

namespace bttest {

TEST_F(GattTest, GattClientRegister) {
  // Registers gatt client.
  bt_uuid_t gatt_client_uuid;
  create_random_uuid(&gatt_client_uuid, DEFAULT_RANDOM_SEED);
  gatt_client_interface()->register_client(&gatt_client_uuid);
  semaphore_wait(register_client_callback_sem_);
  EXPECT_TRUE(status() == BT_STATUS_SUCCESS)
    << "Error registering GATT client app callback.";

  // Unregisters gatt client. No callback is expected.
  gatt_client_interface()->unregister_client(client_interface_id());
}

TEST_F(GattTest, GattClientScanRemoteDevice) {
  // Starts BLE scan. NB: This test assumes there is a BLE beacon advertising nearby.
  gatt_client_interface()->scan(true);
  semaphore_wait(scan_result_callback_sem_);

  // Ends BLE scan. No callback is expected.
  gatt_client_interface()->scan(false);
}

TEST_F(GattTest, GattClientAdvertise) {
  // Registers a new client app.
  bt_uuid_t gatt_client_uuid;
  create_random_uuid(&gatt_client_uuid, DEFAULT_RANDOM_SEED);
  gatt_client_interface()->register_client(&gatt_client_uuid);
  semaphore_wait(register_client_callback_sem_);
  EXPECT_TRUE(status() == BT_STATUS_SUCCESS)
    << "Error registering GATT client app callback.";

  // Starts advertising.
  gatt_client_interface()->listen(client_interface_id(), true);
  semaphore_wait(listen_callback_sem_);
  EXPECT_TRUE(status() == BT_STATUS_SUCCESS)
    << "Error starting BLE advertisement.";

  // Stops advertising.
  gatt_client_interface()->listen(client_interface_id(), false);
  semaphore_wait(listen_callback_sem_);
  EXPECT_TRUE(status() == BT_STATUS_SUCCESS)
    << "Error stopping BLE advertisement.";

  // Unregisters gatt server. No callback is expected.
  gatt_client_interface()->unregister_client(client_interface_id());
}

TEST_F(GattTest, GattServerRegister) {
  // Registers gatt server.
  bt_uuid_t gatt_server_uuid;
  create_random_uuid(&gatt_server_uuid, DEFAULT_RANDOM_SEED);
  gatt_server_interface()->register_server(&gatt_server_uuid);
  semaphore_wait(register_server_callback_sem_);
  EXPECT_TRUE(status() == BT_STATUS_SUCCESS)
    << "Error registering GATT server app callback.";

  // Unregisters gatt server. No callback is expected.
  gatt_server_interface()->unregister_server(server_interface_id());
}

TEST_F(GattTest, GattServerBuild) {
  // Registers gatt server.
  bt_uuid_t gatt_server_uuid;
  create_random_uuid(&gatt_server_uuid, DEFAULT_RANDOM_SEED);
  gatt_server_interface()->register_server(&gatt_server_uuid);
  semaphore_wait(register_server_callback_sem_);
  EXPECT_TRUE(status() == BT_STATUS_SUCCESS)
    << "Error registering GATT server app callback.";

  // Service UUID.
  btgatt_srvc_id_t srvc_id;
  srvc_id.id.inst_id = 0;   // there is only one instance of this service.
  srvc_id.is_primary = 1;   // this service is primary.
  create_random_uuid(&srvc_id.id.uuid, -1);

  // Characteristics UUID.
  bt_uuid_t char_uuid;
  create_random_uuid(&char_uuid, -1);

  // Descriptor UUID.
  bt_uuid_t desc_uuid;
  create_random_uuid(&desc_uuid, -1);

  // Adds service.
  int server_if = server_interface_id();
  gatt_server_interface()->add_service(server_if, &srvc_id, 4 /* # handles */);
  semaphore_wait(service_added_callback_sem_);
  EXPECT_TRUE(status() == BT_STATUS_SUCCESS) << "Error adding service.";

  // Adds characteristics.
  int srvc_handle = service_handle();
  gatt_server_interface()->add_characteristic(server_if, srvc_handle,
      &char_uuid, 0x10 /* notification */, 0x01 /* read only */);
  semaphore_wait(characteristic_added_callback_sem_);
  EXPECT_TRUE(status() == BT_STATUS_SUCCESS)
      << "Error adding characteristics.";

  // Adds descriptor.
  gatt_server_interface()->add_descriptor(server_if, srvc_handle,
                                          &desc_uuid, 0x01);
  semaphore_wait(descriptor_added_callback_sem_);
  EXPECT_TRUE(status() == BT_STATUS_SUCCESS)
      << "Error adding descriptor.";

  // Starts server.
  gatt_server_interface()->start_service(server_if, srvc_handle, 2 /*BREDR/LE*/);
  semaphore_wait(service_started_callback_sem_);
  EXPECT_TRUE(status() == BT_STATUS_SUCCESS) << "Error starting server.";

  // Stops server.
  gatt_server_interface()->stop_service(server_if, srvc_handle);
  semaphore_wait(service_stopped_callback_sem_);
  EXPECT_TRUE(status() == BT_STATUS_SUCCESS) << "Error stopping server.";

  // Deletes service.
  gatt_server_interface()->delete_service(server_if, srvc_handle);
  semaphore_wait(service_deleted_callback_sem_);
  EXPECT_TRUE(status() == BT_STATUS_SUCCESS) << "Error deleting service.";

  // Unregisters gatt server. No callback is expected.
  gatt_server_interface()->unregister_server(server_if);
}

}  // bttest
