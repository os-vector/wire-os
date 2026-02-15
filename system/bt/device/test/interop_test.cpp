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

extern "C" {
#include "device/include/interop.h"
}

TEST(InteropTest, test_lookup_hit) {
  bt_bdaddr_t test_address;
  string_to_bdaddr("38:2c:4a:e6:67:89", &test_address);
  EXPECT_TRUE(interop_match_addr(INTEROP_DISABLE_LE_SECURE_CONNECTIONS, &test_address));
  string_to_bdaddr("9c:df:03:12:34:56", &test_address);
  EXPECT_TRUE(interop_match_addr(INTEROP_AUTO_RETRY_PAIRING, &test_address));
}

TEST(InteropTest, test_lookup_miss) {
  bt_bdaddr_t test_address;
  string_to_bdaddr("00:00:00:00:00:00", &test_address);
  EXPECT_FALSE(interop_match_addr(INTEROP_DISABLE_LE_SECURE_CONNECTIONS, &test_address));
  string_to_bdaddr("ff:ff:ff:ff:ff:ff", &test_address);
  EXPECT_FALSE(interop_match_addr(INTEROP_AUTO_RETRY_PAIRING, &test_address));
  string_to_bdaddr("42:08:15:ae:ae:ae", &test_address);
  EXPECT_FALSE(interop_match_addr(INTEROP_DISABLE_LE_SECURE_CONNECTIONS, &test_address));
  string_to_bdaddr("38:2c:4a:59:67:89", &test_address);
  EXPECT_FALSE(interop_match_addr(INTEROP_AUTO_RETRY_PAIRING, &test_address));
}

TEST(InteropTest, test_dynamic) {
  bt_bdaddr_t test_address;

  string_to_bdaddr("11:22:33:44:55:66", &test_address);
  EXPECT_FALSE(interop_match_addr(INTEROP_DISABLE_LE_SECURE_CONNECTIONS, &test_address));

  interop_database_add(INTEROP_DISABLE_LE_SECURE_CONNECTIONS, &test_address, 3);
  EXPECT_TRUE(interop_match_addr(INTEROP_DISABLE_LE_SECURE_CONNECTIONS, &test_address));
  EXPECT_FALSE(interop_match_addr(INTEROP_AUTO_RETRY_PAIRING, &test_address));

  string_to_bdaddr("66:55:44:33:22:11", &test_address);
  EXPECT_FALSE(interop_match_addr(INTEROP_AUTO_RETRY_PAIRING, &test_address));

  interop_database_add(INTEROP_AUTO_RETRY_PAIRING, &test_address, 3);
  EXPECT_TRUE(interop_match_addr(INTEROP_AUTO_RETRY_PAIRING, &test_address));
  EXPECT_FALSE(interop_match_addr(INTEROP_DISABLE_LE_SECURE_CONNECTIONS, &test_address));

  interop_database_clear();
  EXPECT_FALSE(interop_match_addr(INTEROP_AUTO_RETRY_PAIRING, &test_address));
}

TEST(InteropTest, test_name_hit) {
  EXPECT_TRUE(interop_match_name(INTEROP_DISABLE_AUTO_PAIRING, "BMW M3"));
  EXPECT_TRUE(interop_match_name(INTEROP_DISABLE_AUTO_PAIRING, "Audi"));
  EXPECT_TRUE(interop_match_name(INTEROP_DISABLE_AUTO_PAIRING, "Caramel")); // Starts with "Car" ;)
}

TEST(InteropTest, test_name_miss) {
  EXPECT_FALSE(interop_match_name(INTEROP_DISABLE_AUTO_PAIRING, "__GOOGLE__"));
  EXPECT_FALSE(interop_match_name(INTEROP_DISABLE_AUTO_PAIRING, "BM"));
  EXPECT_FALSE(interop_match_name(INTEROP_DISABLE_AUTO_PAIRING, "audi"));
  EXPECT_FALSE(interop_match_name(INTEROP_AUTO_RETRY_PAIRING, "BMW M3"));
}
