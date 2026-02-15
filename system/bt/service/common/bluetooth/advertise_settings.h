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

#include <base/macros.h>
#include <base/time/time.h>

namespace bluetooth {

// AdvertiseSettings provides a way to adjust advertising preferences for each
// Bluetooth LE advertisement instance. This is the native equivalent of the
// Android framework class defined in
// frameworks/base/core/java/android/bluetooth/le/AdvertiseSettings.java
class AdvertiseSettings {
 public:
  // Advertising mode describes power consumption mode used for advertising.
  enum Mode {
    // Perform Bluetooth LE advertising in low power mode. This is the default
    // and preferred advertising mode as it consumes the least power.
    MODE_LOW_POWER = 0x00,

    // Perform Bluetooth LE advertising in balanced power mode. This is balanced
    // between advertising frequency and power consumption.
    MODE_BALANCED = 0x01,

    // Perform Bluetooth LE advertising in low latency, high power mode. This
    // has the highest power consumption and should not be used for continuous
    // background advertising.
    MODE_LOW_LATENCY = 0x02,
  };

  // Levels that can be set for advertising transmission power.
  enum TxPowerLevel {
    // Advertise using the lowest transmission (TX) power level. Low
    // transmission power can be used to restrict the visibility range of
    // advertising packets.
    TX_POWER_LEVEL_ULTRA_LOW = 0x00,

    // Advertise using low TX power level.
    TX_POWER_LEVEL_LOW = 0x01,

    // Advertise using medium TX power level.
    TX_POWER_LEVEL_MEDIUM = 0x02,

    // Advertise using high TX power level. This corresponds to largest
    // visibility range of the advertising packet.
    TX_POWER_LEVEL_HIGH = 0x03,
  };

  AdvertiseSettings(Mode mode,
                    base::TimeDelta timeout,
                    TxPowerLevel tx_power_level,
                    bool connectable);

  // The default constructor sets all fields to defaults:
  //   mode: MODE_LOW_POWER
  //   TX power level: TX_POWER_LEVEL_MEDIUM
  //   connectable: true
  AdvertiseSettings();
  ~AdvertiseSettings() = default;

  // Returns the advertise mode.
  Mode mode() const { return mode_; }

  // Returns the advertising time limit in milliseconds.
  const base::TimeDelta& timeout() const { return timeout_; }

  // Returns the TX power level for advertising.
  TxPowerLevel tx_power_level() const { return tx_power_level_; }

  // Returns whether the advertisement will indicate connectable.
  bool connectable() const { return connectable_; }

  // Comparison operator.
  bool operator==(const AdvertiseSettings& rhs) const;

 private:
  Mode mode_;
  base::TimeDelta timeout_;
  TxPowerLevel tx_power_level_;
  bool connectable_;
};

}  // namespace bluetooth
