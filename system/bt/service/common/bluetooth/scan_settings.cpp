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

#include "service/common/bluetooth/scan_settings.h"

namespace bluetooth {

ScanSettings::ScanSettings()
    : mode_(MODE_LOW_POWER),
      callback_type_(CALLBACK_TYPE_ALL_MATCHES),
      result_type_(RESULT_TYPE_FULL),
      match_count_per_filter_(MATCH_COUNT_MAX_ADVERTISEMENTS) {
}

ScanSettings::ScanSettings(
    Mode mode,
    CallbackTypeBitField callback_type,
    ResultType result_type,
    base::TimeDelta report_delay_ms,
    MatchMode match_mode,
    MatchCount match_count_per_filter)
    : mode_(mode),
      callback_type_(callback_type),
      result_type_(result_type),
      report_delay_ms_(report_delay_ms),
      match_mode_(match_mode),
      match_count_per_filter_(match_count_per_filter) {
}

bool ScanSettings::operator==(const ScanSettings& rhs) const {
  if (mode_ != rhs.mode_)
    return false;

  if (callback_type_ != rhs.callback_type_)
    return false;

  if (result_type_ != rhs.result_type_)
    return false;

  if (report_delay_ms_ != rhs.report_delay_ms_)
    return false;

  if (match_mode_ != rhs.match_mode_)
    return false;

  if (match_count_per_filter_ != rhs.match_count_per_filter_)
    return false;

  return true;
}

}  // namespace bluetooth
