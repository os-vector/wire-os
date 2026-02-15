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

#include <bluetooth/uuid.h>

namespace bluetooth {

// Used for filtering scan results by allowing clients to restrict scan results
// to only those that are of interest to them.
class ScanFilter {
 public:
  ScanFilter() = default;
  ~ScanFilter() = default;

  // Copy constructor and assignment operator.
  ScanFilter(const ScanFilter& other);
  ScanFilter& operator=(const ScanFilter& other);

  // The device name used while filtering scan results.
  const std::string& device_name() const { return device_name_; }
  void set_device_name(const std::string& name) { device_name_ = name; }

  // The device address used while filtering scan results. Address should be in
  // the XX:XX:XX:XX:XX:XX where X is a hexadecimal digit.
  const std::string& device_address() const { return device_address_; }

  // Sets the device address used for filtering. Returns false if
  // |device_address| is in an illegal format.
  bool SetDeviceAddress(const std::string& device_address);

  // The service UUID and its mask used while filtering scan results. See
  // SetServiceUuidWithMask for what this mask does. The raw pointer returned
  // from these getters belongs to the ScanFilter object. nullptr will be
  // returned if these fields have not been set on this filter.
  UUID* service_uuid() const { return service_uuid_.get(); }
  UUID* service_uuid_mask() const { return service_uuid_mask_.get(); }

  // Sets the service UUID for this filter.
  void SetServiceUuid(const UUID& service_uuid);

  // Sets the service UUID for this filter with a 128-bit mask. The mask allows
  // the caller to partially filter scanned service UUIDs. For any of the
  // 128-bits of a UUID, set the corresponding bit in the mask to 1 to match the
  // advertised value, and 0 to ignore that bit.
  void SetServiceUuidWithMask(const UUID& service_uuid, const UUID& mask);

  // Comparison operator.
  bool operator==(const ScanFilter& rhs) const;

 private:
  std::string device_name_;
  std::string device_address_;

  std::unique_ptr<UUID> service_uuid_;
  std::unique_ptr<UUID> service_uuid_mask_;

  // TODO(armansito): Add service and manufacturer data filter fields.
};

}  // namespace bluetooth
