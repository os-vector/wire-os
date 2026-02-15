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

#include <string.h>

#include <string>

#include "hardware/bluetooth.h"
#include "hardware/bt_av.h"
#include "hardware/bt_gatt_types.h"

const char *BtAvConnectionStateText(const btav_connection_state_t state);

const char *BtAvAudioStateText(const btav_audio_state_t state);

const char *BtTransportText(const btgatt_transport_t t);

const char *BtStateText(const bt_state_t state);

const char *BtDiscoveryStateText(const bt_discovery_state_t);

const char *BtScanModeText(const bt_scan_mode_t mode);

const char *BtStatusText(const bt_status_t status);

const char *BtPropertyText(const bt_property_type_t prop);

const char *BtEventText(const bt_cb_thread_evt evt);

const char *BtAclText(const bt_acl_state_t state);

// TODO(icoolidge): Address object.
std::string BtAddrString(const bt_bdaddr_t *addr);
