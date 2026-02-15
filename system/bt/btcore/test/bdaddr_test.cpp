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
#include "btcore/include/bdaddr.h"
}

TEST(HashFunctionBdaddrTest, test_same_pointer_is_same) {
  bt_bdaddr_t test_address;
  string_to_bdaddr("12:34:56:78:9A:BC", &test_address);

  EXPECT_EQ(hash_function_bdaddr(&test_address), hash_function_bdaddr(&test_address));
}

TEST(HashFunctionBdaddrTest, test_same_value_is_same) {
  bt_bdaddr_t test_address0;
  bt_bdaddr_t test_address1;
  string_to_bdaddr("12:34:56:78:9A:BC", &test_address0);
  string_to_bdaddr("12:34:56:78:9A:BC", &test_address1);

  EXPECT_EQ(hash_function_bdaddr(&test_address0), hash_function_bdaddr(&test_address1));
}

TEST(HashFunctionBdaddrTest, test_different_value_is_different) {
  bt_bdaddr_t test_address0;
  bt_bdaddr_t test_address1;
  string_to_bdaddr("12:34:56:78:9A:BC", &test_address0);
  string_to_bdaddr("43:56:21:78:9A:BC", &test_address1);

  EXPECT_NE(hash_function_bdaddr(&test_address0), hash_function_bdaddr(&test_address1));
}
