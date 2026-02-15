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

#include "service/common/bluetooth/advertise_data.h"
#include "stack/include/bt_types.h"
#include "stack/include/hcidefs.h"

namespace bluetooth {

TEST(AdvertiseDataTest, EmptyData) {
  const std::vector<uint8_t> data0;
  AdvertiseData adv0(data0);
  EXPECT_TRUE(adv0.IsValid());

  // Single empty field not allowed.
  const std::vector<uint8_t> data1{ 0x00 };
  AdvertiseData adv1(data1);
  EXPECT_FALSE(adv1.IsValid());
}

TEST(AdvertiseDataTest, BadTLV) {
  // Single field, field empty.
  const std::vector<uint8_t> data0{ 0x01 };
  AdvertiseData adv0(data0);
  EXPECT_FALSE(adv0.IsValid());

  // Single field, first field length too long.
  const std::vector<uint8_t> data1{ 0x05, 0x02, 0x00, 0x00, 0x00 };
  AdvertiseData adv1(data1);
  EXPECT_FALSE(adv1.IsValid());

  // Two fields, second field length too long.
  const std::vector<uint8_t> data2{ 0x02, 0x02, 0x00, 0x02, 0x00 };
  AdvertiseData adv2(data2);
  EXPECT_FALSE(adv2.IsValid());

  // Two fields, second field empty.
  const std::vector<uint8_t> data3{ 0x02, 0x02, 0x00, 0x01 };
  AdvertiseData adv3(data3);
  EXPECT_FALSE(adv3.IsValid());
}

TEST(AdvertiseDataTest, GoodTLV) {
  // Singe field.
  const std::vector<uint8_t> data0{ 0x03, 0x02, 0x01, 0x02 };
  AdvertiseData adv0(data0);
  EXPECT_TRUE(adv0.IsValid());

  // Twi fields.
  const std::vector<uint8_t> data1{ 0x03, 0x02, 0x01, 0x02, 0x02, 0x03, 0x01 };
  AdvertiseData adv1(data1);
  EXPECT_TRUE(adv0.IsValid());
}

TEST(AdvertiseDataTest, DisallowedFields) {
  // Singe field.
  const std::vector<uint8_t> data0{ 0x02, HCI_EIR_FLAGS_TYPE, 0x00 };
  AdvertiseData adv0(data0);
  EXPECT_FALSE(adv0.IsValid());

  // Two fields, first invalid.
  const std::vector<uint8_t> data1{
      0x02, HCI_EIR_FLAGS_TYPE, 0x00,
      0x03, 0x02, 0x01, 0x02
  };
  AdvertiseData adv1(data1);
  EXPECT_FALSE(adv1.IsValid());

  // Two fields, second invalid.
  const std::vector<uint8_t> data2{
      0x03, 0x02, 0x01, 0x02,
      0x02, HCI_EIR_FLAGS_TYPE, 0x00
  };
  AdvertiseData adv2(data2);
  EXPECT_FALSE(adv2.IsValid());

  // Check all blacklisted fields
  uint8_t blacklist[] = {
      HCI_EIR_FLAGS_TYPE,
      HCI_EIR_TX_POWER_LEVEL_TYPE,
      HCI_EIR_SHORTENED_LOCAL_NAME_TYPE,
      HCI_EIR_COMPLETE_LOCAL_NAME_TYPE,
      HCI_EIR_OOB_BD_ADDR_TYPE,
      HCI_EIR_OOB_COD_TYPE,
      HCI_EIR_OOB_SSP_HASH_C_TYPE,
      HCI_EIR_OOB_SSP_RAND_R_TYPE
  };
  for (size_t i = 0; i < sizeof(blacklist); i++) {
    const std::vector<uint8_t> data{ 0x02, blacklist[i], 0x00 };
    AdvertiseData adv(data);
    EXPECT_FALSE(adv.IsValid());
  }
}

TEST(AdvertiseDataTest, EqualsData) {
  const std::vector<uint8_t> data0{ 0x02, 0x02, 0x00 };
  const std::vector<uint8_t> data1{ 0x02, 0x03, 0x00 };

  AdvertiseData adv0(data0);
  AdvertiseData adv1(data1);

  EXPECT_FALSE(adv0 == adv1);

  AdvertiseData adv2(data1);
  EXPECT_TRUE(adv1 == adv2);
}

TEST(AdvertiseDataTest, EqualsIncludes) {
  const std::vector<uint8_t> data;

  AdvertiseData adv0;
  AdvertiseData adv1;
  AdvertiseData adv2;
  AdvertiseData adv3;
  AdvertiseData adv4;

  adv0.set_include_device_name(true);
  adv0.set_include_tx_power_level(false);

  adv1.set_include_device_name(false);
  adv1.set_include_tx_power_level(false);

  adv2.set_include_device_name(false);
  adv2.set_include_tx_power_level(true);

  adv3.set_include_device_name(true);
  adv3.set_include_tx_power_level(true);

  adv4.set_include_device_name(true);
  adv4.set_include_tx_power_level(false);

  EXPECT_FALSE(adv0 == adv1);
  EXPECT_FALSE(adv0 == adv2);
  EXPECT_FALSE(adv3 == adv1);
  EXPECT_TRUE(adv4 == adv0);
}

}  // namespace bluetooth
