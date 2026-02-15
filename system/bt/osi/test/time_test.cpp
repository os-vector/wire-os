/******************************************************************************
 *
 *  Copyright (C) 2015 Google, Inc.
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

#include <gtest/gtest.h>

#include "AllocationTestHarness.h"

extern "C" {
#include "osi/include/time.h"
}

// Generous upper bound: 10 seconds
static const uint32_t TEST_TIME_DELTA_UPPER_BOUND_MS = 10 * 1000;

class TimeTest : public AllocationTestHarness {};

//
// Test that the return value of time_get_os_boottime_ms() is not zero.
//
// NOTE: For now this test is disabled, because the return value
// of time_get_os_boottime_ms() is 32-bits integer that could wrap-around
// in 49.7 days. It should be re-enabled if/after the wrap-around issue
// is resolved (e.g., if the return value is 64-bits integer).
//
#if 0
TEST_F(TimeTest, test_time_get_os_boottime_ms_not_zero) {
  uint32_t t1 = time_get_os_boottime_ms();
  ASSERT_TRUE(t1 > 0);
}
#endif

//
// Test that the return value of time_get_os_boottime_ms()
// is monotonically increasing within reasonable boundries.
//
TEST_F(TimeTest, test_time_get_os_boottime_ms_increases_upper_bound) {
  uint32_t t1 = time_get_os_boottime_ms();
  uint32_t t2 = time_get_os_boottime_ms();
  ASSERT_TRUE((t2 - t1) < TEST_TIME_DELTA_UPPER_BOUND_MS);
}

//
// Test that the return value of time_get_os_boottime_ms()
// is increasing.
//
TEST_F(TimeTest, test_time_get_os_boottime_ms_increases_lower_bound) {
  static const uint32_t TEST_TIME_SLEEP_MS = 100;
  struct timespec delay;

  delay.tv_sec = TEST_TIME_SLEEP_MS / 1000;
  delay.tv_nsec = 1000 * 1000 * (TEST_TIME_SLEEP_MS % 1000);

  // Take two timestamps with sleep in-between
  uint32_t t1 = time_get_os_boottime_ms();
  int err = nanosleep(&delay, &delay);
  uint32_t t2 = time_get_os_boottime_ms();

  ASSERT_TRUE(err == 0);
  ASSERT_TRUE((t2 - t1) >= TEST_TIME_SLEEP_MS);
  ASSERT_TRUE((t2 - t1) < TEST_TIME_DELTA_UPPER_BOUND_MS);
}
