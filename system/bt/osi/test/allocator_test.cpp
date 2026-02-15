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
 *  limitations under the License
 *
 ******************************************************************************/
#include <cstring>

#include <gtest/gtest.h>

#include "AllocationTestHarness.h"

extern "C" {
#include "osi/include/allocator.h"
}

class AllocatorTest : public AllocationTestHarness {};

TEST_F(AllocatorTest, test_osi_strndup) {
  char str[] = "IloveBluetooth";
  size_t len = strlen(str);
  char *copy_str = NULL;

  // len == 0
  copy_str = osi_strndup(str, 0);
  EXPECT_EQ(0, strcmp(copy_str, ""));
  osi_free(copy_str);

  // len == strlen(str)
  copy_str = osi_strndup(str, len);
  EXPECT_EQ(0, strcmp(str, copy_str));
  osi_free(copy_str);

  // len < strlen(str)
  copy_str = osi_strndup(str, len - 5);
  EXPECT_EQ(0, strcmp("IloveBlue", copy_str));
  osi_free(copy_str);

  // len > strlen(str)
  copy_str = osi_strndup(str, len + 5);
  EXPECT_EQ(0, strcmp(str, copy_str));
  osi_free(copy_str);
}
