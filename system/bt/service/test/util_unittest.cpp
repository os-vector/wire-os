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

#include <gtest/gtest.h>

#include "service/common/bluetooth/util/address_helper.h"

namespace util {

TEST(UtilTest, IsAddressValid) {
  EXPECT_FALSE(IsAddressValid(""));
  EXPECT_FALSE(IsAddressValid("000000000000"));
  EXPECT_FALSE(IsAddressValid("00:00:00:00:0000"));
  EXPECT_FALSE(IsAddressValid("00:00:00:00:00:0"));
  EXPECT_FALSE(IsAddressValid("00:00:00:00:00:0;"));
  EXPECT_TRUE(IsAddressValid("00:00:00:00:00:00"));
  EXPECT_FALSE(IsAddressValid("aB:cD:eF:Gh:iJ:Kl"));
}

TEST(UtilTest, BdAddrFromString) {
  bt_bdaddr_t addr;
  memset(&addr, 0, sizeof(addr));

  EXPECT_TRUE(BdAddrFromString("00:00:00:00:00:00", &addr));
  const bt_bdaddr_t result0 = {{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }};
  EXPECT_EQ(0, memcmp(&addr, &result0, sizeof(addr)));

  EXPECT_TRUE(BdAddrFromString("ab:01:4C:d5:21:9f", &addr));
  const bt_bdaddr_t result1 = {{ 0xab, 0x01, 0x4c, 0xd5, 0x21, 0x9f }};
  EXPECT_EQ(0, memcmp(&addr, &result1, sizeof(addr)));
}

}  // namespace util
