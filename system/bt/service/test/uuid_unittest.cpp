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

#include <algorithm>
#include <array>
#include <stdint.h>

#include <gtest/gtest.h>

#include "service/common/bluetooth/uuid.h"

using namespace bluetooth;

namespace {

const std::array<uint8_t, UUID::kNumBytes128> kBtSigBaseUUID = {
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00,
      0x80, 0x00, 0x00, 0x80, 0x5f, 0x9b, 0x34, 0xfb, }
};

}  // namespace

// Verify that an uninitialized UUID is equal
// To the BT SIG Base UUID.
TEST(UUIDTest, DefaultUUID) {
  UUID uuid;
  ASSERT_TRUE(uuid.is_valid());
  ASSERT_TRUE(uuid.GetFullBigEndian() == kBtSigBaseUUID);
}

// Verify that we initialize a 16-bit UUID in a
// way consistent with how we read it.
TEST(UUIDTest, Init16Bit) {
  auto my_uuid_16 = kBtSigBaseUUID;
  my_uuid_16[2] = 0xde;
  my_uuid_16[3] = 0xad;
  UUID uuid(UUID::UUID16Bit({{ 0xde, 0xad }}));
  ASSERT_TRUE(uuid.is_valid());
  ASSERT_TRUE(uuid.GetFullBigEndian() == my_uuid_16);
  ASSERT_TRUE(UUID::kNumBytes16 == uuid.GetShortestRepresentationSize());
}

// Verify that we initialize a 16-bit UUID in a
// way consistent with how we read it.
TEST(UUIDTest, Init16BitString) {
  auto my_uuid_16 = kBtSigBaseUUID;
  my_uuid_16[2] = 0xde;
  my_uuid_16[3] = 0xad;
  UUID uuid("dead");
  ASSERT_TRUE(uuid.is_valid());
  ASSERT_TRUE(uuid.GetFullBigEndian() == my_uuid_16);
  ASSERT_TRUE(UUID::kNumBytes16 == uuid.GetShortestRepresentationSize());

  uuid = UUID("0xdead");
  ASSERT_TRUE(uuid.is_valid());
  ASSERT_TRUE(uuid.GetFullBigEndian() == my_uuid_16);
  ASSERT_TRUE(UUID::kNumBytes16 == uuid.GetShortestRepresentationSize());
}


// Verify that we initialize a 32-bit UUID in a
// way consistent with how we read it.
TEST(UUIDTest, Init32Bit) {
  auto my_uuid_32 = kBtSigBaseUUID;
  my_uuid_32[0] = 0xde;
  my_uuid_32[1] = 0xad;
  my_uuid_32[2] = 0xbe;
  my_uuid_32[3] = 0xef;
  UUID uuid(UUID::UUID32Bit({{ 0xde, 0xad, 0xbe, 0xef }}));
  ASSERT_TRUE(uuid.is_valid());
  ASSERT_TRUE(uuid.GetFullBigEndian() == my_uuid_32);
  ASSERT_TRUE(UUID::kNumBytes32 == uuid.GetShortestRepresentationSize());
}

// Verify correct reading of a 32-bit UUID initialized from string.
TEST(UUIDTest, Init32BitString) {
  auto my_uuid_32 = kBtSigBaseUUID;
  my_uuid_32[0] = 0xde;
  my_uuid_32[1] = 0xad;
  my_uuid_32[2] = 0xbe;
  my_uuid_32[3] = 0xef;
  UUID uuid("deadbeef");
  ASSERT_TRUE(uuid.is_valid());
  ASSERT_TRUE(uuid.GetFullBigEndian() == my_uuid_32);
  ASSERT_TRUE(UUID::kNumBytes32 == uuid.GetShortestRepresentationSize());
}

// Verify that we initialize a 128-bit UUID in a
// way consistent with how we read it.
TEST(UUIDTest, Init128Bit) {
  auto my_uuid_128 = kBtSigBaseUUID;
  for (int i = 0; i < static_cast<int>(my_uuid_128.size()); ++i) {
    my_uuid_128[i] = i;
  }

  UUID uuid(my_uuid_128);
  ASSERT_TRUE(uuid.is_valid());
  ASSERT_TRUE(uuid.GetFullBigEndian() == my_uuid_128);
  ASSERT_TRUE(UUID::kNumBytes128 == uuid.GetShortestRepresentationSize());
}

// Verify that we initialize a 128-bit UUID in a
// way consistent with how we read it as LE.
TEST(UUIDTest, Init128BitLittleEndian) {
  auto my_uuid_128 = kBtSigBaseUUID;
  for (int i = 0; i < static_cast<int>(my_uuid_128.size()); ++i) {
    my_uuid_128[i] = i;
  }

  UUID uuid(my_uuid_128);
  std::reverse(my_uuid_128.begin(), my_uuid_128.end());
  ASSERT_TRUE(uuid.is_valid());
  ASSERT_TRUE(uuid.GetFullLittleEndian() == my_uuid_128);
}

// Verify that we initialize a 128-bit UUID in a
// way consistent with how we read it.
TEST(UUIDTest, Init128BitString) {
  UUID::UUID128Bit my_uuid{
    { 7, 1, 6, 8, 14, 255, 16, 2, 3, 4, 5, 6, 7, 8, 9, 10 }
  };
  std::string my_uuid_string("07010608-0eff-1002-0304-05060708090a");

  UUID uuid0(my_uuid);
  UUID uuid1(my_uuid_string);

  ASSERT_TRUE(uuid0.is_valid());
  ASSERT_TRUE(uuid1.is_valid());
  ASSERT_TRUE(uuid0 == uuid1);
  ASSERT_TRUE(UUID::kNumBytes128 == uuid0.GetShortestRepresentationSize());
}

TEST(UUIDTest, InitInvalid) {
  UUID uuid0("000102030405060708090A0B0C0D0E0F");
  ASSERT_FALSE(uuid0.is_valid());

  UUID uuid1("1*90");
  ASSERT_FALSE(uuid1.is_valid());

  UUID uuid2("109g");
  ASSERT_FALSE(uuid1.is_valid());
}

TEST(UUIDTest, ToString) {
  const UUID::UUID16Bit data{{ 0x18, 0x0d }};
  UUID uuid(data);
  std::string uuid_string = uuid.ToString();
  EXPECT_EQ("0000180d-0000-1000-8000-00805f9b34fb", uuid_string);
}
