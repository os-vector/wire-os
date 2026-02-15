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
#include "btcore/include/uuid.h"
}

static const char *UUID_EMPTY = "00000000-0000-0000-0000-000000000000";
static const char *UUID_ONES = "11111111-1111-1111-1111-111111111111";
static const char *UUID_SEQUENTIAL = "01234567-89ab-cdef-ABCD-EF0123456789";
static const char *UUID_BASE = "00000000-0000-1000-8000-00805f9b34fb";

class UuidTest : public AllocationTestHarness {
  protected:
    virtual void SetUp() {
    }

    virtual void TearDown() {
    }
};

TEST_F(UuidTest, new_from_string) {
  bt_uuid_t *uuid;

  uuid = uuid_new("incorrect length");
  EXPECT_EQ(NULL, uuid);

  uuid = uuid_new("correct length but missing dashes --");
  EXPECT_EQ(NULL, uuid);

  uuid = uuid_new(UUID_ONES);
  ASSERT_TRUE(uuid != NULL);
  for (int i = 0; i < 16; i++) {
    EXPECT_EQ(0x11, uuid->uu[i]);
  }
  uuid_free(uuid);

  uuid = uuid_new(UUID_SEQUENTIAL);
  EXPECT_EQ(0x01, uuid->uu[0]);
  EXPECT_EQ(0x23, uuid->uu[1]);
  EXPECT_EQ(0x45, uuid->uu[2]);
  EXPECT_EQ(0x67, uuid->uu[3]);
  EXPECT_EQ(0x89, uuid->uu[4]);
  EXPECT_EQ(0xAB, uuid->uu[5]);
  EXPECT_EQ(0xCD, uuid->uu[6]);
  EXPECT_EQ(0xEF, uuid->uu[7]);
  EXPECT_EQ(0xab, uuid->uu[8]);
  EXPECT_EQ(0xcd, uuid->uu[9]);
  EXPECT_EQ(0xef, uuid->uu[10]);
  EXPECT_EQ(0x01, uuid->uu[11]);
  EXPECT_EQ(0x23, uuid->uu[12]);
  EXPECT_EQ(0x45, uuid->uu[13]);
  EXPECT_EQ(0x67, uuid->uu[14]);
  EXPECT_EQ(0x89, uuid->uu[15]);
  uuid_free(uuid);

  uuid = uuid_new(UUID_BASE);
  EXPECT_EQ(0x00, uuid->uu[0]);
  EXPECT_EQ(0x00, uuid->uu[1]);
  EXPECT_EQ(0x00, uuid->uu[2]);
  EXPECT_EQ(0x00, uuid->uu[3]);
  EXPECT_EQ(0x00, uuid->uu[4]);
  EXPECT_EQ(0x00, uuid->uu[5]);
  EXPECT_EQ(0x10, uuid->uu[6]);
  EXPECT_EQ(0x00, uuid->uu[7]);
  EXPECT_EQ(0x80, uuid->uu[8]);
  EXPECT_EQ(0x00, uuid->uu[9]);
  EXPECT_EQ(0x00, uuid->uu[10]);
  EXPECT_EQ(0x80, uuid->uu[11]);
  EXPECT_EQ(0x5f, uuid->uu[12]);
  EXPECT_EQ(0x9b, uuid->uu[13]);
  EXPECT_EQ(0x34, uuid->uu[14]);
  EXPECT_EQ(0xfb, uuid->uu[15]);
  uuid_free(uuid);
}

TEST_F(UuidTest, uuid_is_empty) {
  bt_uuid_t *uuid = NULL;

  uuid = uuid_new(UUID_EMPTY);
  ASSERT_TRUE(uuid != NULL);
  EXPECT_TRUE(uuid_is_empty(uuid));
  uuid_free(uuid);

  uuid = uuid_new(UUID_BASE);
  ASSERT_TRUE(uuid != NULL);
  EXPECT_FALSE(uuid_is_empty(uuid));
  uuid_free(uuid);
}

TEST_F(UuidTest, uuid_128_to_16) {
  bt_uuid_t *uuid = NULL;
  uint16_t uuid16 = 0xffff;

  uuid = uuid_new(UUID_ONES);
  EXPECT_FALSE(uuid_128_to_16(uuid, &uuid16));
  uuid_free(uuid);
  EXPECT_EQ((uint16_t)0xffff, uuid16);

  uuid = uuid_new(UUID_BASE);
  EXPECT_TRUE(uuid_128_to_16(uuid, &uuid16));
  uuid_free(uuid);
  EXPECT_NE((uint16_t)0xffff, uuid16);
  EXPECT_EQ((uint16_t)0, uuid16);
}

TEST_F(UuidTest, uuid_128_to_32) {
  bt_uuid_t *uuid = NULL;
  uint32_t uuid32 = 0xffffffff;

  uuid = uuid_new(UUID_ONES);
  EXPECT_FALSE(uuid_128_to_32(uuid, &uuid32));
  uuid_free(uuid);
  EXPECT_EQ((uint32_t)0xffffffff, uuid32);

  uuid = uuid_new(UUID_BASE);
  EXPECT_TRUE(uuid_128_to_32(uuid, &uuid32));
  uuid_free(uuid);
  EXPECT_NE((uint32_t)0xffffffff, uuid32);
  EXPECT_EQ((uint32_t)0, uuid32);
}

TEST_F(UuidTest, uuid_to_string) {
  bt_uuid_t *uuid = NULL;

  uuid_string_t *uuid_string = uuid_string_new();
  EXPECT_TRUE(uuid_string != NULL);

  uuid = uuid_new(UUID_BASE);
  EXPECT_TRUE(uuid != NULL);
  uuid_to_string(uuid, uuid_string);
  uuid_free(uuid);

  EXPECT_TRUE(!strcmp(UUID_BASE, uuid_string_data(uuid_string)));

  uuid = uuid_new(UUID_SEQUENTIAL);
  EXPECT_TRUE(uuid != NULL);

  uuid_to_string(uuid, uuid_string);
  uuid_free(uuid);

  char lower_case_buf[36+1];
  for (int i = 0; i < 36+1; i++) {
    lower_case_buf[i] = tolower(UUID_SEQUENTIAL[i]);
  }
  EXPECT_TRUE(!strcmp(lower_case_buf, uuid_string_data(uuid_string)));
  uuid_string_free(uuid_string);
}
