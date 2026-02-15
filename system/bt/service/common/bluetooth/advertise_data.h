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

#include <vector>

#include <base/macros.h>

namespace bluetooth {

// Represents a data packet for Bluetooth Low Energy advertisements. This is the
// native equivalent of the Android framework class defined in
// frameworks/base/core/j/android/bluetooth/le/AdvertiseData.java
class AdvertiseData final {
 public:
  // Constructs an AdvertiseData with the given parameters. |data| can only
  // contain the "Service UUIDs", "Service Data", and "Manufacturer Data" fields
  // as specified in the Core Specification Supplement. |data| must be properly
  // formatted according to the supplement and contains the data as it will be
  // sent over the wire.
  //
  // The values for include_device_name() and include_tx_power_level() are
  // initialized to false by default. These can be modified using the setters
  // declared below.
  explicit AdvertiseData(const std::vector<uint8_t>& data);

  // Default constructor initializes all fields to be empty/false.
  AdvertiseData();
  AdvertiseData(const AdvertiseData& other);
  ~AdvertiseData() = default;

  // Returns true if the advertising data is formatted correctly according to
  // the TLV format.
  bool IsValid() const;

  // data() returns the current advertising data contained by this instance. The
  // data is in the TLV format as specified in the Bluetooth Core Specification.
  const std::vector<uint8_t>& data() const { return data_; }

  // Whether the device name should be included in the advertisement packet.
  bool include_device_name() const { return include_device_name_; }
  void set_include_device_name(bool value) { include_device_name_ = value; }

  // Whether the transmission power level should be included in the
  // advertisement packet.
  bool include_tx_power_level() const { return include_tx_power_level_; }
  void set_include_tx_power_level(bool value) {
    include_tx_power_level_ = value;
  }

  // Comparison operator.
  bool operator==(const AdvertiseData& rhs) const;

  // Assignment operator
  AdvertiseData& operator=(const AdvertiseData& other);

 private:
  std::vector<uint8_t> data_;
  bool include_device_name_;
  bool include_tx_power_level_;
};

}  // namespace bluetooth
