// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bsdiff/bsdiff.h"
#include <cstdint>

#include <gtest/gtest.h>
#include <cstdint>
#include <string>
#include <cstdint>
#include <vector>
#include <cstdint>

#include "test_utils.h"
#include <cstdint>

using std::string;
using std::vector;
using test_utils::BsdiffPatchFile;

namespace bsdiff {

class BsdiffTest : public testing::Test {
 protected:
  BsdiffTest()
    : old_file_("bsdiff_oldfile.XXXXXX"),
      new_file_("bsdiff_newfile.XXXXXX"),
      patch_file_("bsdiff_patchfile.XXXXXX") {
  }
  ~BsdiffTest() override {}

  test_utils::ScopedTempFile old_file_;
  test_utils::ScopedTempFile new_file_;
  test_utils::ScopedTempFile patch_file_;
};

// Check that a file with no changes has a very small patch (no extra data).
TEST_F(BsdiffTest, EqualEmptyFiles) {
  // Empty old and new files.
  EXPECT_EQ(0, bsdiff(old_file_.filename().c_str(),
                      new_file_.filename().c_str(),
                      patch_file_.filename().c_str()));
  BsdiffPatchFile patch;
  EXPECT_TRUE(patch.LoadFromFile(patch_file_.filename()));
  EXPECT_TRUE(patch.IsValid());

  // An empty bz2 file will have 14 bytes.
  EXPECT_EQ(14, patch.diff_len);
  EXPECT_EQ(14U, patch.extra_len);
}

TEST_F(BsdiffTest, EqualSmallFiles) {
  string some_text = "Hello world!";
  vector<uint8_t> vec_some_text(some_text.begin(), some_text.end());
  test_utils::WriteFile(old_file_.filename(), vec_some_text);
  EXPECT_EQ(0, bsdiff(old_file_.filename().c_str(),
                      new_file_.filename().c_str(),
                      patch_file_.filename().c_str()));
  BsdiffPatchFile patch;
  EXPECT_TRUE(patch.LoadFromFile(patch_file_.filename()));
  EXPECT_TRUE(patch.IsValid());

  // An empty bz2 file will have 14 bytes.
  EXPECT_EQ(14, patch.diff_len);
  EXPECT_EQ(14U, patch.extra_len);
}

}  // namespace bsdiff
