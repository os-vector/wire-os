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

#include "base.h"
#include "btcore/include/bdaddr.h"
#include "btcore/include/property.h"
#include "support/adapter.h"
#include "support/callbacks.h"

static bt_state_t state;
static int property_count = 0;
static bt_property_t *properties = NULL;
static bt_discovery_state_t discovery_state;
static bt_acl_state_t acl_state;
static bt_bond_state_t bond_state;

static void parse_properties(int num_properties, bt_property_t *property);

// Returns the current adapter state.
bt_state_t adapter_get_state() {
  return state;
}

// Returns the number of adapter properties.
int adapter_get_property_count() {
  return property_count;
}

// Returns the specified property.
bt_property_t *adapter_get_property(bt_property_type_t type) {
  for (int i = 0; i < property_count; ++i) {
    if (properties[i].type == type) {
      return &properties[i];
    }
  }

  return NULL;
}

// Returns the device discovery state.
bt_discovery_state_t adapter_get_discovery_state() {
  return discovery_state;
}

// Returns the device acl state.
bt_acl_state_t adapter_get_acl_state() {
  return acl_state;
}

// Returns the device bond state.
bt_bond_state_t adapter_get_bond_state() {
  return bond_state;
}

// callback
void acl_state_changed(bt_status_t status, bt_bdaddr_t *remote_bd_addr, bt_acl_state_t state) {
  acl_state = state;
  CALLBACK_RET();
}

// callback
void adapter_properties(bt_status_t status,
    int num_properties,
    bt_property_t *new_properties) {
  property_free_array(properties, property_count);
  properties = property_copy_array(new_properties, num_properties);
  property_count = num_properties;

  CALLBACK_RET();
}

// callback
void adapter_state_changed(bt_state_t new_state) {
  state = new_state;
  CALLBACK_RET();
}

// callback
void bond_state_changed(bt_status_t status,
    bt_bdaddr_t *bdaddr,
    bt_bond_state_t state) {
  char buf[18];
  bond_state = state;

  const char *state_name = "Bond state unknown";
  switch (bond_state) {
    case BT_BOND_STATE_NONE:
      state_name = "Bond state none";
      break;

    case BT_BOND_STATE_BONDING:
      state_name = "Bond state bonding";
      break;

    case BT_BOND_STATE_BONDED:
      state_name = "Bond state bonded";
      break;

      // default none
  }
  fprintf(stdout, "Bond state changed callback addr:%s state:%s\n", bdaddr_to_string(bdaddr, buf, sizeof(buf)), state_name);

  CALLBACK_RET();
}

// callback
void device_found(int num_properties, bt_property_t *property) {
  fprintf(stdout, "Device found num_properties:%d\n", num_properties);
  parse_properties(num_properties, property);

  CALLBACK_RET();
}

// callback
void discovery_state_changed(bt_discovery_state_t state) {
  const char *state_name = "Unknown";
  discovery_state = state;

  switch (discovery_state) {
    case BT_DISCOVERY_STOPPED:
      state_name = "Discovery stopped";
      break;

    case BT_DISCOVERY_STARTED:
      state_name = "Discovery started";
      break;

      // default omitted
  }
  fprintf(stdout, "Discover state %s\n", state_name);

  CALLBACK_RET();
}

// callback
void remote_device_properties(bt_status_t status,
    bt_bdaddr_t *bdaddr,
    int num_properties,
    bt_property_t *properties) {
  char buf[18];
  fprintf(stdout, "Device found bdaddr:%s num_properties:%d\n",
      bdaddr_to_string(bdaddr, buf, sizeof(buf)), num_properties);

  parse_properties(num_properties, properties);

  CALLBACK_RET();
}

