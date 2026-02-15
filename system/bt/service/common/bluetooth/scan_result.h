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

#include <stdint.h>

#include <string>
#include <vector>

namespace bluetooth {

// ScanResult represents a single Bluetooth LE device scan result. It
// encapsulates information about discovered LE devices.
class ScanResult {
 public:
  ScanResult(const std::string& device_address,
             const std::vector<uint8_t> scan_record,
             int rssi);
  ScanResult() = default;
  ~ScanResult() = default;

  // Returns the remote BD_ADDR associated with this scan result.
  const std::string& device_address() const { return device_address_; }

  // Returns the scan record (advertising +scan-response data) associated with
  // this scan result.
  const std::vector<uint8_t>& scan_record() const { return scan_record_; }

  // Returns the RSSI associated with this scan result.
  int rssi() const { return rssi_; }

  // Comparison operator.
  bool operator==(const ScanResult& rhs) const;

 private:
  std::string device_address_;
  std::vector<uint8_t> scan_record_;
  int rssi_;
};

}  // namespace bluetooth
