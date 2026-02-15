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

extern "C" {
#include "btif/include/btif_storage.h"
#include "btif/include/btif_util.h"
}

TEST(BtifStorageTest, test_string_to_uuid) {
  const char *s1 = "e39c6285-867f-4b1d-9db0-35fbd9aebf22";
  const uint8_t u1[] = {0xe3, 0x9c, 0x62, 0x85, 0x86, 0x7f, 0x4b, 0x1d,
                        0x9d, 0xb0, 0x35, 0xfb, 0xd9, 0xae, 0xbf, 0x22};

  bt_uuid_t uuid;
  memset(&uuid, 0, sizeof(uuid));
  EXPECT_FALSE(memcmp(&uuid, u1, sizeof(u1)) == 0);

  bool rc = string_to_uuid(s1, &uuid);
  EXPECT_TRUE(rc);
  EXPECT_TRUE(memcmp(&uuid, u1, sizeof(u1)) == 0);
}

TEST(BtifStorageTest, test_string_to_uuid_invalid) {
  bt_uuid_t uuid;
  bool rc = string_to_uuid("This is not a UUID", &uuid);
  EXPECT_FALSE(rc);
}

TEST(BtifStorageTest, test_uuid_split_multiple) {
  const char *s1 = "e39c6285-867f-4b1d-9db0-35fbd9aebf22 e39c6285-867f-4b1d-9db0-35fbd9aebf23";
  const uint8_t u1[] = {0xe3, 0x9c, 0x62, 0x85, 0x86, 0x7f, 0x4b, 0x1d,
                        0x9d, 0xb0, 0x35, 0xfb, 0xd9, 0xae, 0xbf, 0x22};
  const uint8_t u2[] = {0xe3, 0x9c, 0x62, 0x85, 0x86, 0x7f, 0x4b, 0x1d,
                        0x9d, 0xb0, 0x35, 0xfb, 0xd9, 0xae, 0xbf, 0x23};

  bt_uuid_t uuids[2];
  size_t num_uuids = btif_split_uuids_string(s1, uuids, 2);
  EXPECT_EQ(num_uuids, 2u);
  EXPECT_TRUE(memcmp(&uuids[0], u1, sizeof(u1)) == 0);
  EXPECT_TRUE(memcmp(&uuids[1], u2, sizeof(u2)) == 0);
}

TEST(BtifStorageTest, test_uuid_split_partial) {
  const char *s1 = "e39c6285-867f-4b1d-9db0-35fbd9aebf22 e39c6285-867f-4b1d-9db0-35fbd9aebf23";

  bt_uuid_t uuids[2];
  size_t num_uuids = btif_split_uuids_string(s1, uuids, 1);
  EXPECT_EQ(num_uuids, 1u);
}
