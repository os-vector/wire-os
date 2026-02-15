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

#include "service/common/bluetooth/advertise_settings.h"

namespace bluetooth {

AdvertiseSettings::AdvertiseSettings(
    Mode mode,
    base::TimeDelta timeout,
    TxPowerLevel tx_power_level,
    bool connectable)
    : mode_(mode),
      timeout_(timeout),
      tx_power_level_(tx_power_level),
      connectable_(connectable) {
}

// Default values are taken from the AdvertiseSettings.java
AdvertiseSettings::AdvertiseSettings()
    : mode_(MODE_LOW_POWER),
      tx_power_level_(TX_POWER_LEVEL_MEDIUM),
      connectable_(true) {
}

bool AdvertiseSettings::operator==(const AdvertiseSettings& rhs) const {
  if (mode_ != rhs.mode_)
    return false;

  if (timeout_ != rhs.timeout_)
    return false;

  if (tx_power_level_ != rhs.tx_power_level_)
    return false;

  if (connectable_ != rhs.connectable_)
    return false;

  return true;
}

}  // namespace bluetooth
