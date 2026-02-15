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

#include "service/common/bluetooth/gatt_identifier.h"
#include "service/common/bluetooth/uuid.h"

namespace bluetooth {
namespace {

const std::string kAddr0 = "00:01:02:03:04:05";
const std::string kAddr1 = "06:07:08:08:0a:0b";

const UUID kUUID0;
const UUID kUUID1("180d");

const int kId0 = 0;
const int kId1 = 1;

TEST(GattIdentifierTest, ServiceId) {
  auto service0 = GattIdentifier::CreateServiceId(kAddr0, kId0, kUUID0, true);

  EXPECT_TRUE(service0->IsService());
  EXPECT_FALSE(service0->IsCharacteristic());
  EXPECT_FALSE(service0->IsDescriptor());

  EXPECT_FALSE(service0->GetOwningServiceId());
  EXPECT_FALSE(service0->GetOwningCharacteristicId());

  // Create different variants, swapping one entry at a time.
  auto service1 = GattIdentifier::CreateServiceId(kAddr1, kId0, kUUID0, true);
  auto service2 = GattIdentifier::CreateServiceId(kAddr0, kId1, kUUID0, true);
  auto service3 = GattIdentifier::CreateServiceId(kAddr0, kId0, kUUID1, true);
  auto service4 = GattIdentifier::CreateServiceId(kAddr0, kId0, kUUID0, false);

  EXPECT_TRUE(*service1 != *service0);
  EXPECT_TRUE(*service2 != *service0);
  EXPECT_TRUE(*service3 != *service0);
  EXPECT_TRUE(*service4 != *service0);

  GattIdentifier service_copy = *service0;
  EXPECT_TRUE(service_copy == *service0);
}

TEST(GattIdentifierTest, CharacteristicId) {
  auto service0 = GattIdentifier::CreateServiceId(kAddr0, kId0, kUUID0, true);
  auto char0 = GattIdentifier::CreateCharacteristicId(kId1, kUUID1, *service0);

  EXPECT_FALSE(char0->IsService());
  EXPECT_TRUE(char0->IsCharacteristic());
  EXPECT_FALSE(char0->IsDescriptor());

  EXPECT_FALSE(char0->GetOwningCharacteristicId());
  EXPECT_TRUE(*char0->GetOwningServiceId() == *service0);

  auto service1 = GattIdentifier::CreateServiceId(kAddr1, kId0, kUUID0, true);

  auto char1 = GattIdentifier::CreateCharacteristicId(kId0, kUUID1, *service0);
  auto char2 = GattIdentifier::CreateCharacteristicId(kId1, kUUID0, *service0);
  auto char3 = GattIdentifier::CreateCharacteristicId(kId1, kUUID1, *service1);

  EXPECT_TRUE(*char1 != *char0);
  EXPECT_TRUE(*char2 != *char0);
  EXPECT_TRUE(*char3 != *char0);

  GattIdentifier char_copy = *char0;
  EXPECT_TRUE(char_copy == *char0);

  EXPECT_TRUE(*service0 != *char0);
}

TEST(GattIdentifierTest, DescriptorId) {
  auto service0 = GattIdentifier::CreateServiceId(kAddr0, kId0, kUUID0, true);
  auto char0 = GattIdentifier::CreateCharacteristicId(kId1, kUUID1, *service0);
  auto desc0 = GattIdentifier::CreateDescriptorId(kId0, kUUID0, *char0);

  EXPECT_FALSE(desc0->IsService());
  EXPECT_FALSE(desc0->IsCharacteristic());
  EXPECT_TRUE(desc0->IsDescriptor());

  EXPECT_TRUE(*desc0->GetOwningCharacteristicId() == *char0);
  EXPECT_TRUE(*desc0->GetOwningServiceId() == *service0);

  auto char1 = GattIdentifier::CreateCharacteristicId(kId0, kUUID1, *service0);

  auto desc1 = GattIdentifier::CreateDescriptorId(kId1, kUUID0, *char0);
  auto desc2 = GattIdentifier::CreateDescriptorId(kId0, kUUID1, *char0);
  auto desc3 = GattIdentifier::CreateDescriptorId(kId0, kUUID0, *char1);

  EXPECT_TRUE(*desc1 != *desc0);
  EXPECT_TRUE(*desc2 != *desc0);
  EXPECT_TRUE(*desc3 != *desc0);

  GattIdentifier desc_copy = *desc0;
  EXPECT_TRUE(desc_copy == *desc0);

  EXPECT_TRUE(*service0 != *char0);
  EXPECT_TRUE(*service0 != *desc0);
  EXPECT_TRUE(*char0 != *desc0);
}

}  // namespace
}  // namespace bluetooth
