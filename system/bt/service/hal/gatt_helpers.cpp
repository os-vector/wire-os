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

#include "service/hal/gatt_helpers.h"

namespace bluetooth {
namespace hal {

// Populates a HAL btgatt_srvc_id_t structure from the given GattIdentifier that
// represents a service ID.
void GetHALServiceId(const GattIdentifier& id, btgatt_srvc_id_t* hal_id) {
  CHECK(hal_id);
  CHECK(id.IsService());

  memset(hal_id, 0, sizeof(*hal_id));
  hal_id->is_primary = id.is_primary();
  hal_id->id.inst_id = id.service_instance_id();
  hal_id->id.uuid = id.service_uuid().GetBlueDroid();
}

std::unique_ptr<GattIdentifier> GetServiceIdFromHAL(
    const btgatt_srvc_id_t& srvc_id) {
  UUID uuid(srvc_id.id.uuid);

  return GattIdentifier::CreateServiceId(
      "", srvc_id.id.inst_id, uuid, srvc_id.is_primary);
}

}  // namespace hal
}  // namespace bluetooth
