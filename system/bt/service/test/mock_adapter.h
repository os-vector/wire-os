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

#pragma once

#include <gmock/gmock.h>

#include "service/daemon.h"

namespace bluetooth {
namespace testing {

class MockAdapter : public Adapter {
 public:
  MockAdapter() = default;
  ~MockAdapter() override = default;

  MOCK_METHOD1(AddObserver, void(Observer*));
  MOCK_METHOD1(RemoveObserver, void(Observer*));
  MOCK_CONST_METHOD0(GetState, AdapterState());
  MOCK_CONST_METHOD0(IsEnabled, bool());
  MOCK_METHOD1(Enable, bool(bool));
  MOCK_METHOD0(Disable, bool());
  MOCK_CONST_METHOD0(GetName, std::string());
  MOCK_METHOD1(SetName, bool(const std::string&));
  MOCK_CONST_METHOD0(GetAddress, std::string());
  MOCK_METHOD0(IsMultiAdvertisementSupported, bool());
  MOCK_METHOD1(IsDeviceConnected, bool(const std::string&));
  MOCK_METHOD0(GetTotalNumberOfTrackableAdvertisements, int());
  MOCK_METHOD0(IsOffloadedFilteringSupported, bool());
  MOCK_METHOD0(IsOffloadedScanBatchingSupported, bool());
  MOCK_CONST_METHOD0(GetLowEnergyClientFactory, LowEnergyClientFactory*());
  MOCK_CONST_METHOD0(GetGattClientFactory, GattClientFactory*());
  MOCK_CONST_METHOD0(GetGattServerFactory, GattServerFactory*());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockAdapter);
};

}  // namespace testing
}  // namespace bluetooth
