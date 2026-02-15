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

#include <arpa/inet.h>
#include <gtest/gtest.h>
#include "osi/test/AllocationTestHarness.h"

extern "C" {
#include "btcore/include/device_class.h"
}  // "C"

// Device Class is 3 bytes.
static const int DC_MASK = 0xffffff;

::testing::AssertionResult check_bitfield(const char *m_expr,
    const char *n_expr, int m, int n) {
  if (m == n)
    return ::testing::AssertionSuccess();

  std::stringstream ss;

  ss.str("");
  ss << std::showbase << std::hex << std::setw(8) << std::setfill('0') << m;
  std::string expected_str = ss.str();

  ss.str("");
  ss << std::showbase << std::hex << std::setw(8) << std::setfill('0') << n;
  std::string actual_str = ss.str();

  return ::testing::AssertionFailure() << m_expr << " and " << n_expr
    << " ( " << expected_str << " vs " << actual_str << " )";
}

class DeviceClassTest : public AllocationTestHarness {};

TEST_F(DeviceClassTest, cod_sizeof) {
  uint8_t dc_stream[] = { 0x00, 0x00, 0x00, 0x00};
  bt_device_class_t dc0;
  device_class_from_stream(&dc0, dc_stream);
  EXPECT_EQ((size_t)3, sizeof(dc0));
}

TEST_F(DeviceClassTest, simple) {
  uint8_t dc_stream[][sizeof(bt_device_class_t)] = {
    { 0x00, 0x00, 0x00 },
    { 0xff, 0xff, 0xff },
    { 0xaa, 0x55, 0xaa },
    { 0x01, 0x23, 0x45 },
    { 0x20, 0x07, 0x14 },
  };

  for (size_t i = 0; i < sizeof(dc_stream)/sizeof(bt_device_class_t); i++) {
    bt_device_class_t dc;
    device_class_from_stream(&dc, (uint8_t*)&dc_stream[i]);

    uint8_t *to_stream = (uint8_t *)&dc;
    EXPECT_PRED_FORMAT2(check_bitfield, (unsigned)dc_stream[i][0], to_stream[0]);
    EXPECT_PRED_FORMAT2(check_bitfield, (unsigned)dc_stream[i][1], to_stream[1]);
    EXPECT_PRED_FORMAT2(check_bitfield, (unsigned)dc_stream[i][2], to_stream[2]);
  }
}

TEST_F(DeviceClassTest, to_stream) {
  {
    bt_device_class_t dc;

    uint8_t dc_stream0[] = { 0x00, 0x00, 0x00, 0xaa };
    device_class_from_stream(&dc, dc_stream0);

    uint8_t dc_stream1[] = { 0x00, 0x00, 0x00, 0x00 };
    int rc = device_class_to_stream(&dc, dc_stream1, sizeof(dc_stream1));
    EXPECT_EQ(3, rc);

    uint32_t *val = (uint32_t *)&dc;
    EXPECT_PRED_FORMAT2(check_bitfield, 0x00000000, *val & 0xffffff);

    EXPECT_PRED_FORMAT2(check_bitfield, 0x00, dc_stream1[0]);
    EXPECT_PRED_FORMAT2(check_bitfield, 0x00, dc_stream1[1]);
    EXPECT_PRED_FORMAT2(check_bitfield, 0x00, dc_stream1[2]);
  }

  {
    uint8_t dc_stream0[] = { 0xaa, 0x55, 0xaa, 0x55 };
    uint8_t dc_stream1[] = { 0x00, 0x00, 0x00, 0x00 };

    bt_device_class_t dc;
    device_class_from_stream(&dc, dc_stream0);

    int rc = device_class_to_stream(&dc, dc_stream1, sizeof(dc_stream1));
    EXPECT_EQ(3, rc);
    uint32_t *val = (uint32_t *)&dc;
    EXPECT_PRED_FORMAT2(check_bitfield, 0x00aa55aa, *val & 0xffffff);

    EXPECT_PRED_FORMAT2(check_bitfield, 0xaa, dc_stream1[0]);
    EXPECT_PRED_FORMAT2(check_bitfield, 0x55, dc_stream1[1]);
    EXPECT_PRED_FORMAT2(check_bitfield, 0xaa, dc_stream1[2]);
  }

  {
    uint8_t dc_stream0[] = { 0x01, 0x23, 0x45, 0x67 };
    uint8_t dc_stream1[] = { 0x00, 0x00, 0x00, 0x00 };

    bt_device_class_t dc;
    device_class_from_stream(&dc, dc_stream0);

    int rc = device_class_to_stream(&dc, dc_stream1, sizeof(dc_stream1));
    EXPECT_EQ(3, rc);
    uint32_t *val = (uint32_t *)&dc;
    EXPECT_PRED_FORMAT2(check_bitfield, 0x452301, *val & 0xffffff);

    EXPECT_PRED_FORMAT2(check_bitfield, 0x01, dc_stream1[0]);
    EXPECT_PRED_FORMAT2(check_bitfield, 0x23, dc_stream1[1]);
    EXPECT_PRED_FORMAT2(check_bitfield, 0x45, dc_stream1[2]);
  }
}

