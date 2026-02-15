/******************************************************************************
 *
 *  Copyright (C) 2014 Google, Inc.
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

#include "osi/test/AllocationTestHarness.h"

extern "C" {
#include "osi/include/allocation_tracker.h"
}

void AllocationTestHarness::SetUp() {
  allocation_tracker_init();
  allocation_tracker_reset();
}

void AllocationTestHarness::TearDown() {
  EXPECT_EQ(0U, allocation_tracker_expect_no_allocations()) << "not all memory freed";
}
