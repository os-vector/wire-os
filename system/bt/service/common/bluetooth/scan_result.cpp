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

#include "service/common/bluetooth/scan_result.h"

#include <base/logging.h>

#include "service/common/bluetooth/util/address_helper.h"


namespace bluetooth {

ScanResult::ScanResult(const std::string& device_address,
                       const std::vector<uint8_t> scan_record,
                       int rssi)
    : device_address_(device_address),
      scan_record_(scan_record),
      rssi_(rssi) {
  CHECK(util::IsAddressValid(device_address)) << "Invalid BD_ADDR given: "
                                              << device_address;
}

bool ScanResult::operator==(const ScanResult& rhs) const {
  if (device_address_ != rhs.device_address_)
    return false;

  if (scan_record_ != rhs.scan_record_)
    return false;

  if (rssi_ != rhs.rssi_)
    return false;

  return true;
}

}  // namespace bluetooth
