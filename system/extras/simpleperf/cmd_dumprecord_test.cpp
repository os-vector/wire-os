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

class DumpRecordCommandTest : public ::testing::Test {
 protected:
  virtual void SetUp() {
    record_cmd = Command::FindCommandByName("record");
    ASSERT_TRUE(record_cmd != nullptr);
    dumprecord_cmd = Command::FindCommandByName("dump");
    ASSERT_TRUE(dumprecord_cmd != nullptr);
  }

  Command* record_cmd;
  Command* dumprecord_cmd;
};

TEST_F(DumpRecordCommandTest, no_options) {
  ASSERT_TRUE(record_cmd->Run({"record", "-a", "sleep", "1"}));
  ASSERT_TRUE(dumprecord_cmd->Run({"dump"}));
}

TEST_F(DumpRecordCommandTest, record_file_option) {
  ASSERT_TRUE(record_cmd->Run({"record", "-a", "-o", "perf2.data", "sleep", "1"}));
  ASSERT_TRUE(dumprecord_cmd->Run({"dump", "perf2.data"}));
}
