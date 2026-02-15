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

#include <gtest/gtest.h>

#include "AllocationTestHarness.h"

extern "C" {
#include "osi/include/properties.h"
}

class PropertiesTest : public AllocationTestHarness {};

TEST_F(PropertiesTest, test_default_value) {
  char value[PROPERTY_VALUE_MAX] = {0};
  osi_property_get("very.useful.test", value, "very_useful_value");
  ASSERT_STREQ(value, "very_useful_value");
}

TEST_F(PropertiesTest, test_successfull_set_and_get_value) {
  char value[PROPERTY_VALUE_MAX] = "nothing_interesting";
  int ret = osi_property_set("very.useful.set.test", value);
  ASSERT_EQ(0, ret);

  char received[PROPERTY_VALUE_MAX];
  osi_property_get("very.useful.set.test", received, NULL);
  ASSERT_STREQ(received, "nothing_interesting");
}
