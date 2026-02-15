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

#include <hardware/bluetooth.h>
#include <hardware/bt_gatt_types.h>

#include "service/common/bluetooth/gatt_identifier.h"

// Some utility functions for interacting with GATT HAL interfaces.

namespace bluetooth {
namespace hal {

// Populates a HAL btgatt_srvc_id_t structure from the given GattIdentifier that
// represents a service ID.
void GetHALServiceId(const GattIdentifier& id, btgatt_srvc_id_t* hal_id);

// Populates and returns a GattIdentifier for the given HAL btgatt_srvc_id_t
// structure. Returns nullptr for invalid input.
std::unique_ptr<GattIdentifier> GetServiceIdFromHAL(
    const btgatt_srvc_id_t& srvc_id);

}  // namespace hal
}  // namespace bluetooth
