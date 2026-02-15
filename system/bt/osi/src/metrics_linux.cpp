/******************************************************************************
 *
 *  Copyright (C) 2016 Google, Inc.
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at:
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 ******************************************************************************/


#define LOG_TAG "bt_osi_metrics"

extern "C" {
#include "osi/include/metrics.h"
}

void metrics_pair_event(uint32_t disconnect_reason, uint64_t timestamp_ms,
                        uint32_t device_class, device_type_t device_type) {
  //TODO(jpawlowski): implement
}

void metrics_wake_event(wake_event_type_t type, const char *requestor,
                        const char *name, uint64_t timestamp_ms) {
  //TODO(jpawlowski): implement
}

void metrics_scan_event(bool start, const char *initator, scan_tech_t type,
                        uint32_t results, uint64_t timestamp_ms) {
  //TODO(jpawlowski): implement
}

void metrics_a2dp_session(int64_t session_duration_sec,
                          const char *disconnect_reason,
                          uint32_t device_class,
                          int32_t media_timer_min_ms,
                          int32_t media_timer_max_ms,
                          int32_t media_timer_avg_ms,
                          int32_t buffer_overruns_max_count,
                          int32_t buffer_overruns_total,
                          float buffer_underruns_average,
                          int32_t buffer_underruns_count) {
  //TODO(jpawlowski): implement
}

void metrics_write(int fd, bool clear) {
  //TODO(jpawlowski): implement
}

void metrics_print(int fd, bool clear) {
  //TODO(jpawlowski): implement
}
