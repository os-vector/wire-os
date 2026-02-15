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

extern "C" {
#include "osi/include/allocation_tracker.h"

void allocation_tracker_uninit(void);
}

static const allocator_id_t allocator_id = 5;

TEST(AllocationTrackerTest, test_uninit_no_bad_effects) {
  void *dummy_allocation = malloc(4);

  // Ensure uninitialized state (previous tests may have called init)
  allocation_tracker_uninit();

  EXPECT_EQ(4U, allocation_tracker_resize_for_canary(4));
  allocation_tracker_notify_alloc(allocator_id, dummy_allocation, 4);
  EXPECT_EQ(0U, allocation_tracker_expect_no_allocations()); // should not have registered an allocation
  allocation_tracker_notify_free(allocator_id, dummy_allocation);
  EXPECT_EQ(0U, allocation_tracker_expect_no_allocations());

  free(dummy_allocation);
}

TEST(AllocationTrackerTest, test_canaries_on) {
  allocation_tracker_uninit();
  allocation_tracker_init();

  size_t with_canary_size = allocation_tracker_resize_for_canary(4);
  EXPECT_TRUE(with_canary_size > 4);

  void *dummy_allocation = malloc(with_canary_size);
  void *useable_ptr = allocation_tracker_notify_alloc(allocator_id, dummy_allocation, 4);
  EXPECT_TRUE(useable_ptr > dummy_allocation);
  EXPECT_EQ(4U, allocation_tracker_expect_no_allocations()); // should have registered the allocation
  void *freeable_ptr = allocation_tracker_notify_free(allocator_id, useable_ptr);
  EXPECT_EQ(dummy_allocation, freeable_ptr);
  EXPECT_EQ(0U, allocation_tracker_expect_no_allocations());

  free(dummy_allocation);
}
