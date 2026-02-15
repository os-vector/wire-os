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

#include <base/time/time.h>

namespace bluetooth {

// ScanSettings encapsulates Bluetooth LE device scan parameters. This is the
// native equivalent of the Android framework class defined in
// frameworks/base/core/java/android/bluetooth/le/ScanSettings.java.
class ScanSettings {
 public:
  // A scan mode describes the power consumption involved in LE scans.
  enum Mode {
    // A special Bluetooth LE scan mode. Applications using this scan mode will
    // passively listen for other scan results without starting BLE scans
    // themselves.
    MODE_OPPORTUNISTIC = -1,

    // Perform Bluetooth LE scan in low power mode. This is the default scan
    // mode as it consumes the least power.
    MODE_LOW_POWER = 0,

    // Perform Bluetooth LE scan in balanced power mode. Scan results are
    // returned at a rate that provides a good trade-off between scan frequency
    // and power consumption.
    MODE_BALANCED = 1,

    // Scan using the highest duty cycle. It's recommended to only use this mode
    // when the application is running in the foreground.
    MODE_LOW_LATENCY = 2,
  };

  // A callback type describes how scan results will be reported to applications
  // in asynchronous callbacks.
  enum CallbackType {
    // Trigger a callback for every Bluetooth advertisement found that matches
    // the filter criteria. If no filter is active, all advertisement packets
    // are reported.
    CALLBACK_TYPE_ALL_MATCHES = 1,

    // A result callback is only triggered for the first advertisement packet
    // received that matches the filter criteria. This requires that the
    // hardware support the offloaded filtering feature.
    CALLBACK_TYPE_FIRST_MATCH = 2,

    // Receive a callback when advertisements are no longer received from a
    // device that has been previously reported by a first match callback. This
    // requires that the hardware support the offloaded filtering feature.
    CALLBACK_TYPE_MATCH_LOST = 4,
  };
  using CallbackTypeBitField = int;

  // Determines how many advertisements to match per filter.
  enum MatchCount {
    // Match one advertisement per filter.
    MATCH_COUNT_ONE_ADVERTISEMENT = 1,

    // Match few advertisements per filter depending on the current capability
    // and availability of hardware resources.
    MATCH_COUNT_FEW_ADVERTISEMENTS = 2,

    // Match as many advertisements per filter as the underlying hardware can
    // allow, depending on the current capability and availability of hardware
    // resources.
    MATCH_COUNT_MAX_ADVERTISEMENTS = 3,
  };

  // Hardware filter match mode.
  enum MatchMode {
    // In aggressive mode the hardware will determine a match sooner even with
    // feeble signal strength and a low number of sightings in a duration.
    MATCH_MODE_AGGRESSIVE = 1,

    // In sticky mode a higher threshold of signal strength and sightings is
    // required before a scan result is reported by the hardware.
    MATCH_MODE_STICKY = 2,
  };

  // Scan result type describes the contents of each scan result.
  enum ResultType {
    // Request full scan results which contain the device name, RSSI,
    // advertising data, scan response data, and the scan timestamp.
    RESULT_TYPE_FULL = 0,

    // Request abbreviated scan results which contain the device name, RSSI, and
    // scan timestamp.
    // Note: It is possible for an application to get more scan results than it
    // asked for, if there are multiple apps using this type.
    RESULT_TYPE_ABBREVIATED = 1,
  };

  // The default constructor sets all fields to defaults:
  //   mode: MODE_LOW_POWER
  //   callback_type: CALLBACK_TYPE_ALL_MATCHES
  //   result_type: RESULT_TYPE_FULL
  //   report_delay_ms: 0
  //   match_mode: MATCH_MODE_AGGRESSIVE
  //   match_count_per_filter: MATCH_COUNT_MAX_ADVERTISEMENTS
  ScanSettings();
  ScanSettings(Mode mode,
               CallbackTypeBitField callback_type,
               ResultType result_type,
               base::TimeDelta report_delay_ms,
               MatchMode match_mode,
               MatchCount match_count_per_filter);
  ~ScanSettings() = default;

  // Returns the scan mode.
  Mode mode() const { return mode_; }
  void set_mode(Mode mode) { mode_ = mode; }

  // Returns the callback type.
  CallbackTypeBitField callback_type() const { return callback_type_; }
  void set_callback_type(CallbackTypeBitField type) { callback_type_ = type; }

  // Returns the scan result type.
  ResultType result_type() const { return result_type_; }
  void set_result_type(ResultType type) { result_type_ = type; }

  // Returns the report delay value in milliseconds.
  const base::TimeDelta& report_delay() const { return report_delay_ms_; }
  void set_report_delay(const base::TimeDelta& delay) {
    report_delay_ms_ = delay;
  }

  // Returns the hardware filter match mode.
  MatchMode match_mode() const { return match_mode_; }
  void set_match_mode(MatchMode mode) { match_mode_ = mode; }

  // Returns the count of advertisements to match per filter.
  MatchCount match_count_per_filter() const { return match_count_per_filter_; }
  void set_match_count_per_filter(MatchCount count) {
    match_count_per_filter_ = count;
  }

  // Comparison operator.
  bool operator==(const ScanSettings& rhs) const;

 private:
  Mode mode_;
  CallbackTypeBitField callback_type_;
  ResultType result_type_;
  base::TimeDelta report_delay_ms_;
  MatchMode match_mode_;
  MatchCount match_count_per_filter_;
};

}  // namespace bluetooth
