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

class MockDaemon : public Daemon {
 public:
  MockDaemon() = default;
  ~MockDaemon() override = default;

  MOCK_CONST_METHOD0(GetSettings, Settings*());
  MOCK_CONST_METHOD0(GetMessageLoop, base::MessageLoop*());
  MOCK_METHOD0(StartMainLoop, void());
  MOCK_METHOD0(Init, bool());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockDaemon);
};

}  // namespace testing
}  // namespace bluetooth
