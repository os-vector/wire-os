/*
 * Copyright (C) 2015 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <gtest/gtest.h>

#include "command.h"

class MockCommand : public Command {
 public:
  MockCommand(const std::string& name) : Command(name, name + "_short_help", name + "_long_help") {
  }

  bool Run(const std::vector<std::string>&) override {
    return true;
  }
};

TEST(command, FindCommandByName) {
  ASSERT_EQ(Command::FindCommandByName("mock1"), nullptr);
  {
    MockCommand mock1("mock1");
    ASSERT_EQ(Command::FindCommandByName("mock1"), &mock1);
  }
  ASSERT_EQ(Command::FindCommandByName("mock1"), nullptr);
}

TEST(command, GetAllCommands) {
  size_t command_count = Command::GetAllCommands().size();
  {
    MockCommand mock1("mock1");
    ASSERT_EQ(command_count + 1, Command::GetAllCommands().size());
  }
  ASSERT_EQ(command_count, Command::GetAllCommands().size());
}