// callback
void ssp_request(
    bt_bdaddr_t *remote_bd_addr,
    bt_bdname_t *bd_name,
    uint32_t cod,
    bt_ssp_variant_t pairing_variant,
    uint32_t pass_key) {
  char *pairing_variant_name = "Unknown";

  switch (pairing_variant) {
    case BT_SSP_VARIANT_PASSKEY_CONFIRMATION:
      pairing_variant_name = "Passkey confirmation";
      break;
    case BT_SSP_VARIANT_PASSKEY_ENTRY:
      pairing_variant_name = "Passkey entry";
      break;

    case BT_SSP_VARIANT_CONSENT:
      pairing_variant_name = "Passkey consent";
      break;

    case BT_SSP_VARIANT_PASSKEY_NOTIFICATION:
      pairing_variant_name = "Passkey notification";
      break;
  }

  fprintf(stdout, "Got ssp request device_class:%u passkey:%x pairing_variant:%s\n", cod, pass_key, pairing_variant_name);
  char buf[18];
  fprintf(stdout, "Device found:%s %s\n", bdaddr_to_string(remote_bd_addr, buf, sizeof(buf)), bd_name->name);


  fprintf(stdout, "auto-accepting bond\n");
  bool accept = true;
  int rc = bt_interface->ssp_reply(remote_bd_addr, pairing_variant,
      (uint8_t)accept, pass_key);
  CALLBACK_RET();
}

// callback
void thread_evt(bt_cb_thread_evt evt) {
  CALLBACK_RET();
}

static void parse_properties(int num_properties, bt_property_t *property) {
  while (num_properties-- > 0) {
    switch(property->type) {
      case BT_PROPERTY_BDNAME:
        {
          const bt_bdname_t *name = property_as_name(property);
          if (name)
            fprintf(stdout, " name:%s\n", name->name);
        }
        break;

      case BT_PROPERTY_BDADDR:
        {
          char buf[18];
          const bt_bdaddr_t *addr = property_as_addr(property);
          if (addr)
            fprintf(stdout, " addr:%s\n", bdaddr_to_string(addr, buf, sizeof(buf)));
        }
        break;

      case BT_PROPERTY_UUIDS:
        {
          size_t num_uuid;
          const bt_uuid_t *uuid = property_as_uuids(property, &num_uuid);
          if (uuid) {
            for (size_t i = 0; i < num_uuid; i++) {
              fprintf(stdout, " uuid:%zd: ", i);
              for (size_t j = 0; j < sizeof(uuid); j++) {
                fprintf(stdout, "%02x", uuid->uu[j]);
              }
              fprintf(stdout, "\n");
            }
          }
        }
        break;

      case BT_PROPERTY_TYPE_OF_DEVICE:
        {
          bt_device_type_t device_type = property_as_device_type(property);
          if (device_type) {
            const struct {
              const char * device_type;
            } device_type_lookup[] = {
              { "Unknown" },
              { "Classic Only" },
              { "BLE Only" },
              { "Both Classic and BLE" },
            };
            int idx = (int)device_type;
            if (idx > BT_DEVICE_DEVTYPE_DUAL)
              idx = 0;
            fprintf(stdout, " device_type:%s\n", device_type_lookup[idx].device_type);
          }
        }
        break;

      case BT_PROPERTY_CLASS_OF_DEVICE:
        {
          const bt_device_class_t *dc = property_as_device_class(property);
          int dc_int = device_class_to_int(dc);
          fprintf(stdout, " device_class:0x%x\n", dc_int);
        }
        break;

      case BT_PROPERTY_REMOTE_RSSI:
        {
          int8_t rssi = property_as_rssi(property);
          fprintf(stdout, " rssi:%d\n", rssi);
        }
        break;

      case BT_PROPERTY_REMOTE_FRIENDLY_NAME:
        {
          const bt_bdname_t *name = property_as_name(property);
          if (name)
            fprintf(stdout, " remote_name:%s\n", name->name);
        }
        break;

      case BT_PROPERTY_SERVICE_RECORD:
      case BT_PROPERTY_ADAPTER_SCAN_MODE:
      case BT_PROPERTY_ADAPTER_BONDED_DEVICES:
      case BT_PROPERTY_ADAPTER_DISCOVERY_TIMEOUT:
      case BT_PROPERTY_REMOTE_VERSION_INFO:
      case BT_PROPERTY_LOCAL_LE_FEATURES:
      case BT_PROPERTY_REMOTE_DEVICE_TIMESTAMP:
      default:
        {
          fprintf(stderr, "Unhandled property type:%d len:%d\n", property->type, property->len);
          uint8_t *p = (uint8_t *)property->val;
          for (int i = 0; i < property->len; ++i, p++) {
            fprintf(stderr, " %02x", *p);
          }
          if (property->len != 0)
            fprintf(stderr, "\n");
        }
    }
    property++;
  }
}
