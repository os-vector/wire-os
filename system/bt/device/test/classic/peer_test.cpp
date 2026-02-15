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
#include "btcore/include/bdaddr.h"
#include "btcore/include/module.h"
#include "device/include/classic/peer.h"

extern const module_t classic_peer_module;
}

class ClassicPeerTest : public AllocationTestHarness {
  protected:
    virtual void SetUp() {
      AllocationTestHarness::SetUp();

      module_management_start();
      module_init(&classic_peer_module);
    }

    virtual void TearDown() {
      module_clean_up(&classic_peer_module);
      module_management_stop();

      AllocationTestHarness::TearDown();
    }
};

TEST_F(ClassicPeerTest, test_basic_get) {
  bt_bdaddr_t test_address;
  string_to_bdaddr("12:34:56:78:9A:BC", &test_address);

  classic_peer_t *peer = classic_peer_by_address(&test_address);

  EXPECT_TRUE(peer != NULL);
  // The stored address should be a copy (different memory address)
  EXPECT_NE(&test_address, classic_peer_get_address(peer));
  EXPECT_TRUE(bdaddr_equals(&test_address, classic_peer_get_address(peer)));
}

TEST_F(ClassicPeerTest, test_multi_get_are_same) {
  bt_bdaddr_t test_address;
  string_to_bdaddr("12:34:56:78:9A:BC", &test_address);

  classic_peer_t *peer = classic_peer_by_address(&test_address);
  classic_peer_t *peer_again = classic_peer_by_address(&test_address);

  EXPECT_TRUE(peer != NULL);
  EXPECT_TRUE(peer_again != NULL);
  EXPECT_EQ(peer, peer_again);
}

TEST_F(ClassicPeerTest, test_multi_get_different) {
  bt_bdaddr_t test_address0;
  bt_bdaddr_t test_address1;
  string_to_bdaddr("12:34:56:78:9A:BC", &test_address0);
  string_to_bdaddr("42:42:42:42:42:42", &test_address1);

  classic_peer_t *peer0 = classic_peer_by_address(&test_address0);
  classic_peer_t *peer1 = classic_peer_by_address(&test_address1);

  EXPECT_TRUE(peer0 != NULL);
  EXPECT_TRUE(peer1 != NULL);
  EXPECT_TRUE(bdaddr_equals(&test_address0, classic_peer_get_address(peer0)));
  EXPECT_TRUE(bdaddr_equals(&test_address1, classic_peer_get_address(peer1)));
}

