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
#include "environment.h"
#include "record.h"
#include "record_file.h"

using namespace PerfFileFormat;

class RecordCommandTest : public ::testing::Test {
 protected:
  virtual void SetUp() {
    record_cmd = Command::FindCommandByName("record");
    ASSERT_TRUE(record_cmd != nullptr);
  }

  Command* record_cmd;
};

TEST_F(RecordCommandTest, no_options) {
  ASSERT_TRUE(record_cmd->Run({"record", "sleep", "1"}));
}

TEST_F(RecordCommandTest, system_wide_option) {
  ASSERT_TRUE(record_cmd->Run({"record", "-a", "sleep", "1"}));
}

TEST_F(RecordCommandTest, sample_period_option) {
  ASSERT_TRUE(record_cmd->Run({"record", "-c", "100000", "sleep", "1"}));
}

TEST_F(RecordCommandTest, event_option) {
  ASSERT_TRUE(record_cmd->Run({"record", "-e", "cpu-clock", "sleep", "1"}));
}

TEST_F(RecordCommandTest, freq_option) {
  ASSERT_TRUE(record_cmd->Run({"record", "-f", "99", "sleep", "1"}));
  ASSERT_TRUE(record_cmd->Run({"record", "-F", "99", "sleep", "1"}));
}

TEST_F(RecordCommandTest, output_file_option) {
  ASSERT_TRUE(record_cmd->Run({"record", "-o", "perf2.data", "sleep", "1"}));
}

TEST_F(RecordCommandTest, dump_kernel_mmap) {
  ASSERT_TRUE(record_cmd->Run({"record", "sleep", "1"}));
  std::unique_ptr<RecordFileReader> reader = RecordFileReader::CreateInstance("perf.data");
  ASSERT_TRUE(reader != nullptr);
  std::vector<std::unique_ptr<const Record>> records = reader->DataSection();
  ASSERT_GT(records.size(), 0U);
  bool have_kernel_mmap = false;
  for (auto& record : records) {
    if (record->header.type == PERF_RECORD_MMAP) {
      const MmapRecord* mmap_record = static_cast<const MmapRecord*>(record.get());
      if (mmap_record->filename == DEFAULT_KERNEL_MMAP_NAME) {
        have_kernel_mmap = true;
        break;
      }
    }
  }
  ASSERT_TRUE(have_kernel_mmap);
}

TEST_F(RecordCommandTest, dump_build_id_feature) {
  ASSERT_TRUE(record_cmd->Run({"record", "sleep", "1"}));
  std::unique_ptr<RecordFileReader> reader = RecordFileReader::CreateInstance("perf.data");
  ASSERT_TRUE(reader != nullptr);
  const FileHeader* file_header = reader->FileHeader();
  ASSERT_TRUE(file_header != nullptr);
  ASSERT_TRUE(file_header->features[FEAT_BUILD_ID / 8] & (1 << (FEAT_BUILD_ID % 8)));
  ASSERT_GT(reader->FeatureSectionDescriptors().size(), 0u);
}
