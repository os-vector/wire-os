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
#include <cstring>

#include <gtest/gtest.h>

#include "AllocationTestHarness.h"

extern "C" {
#include "osi/include/hash_map.h"
#include "osi/include/hash_map_utils.h"
#include "osi/include/allocator.h"
}

class HashMapUtilsTest : public AllocationTestHarness {
 protected:
  virtual void SetUp() {
    AllocationTestHarness::SetUp();
    map = NULL;
  }
  virtual void TearDown() {
    hash_map_free(map);
    AllocationTestHarness::TearDown();
  }

  hash_map_t *map;
};

TEST_F(HashMapUtilsTest, test_empty_string_params) {
  char params[] = "";
  map = hash_map_utils_new_from_string_params(params);
  EXPECT_TRUE(hash_map_is_empty(map));
}

TEST_F(HashMapUtilsTest, test_semicolons) {
  char params[] = ";;;";
  map = hash_map_utils_new_from_string_params(params);
  EXPECT_TRUE(hash_map_is_empty(map));
}

TEST_F(HashMapUtilsTest, test_equal_sign_in_value) {
  char params[] = "keyOfSomething=value=OfSomething";
  char key[] = "keyOfSomething";
  char value[] = "value=OfSomething";
  map = hash_map_utils_new_from_string_params(params);
  EXPECT_EQ(1u, hash_map_size(map));
  EXPECT_EQ(0, strcmp(value, (char *)hash_map_get(map, key)));
}

TEST_F(HashMapUtilsTest, test_two_pairs_with_same_key) {
  char params[] = "key=valu0;key=value1";
  char key[] = "key";
  char value1[] = "value1";
  map = hash_map_utils_new_from_string_params(params);
  EXPECT_EQ(1u, hash_map_size(map));
  EXPECT_EQ(0, strcmp(value1, (char *)hash_map_get(map, key)));
}

TEST_F(HashMapUtilsTest, test_one_key_value_pair_without_semicolon) {
  char params[] = "keyOfSomething=valueOfSomething";
  char key[] = "keyOfSomething";
  char value[] = "valueOfSomething";
  map = hash_map_utils_new_from_string_params(params);
  EXPECT_EQ(1u, hash_map_size(map));
  EXPECT_EQ(0, strcmp(value, (char *)hash_map_get(map, key)));
}

TEST_F(HashMapUtilsTest, test_one_key_value_pair_with_semicolon) {
  char params[] = "keyOfSomething=valueOfSomething;";
  char key[] = "keyOfSomething";
  char value[] = "valueOfSomething";
  map = hash_map_utils_new_from_string_params(params);
  EXPECT_EQ(1u, hash_map_size(map));
  EXPECT_EQ(0, strcmp(value, (char *)hash_map_get(map, key)));
}

TEST_F(HashMapUtilsTest, test_one_pair_with_empty_value) {
  char params[] = "keyOfSomething=;";
  char key[] = "keyOfSomething";
  char value[] = "";
  map = hash_map_utils_new_from_string_params(params);
  EXPECT_EQ(1u, hash_map_size(map));
  EXPECT_EQ(0, strcmp(value, (char *)hash_map_get(map, key)));
}

TEST_F(HashMapUtilsTest, test_one_pair_with_empty_key) {
  char params[] = "=valueOfSomething;";
  map = hash_map_utils_new_from_string_params(params);
  EXPECT_TRUE(hash_map_is_empty(map));
}

TEST_F(HashMapUtilsTest, test_two_key_value_pairs) {
  char params[] = "key0=value0;key1=value1;";
  char key0[] = "key0";
  char value0[] = "value0";
  char key1[] = "key1";
  char value1[] = "value1";
  map = hash_map_utils_new_from_string_params(params);
  EXPECT_EQ(2u, hash_map_size(map));
  EXPECT_EQ(0, strcmp(value0, (char *)hash_map_get(map, key0)));
  EXPECT_EQ(0, strcmp(value1, (char *)hash_map_get(map, key1)));
}

TEST_F(HashMapUtilsTest, test_dump_null_map) {
  hash_map_t *map = NULL;
  hash_map_utils_dump_string_keys_string_values(map);
}
