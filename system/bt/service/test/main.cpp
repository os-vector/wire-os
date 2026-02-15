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

#ifdef BT_LIBCHROME_NDEBUG
#define NDEBUG 1
#endif

#include <base/at_exit.h>
#include <base/command_line.h>
#include <base/logging.h>

#include <gtest/gtest.h>

GTEST_API_ int main(int argc, char **argv) {
  base::AtExitManager exit_manager;
  base::CommandLine::Init(argc, argv);
  logging::LoggingSettings log_settings;
  logging::InitLogging(log_settings);

  LOG(INFO) << "Running Bluetooth daemon unit tests.";
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