TEST_F(DeviceClassTest, limited_discoverable_mode) {
  uint8_t dc_stream[] = { 0x00, 0x00, 0x00 };
  bt_device_class_t dc;
  device_class_from_stream(&dc, dc_stream);
  uint32_t *test = (uint32_t *)&dc;

  EXPECT_FALSE(device_class_get_limited(&dc));
  EXPECT_EQ((unsigned)0x00000000, *test & DC_MASK);

  device_class_set_limited(&dc, true);
  EXPECT_TRUE(device_class_get_limited(&dc));
  EXPECT_EQ((unsigned)0x00002000, *test & DC_MASK);

  device_class_set_limited(&dc, false);
  EXPECT_FALSE(device_class_get_limited(&dc));
  EXPECT_EQ((unsigned)0x00000000, *test & DC_MASK);

  device_class_set_limited(&dc, true);
  EXPECT_PRED_FORMAT2(check_bitfield, 0x00002000, *test & DC_MASK);

  device_class_set_limited(&dc, false);
  EXPECT_PRED_FORMAT2(check_bitfield, 0x00000000, *test & DC_MASK);
}

TEST_F(DeviceClassTest, equals) {
  uint8_t dc_stream0[] = { 0x00, 0x01, 0x02 };
  uint8_t dc_stream1[] = { 0x00, 0x02, 0x03 };

  bt_device_class_t dc0;
  device_class_from_stream(&dc0, dc_stream0);
  bt_device_class_t dc1;
  device_class_from_stream(&dc1, dc_stream1);
  EXPECT_FALSE(device_class_equals(&dc0, &dc1));
}

TEST_F(DeviceClassTest, copy) {
  uint8_t dc_stream0[] = { 0xaa, 0x55, 0x33 };
  bt_device_class_t dc0;
  device_class_from_stream(&dc0, dc_stream0);
  bt_device_class_t dc1;
  EXPECT_TRUE(device_class_copy(&dc1, &dc0));
  EXPECT_TRUE(device_class_equals(&dc0, &dc1));
}

TEST_F(DeviceClassTest, from_int) {
  bt_device_class_t dc1;
  int cod1 = 0x5a020c;  // 5898764
  device_class_from_int(&dc1, cod1);

  uint8_t dc_stream[] = { 0x0c, 0x02, 0x5a };
  bt_device_class_t dc2;
  device_class_from_stream(&dc2, dc_stream);
  EXPECT_TRUE(device_class_equals(&dc1, &dc2));
}

TEST_F(DeviceClassTest, to_int) {
  bt_device_class_t dc1 = {{ 0x0c, 0x02, 0x5a }};
  int cod1 = device_class_to_int(&dc1);

  EXPECT_EQ(dc1._[0], 0x0c);
  EXPECT_EQ(dc1._[1], 0x02);
  EXPECT_EQ(dc1._[2], 0x5a);

  bt_device_class_t dc2;
  uint8_t dc_stream[] = { 0x0c, 0x02, 0x5a };
  device_class_from_stream(&dc2, dc_stream);

  EXPECT_EQ(dc2._[0], 0x0c);
  EXPECT_EQ(dc2._[1], 0x02);
  EXPECT_EQ(dc2._[2], 0x5a);

  int cod2 = device_class_to_int(&dc2);
  EXPECT_EQ(cod1, cod2);
  EXPECT_EQ(cod1, 0x5a020c);  // 5898764
}

TEST_F(DeviceClassTest, endian) {
  bt_device_class_t dc;
  int cod1 = 0x200714;  // 2098964
  device_class_from_int(&dc, cod1);

  EXPECT_EQ(dc._[0], 0x14);
  EXPECT_EQ(dc._[1], 0x07);
  EXPECT_EQ(dc._[2], 0x20);

  int cod2 = device_class_to_int(&dc);
  EXPECT_EQ(cod1, cod2);
  EXPECT_EQ(cod2, 0x200714); // 2098964
}
