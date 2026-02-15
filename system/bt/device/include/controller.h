/******************************************************************************
 *
 *  Copyright (C) 2014 Google, Inc.
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

#include <stdbool.h>
#include <stdint.h>

#include "bdaddr.h"
#include "device_features.h"
#include "hci_layer.h"
#include "hci_packet_factory.h"
#include "hci_packet_parser.h"

static const char CONTROLLER_MODULE[] = "controller_module";

typedef struct controller_static_t {
  void (*enable_soc_logging) (bool value);
} controller_static_t;

typedef struct controller_t {
  bool (*get_is_ready)(void);

  const bt_bdaddr_t *(*get_address)(void);
  const bt_version_t *(*get_bt_version)(void);

  const bt_device_features_t *(*get_features_classic)(int index);
  uint8_t (*get_last_features_classic_index)(void);

  const bt_device_features_t *(*get_features_ble)(void);
  const uint8_t *(*get_ble_supported_states)(void);

  bool (*supports_simple_pairing)(void);
  bool (*supports_secure_connections)(void);
  bool (*supports_simultaneous_le_bredr)(void);
  bool (*supports_reading_remote_extended_features)(void);
  bool (*supports_interlaced_inquiry_scan)(void);
  bool (*supports_rssi_with_inquiry_results)(void);
  bool (*supports_extended_inquiry_response)(void);
  bool (*supports_master_slave_role_switch)(void);
  bool (*supports_set_le_privacy_mode)(void);

  bool (*supports_ble)(void);
  bool (*supports_ble_packet_extension)(void);
  bool (*supports_ble_connection_parameters_request)(void);
  bool (*supports_ble_privacy)(void);
  bool (*supports_ble_two_mbps_rate)(void);
  bool (*supports_ble_extended_advertisements)(void);

  // Get the cached acl data sizes for the controller.
  uint16_t (*get_acl_data_size_classic)(void);
  uint16_t (*get_acl_data_size_ble)(void);

  // Get the cached acl packet sizes for the controller.
  // This is a convenience function for the respective
  // acl data size + size of the acl header.
  uint16_t (*get_acl_packet_size_classic)(void);
  uint16_t (*get_acl_packet_size_ble)(void);

  uint16_t (*get_ble_default_data_packet_length)(void);

  // Get the number of acl packets the controller can buffer.
  uint16_t (*get_acl_buffer_count_classic)(void);
  uint8_t (*get_acl_buffer_count_ble)(void);

  uint8_t (*get_ble_white_list_size)(void);

  uint8_t (*get_ble_resolving_list_max_size)(void);
  void (*set_ble_resolving_list_max_size)(int resolving_list_max_size);
  uint8_t *(*get_local_supported_codecs)(uint8_t *number_of_codecs);
  bool (*supports_ble_offload_features)(void);

  uint8_t (*get_ble_adv_ext_size)(void);
  void (*set_ble_adv_ext_size)(int adv_ext_size);
} controller_t;

const controller_static_t *controller_get_static_interface();

const controller_t *controller_get_interface();

const controller_t *controller_get_test_interface(
    const hci_t *hci_interface,
    const hci_packet_factory_t *packet_factory_interface,
    const hci_packet_parser_t *packet_parser_interface);
