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

#include <hardware/bluetooth.h>
#include <stdint.h>
#include <stdlib.h>

#include "btcore/include/device_class.h"

// Copies an array of consecutive properties of |count| to a newly
// allocated array. |properties| must not be NULL.
bt_property_t *property_copy_array(const bt_property_t *properties, size_t count);

// Copies |src| to |dest|. Returns the value of |dest|.
// |src| and |dest| must not be NULL.
bt_property_t *property_copy(bt_property_t *dest, const bt_property_t *src);

// Returns true if the value of the two properties |p1| and |p2| are equal.
// |p1| and |p2| must not be NULL.
bool property_equals(const bt_property_t *p1, const bt_property_t *p2);

// Property resource allocations. Caller is expected to free |property|
// using |property_free| or |property_free_array|.
// Parameter must not be NULL. A copy of the parameter is made and
// stored in the property.
bt_property_t *property_new_addr(const bt_bdaddr_t *addr);
bt_property_t *property_new_device_class(const bt_device_class_t *dc);
bt_property_t *property_new_device_type(bt_device_type_t device_type);
bt_property_t *property_new_discovery_timeout(const uint32_t timeout);
bt_property_t *property_new_name(const char *name);
bt_property_t *property_new_rssi(const int8_t rssi);
bt_property_t *property_new_scan_mode(bt_scan_mode_t scan_mode);
bt_property_t *property_new_uuids(const bt_uuid_t *uuid, size_t count);

// Property resource frees both property and value.
void property_free(bt_property_t *property);
void property_free_array(bt_property_t *properties, size_t count);

// Value check convenience methods. The contents of the property are
// checked for the respective validity and returns true, false otherwise.
// |property| must not be NULL.
bool property_is_addr(const bt_property_t *property);
bool property_is_device_class(const bt_property_t *property);
bool property_is_device_type(const bt_property_t *property);
bool property_is_discovery_timeout(const bt_property_t *property);
bool property_is_name(const bt_property_t *property);
bool property_is_rssi(const bt_property_t *property);
bool property_is_scan_mode(const bt_property_t *property);
bool property_is_uuids(const bt_property_t *property);

// Value conversion convenience methods. The contents of the property are
// properly typed and returned to the caller. |property| must not be NULL.
const bt_bdaddr_t *property_as_addr(const bt_property_t *property);
const bt_device_class_t *property_as_device_class(const bt_property_t *property);
bt_device_type_t property_as_device_type(const bt_property_t *property);
uint32_t property_as_discovery_timeout(const bt_property_t *property);
const bt_bdname_t *property_as_name(const bt_property_t *property);
int8_t property_as_rssi(const bt_property_t *property);
bt_scan_mode_t property_as_scan_mode(const bt_property_t *property);
const bt_uuid_t *property_as_uuids(const bt_property_t *property, size_t *count);
