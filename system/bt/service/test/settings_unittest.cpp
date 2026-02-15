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

#include <base/at_exit.h>
#include <base/command_line.h>
#include <base/macros.h>
#include <gtest/gtest.h>

#include "service/settings.h"
#include "service/switches.h"

using bluetooth::Settings;
using namespace bluetooth::switches;

namespace {

class SettingsTest : public ::testing::Test {
 public:
  SettingsTest() = default;

  void SetUp() override {
    base::CommandLine::Reset();
  }

  void TearDown() override {
    base::CommandLine::Reset();
  }

 protected:
  base::AtExitManager exit_manager_;
  Settings settings_;

 private:
  DISALLOW_COPY_AND_ASSIGN(SettingsTest);
};

TEST_F(SettingsTest, EmptyCommandLine) {
  const base::CommandLine::CharType* argv[] = { "program" };
  EXPECT_TRUE(base::CommandLine::Init(arraysize(argv), argv));
  EXPECT_TRUE(settings_.Init());
}

TEST_F(SettingsTest, UnexpectedSwitches1) {
  const base::CommandLine::CharType* argv[] = {
    "program", "--create-ipc-socket=foobar", "--foobarbaz"
  };
  EXPECT_TRUE(base::CommandLine::Init(arraysize(argv), argv));
  EXPECT_FALSE(settings_.Init());
}

TEST_F(SettingsTest, UnexpectedSwitches2) {
  const base::CommandLine::CharType* argv[] = {
    "program", "--foobarbaz"
  };
  EXPECT_TRUE(base::CommandLine::Init(arraysize(argv), argv));
  EXPECT_FALSE(settings_.Init());
}

TEST_F(SettingsTest, UnexpectedArguments1) {
  const base::CommandLine::CharType* argv[] = {
    "program", "foobarbaz"
  };
  EXPECT_TRUE(base::CommandLine::Init(arraysize(argv), argv));
  EXPECT_FALSE(settings_.Init());
}

TEST_F(SettingsTest, UnexpectedArguments2) {
  const base::CommandLine::CharType* argv[] = {
    "program", "--create-ipc-socket=foobar", "foobarbaz"
  };
  EXPECT_TRUE(base::CommandLine::Init(arraysize(argv), argv));
  EXPECT_FALSE(settings_.Init());
}

TEST_F(SettingsTest, TooManyIpcOptions) {
  const base::CommandLine::CharType* argv[] = {
      "program", "--create-ipc-socket=foobar",
      "--android-ipc-socket-suffix=foobar"};
  EXPECT_TRUE(base::CommandLine::Init(arraysize(argv), argv));
  EXPECT_FALSE(settings_.Init());
}

TEST_F(SettingsTest, GoodArgumentsCreateIpc) {
  const base::CommandLine::CharType* argv[] = {
    "program", "--create-ipc-socket=foobar"
  };
  EXPECT_TRUE(base::CommandLine::Init(arraysize(argv), argv));
  EXPECT_TRUE(settings_.Init());
}

TEST_F(SettingsTest, GoodArgumentsAndroidIpc) {
  const base::CommandLine::CharType* argv[] = {
    "program", "--android-ipc-socket-suffix=foobar"
  };
  EXPECT_TRUE(base::CommandLine::Init(arraysize(argv), argv));
  EXPECT_TRUE(settings_.Init());
}

}  // namespace
