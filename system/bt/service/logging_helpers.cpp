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
#include "logging_helpers.h"

#include <string.h>

#include <string>

#define CASE_RETURN_TEXT(code) \
  case code:                   \
    return #code

const char *BtAvConnectionStateText(const btav_connection_state_t state) {
  switch (state) {
    CASE_RETURN_TEXT(BTAV_CONNECTION_STATE_DISCONNECTED);
    CASE_RETURN_TEXT(BTAV_CONNECTION_STATE_CONNECTING);
    CASE_RETURN_TEXT(BTAV_CONNECTION_STATE_CONNECTED);
    CASE_RETURN_TEXT(BTAV_CONNECTION_STATE_DISCONNECTING);
    default:
      return "Invalid AV connection state";
  }
}

const char *BtAvAudioStateText(const btav_audio_state_t state) {
  switch (state) {
    CASE_RETURN_TEXT(BTAV_AUDIO_STATE_REMOTE_SUSPEND);
    CASE_RETURN_TEXT(BTAV_AUDIO_STATE_STOPPED);
    CASE_RETURN_TEXT(BTAV_AUDIO_STATE_STARTED);
    default:
      return "Invalid audio state";
  }
}

const char *BtTransportText(const btgatt_transport_t t) {
  switch(t) {
    CASE_RETURN_TEXT(GATT_TRANSPORT_AUTO);
    CASE_RETURN_TEXT(GATT_TRANSPORT_BREDR);
    CASE_RETURN_TEXT(GATT_TRANSPORT_LE);
    default:
      return "unknown transport";
  }
}

const char *BtStateText(const bt_state_t state) {
  switch (state) {
    CASE_RETURN_TEXT(BT_STATE_OFF);
    CASE_RETURN_TEXT(BT_STATE_ON);
    default:
      return "unknown state code";
  }
}

const char *BtDiscoveryStateText(const bt_discovery_state_t state) {
  switch (state) {
    CASE_RETURN_TEXT(BT_DISCOVERY_STOPPED);
    CASE_RETURN_TEXT(BT_DISCOVERY_STARTED);
    default:
      return "unknown discovery state code";
  }
}

const char *BtScanModeText(const bt_scan_mode_t mode) {
  switch (mode) {
    CASE_RETURN_TEXT(BT_SCAN_MODE_NONE);
    CASE_RETURN_TEXT(BT_SCAN_MODE_CONNECTABLE);
    CASE_RETURN_TEXT(BT_SCAN_MODE_CONNECTABLE_DISCOVERABLE);
    default:
      return "unknown scan mode";
  }
}

const char *BtStatusText(const bt_status_t status) {
  switch (status) {
    CASE_RETURN_TEXT(BT_STATUS_SUCCESS);
    CASE_RETURN_TEXT(BT_STATUS_FAIL);
    CASE_RETURN_TEXT(BT_STATUS_NOT_READY);
    CASE_RETURN_TEXT(BT_STATUS_NOMEM);
    CASE_RETURN_TEXT(BT_STATUS_DONE);
    CASE_RETURN_TEXT(BT_STATUS_BUSY);
    CASE_RETURN_TEXT(BT_STATUS_UNSUPPORTED);
    CASE_RETURN_TEXT(BT_STATUS_PARM_INVALID);
    CASE_RETURN_TEXT(BT_STATUS_UNHANDLED);
    CASE_RETURN_TEXT(BT_STATUS_AUTH_FAILURE);
    CASE_RETURN_TEXT(BT_STATUS_RMT_DEV_DOWN);
    CASE_RETURN_TEXT(BT_STATUS_AUTH_REJECTED);
    default:
      return "unknown status code";
  }
}

const char *BtPropertyText(const bt_property_type_t prop) {
  switch (prop) {
    CASE_RETURN_TEXT(BT_PROPERTY_BDNAME);
    CASE_RETURN_TEXT(BT_PROPERTY_BDADDR);
    CASE_RETURN_TEXT(BT_PROPERTY_UUIDS);
    CASE_RETURN_TEXT(BT_PROPERTY_CLASS_OF_DEVICE);
    CASE_RETURN_TEXT(BT_PROPERTY_TYPE_OF_DEVICE);
    CASE_RETURN_TEXT(BT_PROPERTY_SERVICE_RECORD);
    CASE_RETURN_TEXT(BT_PROPERTY_ADAPTER_SCAN_MODE);
    CASE_RETURN_TEXT(BT_PROPERTY_ADAPTER_BONDED_DEVICES);
    CASE_RETURN_TEXT(BT_PROPERTY_ADAPTER_DISCOVERY_TIMEOUT);
    CASE_RETURN_TEXT(BT_PROPERTY_REMOTE_FRIENDLY_NAME);
    CASE_RETURN_TEXT(BT_PROPERTY_REMOTE_RSSI);
    CASE_RETURN_TEXT(BT_PROPERTY_REMOTE_VERSION_INFO);
    CASE_RETURN_TEXT(BT_PROPERTY_LOCAL_LE_FEATURES);
    CASE_RETURN_TEXT(BT_PROPERTY_REMOTE_DEVICE_TIMESTAMP);
    default:
      return "Invalid property";
  }
}

const char *BtEventText(const bt_cb_thread_evt evt) {
  switch (evt) {
    CASE_RETURN_TEXT(ASSOCIATE_JVM);
    CASE_RETURN_TEXT(DISASSOCIATE_JVM);
    default:
      return "unknown state code";
  }
}

const char *BtAclText(const bt_acl_state_t code) {
  switch (code) {
    CASE_RETURN_TEXT(BT_ACL_STATE_CONNECTED);
    CASE_RETURN_TEXT(BT_ACL_STATE_DISCONNECTED);
    default:
      return "unknown ACL code";
  }
}

std::string BtAddrString(const bt_bdaddr_t *addr) {
  char buffer[20];
  snprintf(buffer, sizeof(buffer), "%02X:%02X:%02X:%02X:%02X:%02X",
           addr->address[0], addr->address[1], addr->address[2],
           addr->address[3], addr->address[4], addr->address[5]);
  return std::string(buffer);
}
