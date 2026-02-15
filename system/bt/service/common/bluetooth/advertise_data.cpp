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

#include "service/common/bluetooth/advertise_data.h"

#include <base/logging.h>

#include "stack/include/bt_types.h"
#include "stack/include/hcidefs.h"

namespace bluetooth {

AdvertiseData::AdvertiseData(const std::vector<uint8_t>& data)
    : data_(data),
      include_device_name_(false),
      include_tx_power_level_(false) {
}

AdvertiseData::AdvertiseData()
    : include_device_name_(false),
      include_tx_power_level_(false) {
}

AdvertiseData::AdvertiseData(const AdvertiseData& other)
    : data_(other.data_),
      include_device_name_(other.include_device_name_),
      include_tx_power_level_(other.include_tx_power_level_) {
}

bool AdvertiseData::IsValid() const {
  size_t len = data_.size();

  // Consider empty data as valid.
  if (!len)
    return true;

  for (size_t i = 0, field_len = 0; i < len; i += (field_len + 1)) {
    field_len = data_[i];

    // If the length of the current field would exceed the total data length,
    // then the data is badly formatted.
    if (i + field_len >= len) {
      VLOG(1) << "Advertising data badly formatted";
      return false;
    }

    // A field length of 0 would be invalid as it should at least contain the
    // EIR field type.
    if (field_len < 1)
      return false;

    uint8_t type = data_[i + 1];

    // Clients are not allowed to set the following EIR fields as these are
    // managed by stack.
    switch (type) {
    case HCI_EIR_FLAGS_TYPE:
    case HCI_EIR_TX_POWER_LEVEL_TYPE:
    case HCI_EIR_SHORTENED_LOCAL_NAME_TYPE:
    case HCI_EIR_COMPLETE_LOCAL_NAME_TYPE:
    case HCI_EIR_OOB_BD_ADDR_TYPE:
    case HCI_EIR_OOB_COD_TYPE:
    case HCI_EIR_OOB_SSP_HASH_C_TYPE:
    case HCI_EIR_OOB_SSP_RAND_R_TYPE:
      VLOG(1) << "Cannot set EIR field type: " << type;
      return false;
    default:
      break;
    }
  }

  return true;
}

bool AdvertiseData::operator==(const AdvertiseData& rhs) const {
  if (include_tx_power_level_ != rhs.include_tx_power_level_)
    return false;

  if (include_device_name_ != rhs.include_device_name_)
    return false;

  return data_ == rhs.data_;
}

AdvertiseData& AdvertiseData::operator=(const AdvertiseData& other) {
  if (this == &other)
    return *this;

  data_ = other.data_;
  include_device_name_ = other.include_device_name_;
  include_tx_power_level_ = other.include_tx_power_level_;

  return *this;
}

}  // namespace bluetooth
