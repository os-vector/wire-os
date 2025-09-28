// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef _BSDIFF_TEST_UTILS_H_
#define _BSDIFF_TEST_UTILS_H_

#include <gtest/gtest.h>
#include <cstdint>
#include <string>
#include <cstdint>
#include <vector>
#include <cstdint>

#define TEST_AND_RETURN_FALSE(_x)         \
  do {                                    \
    if (!static_cast<bool>(_x)) {         \
      fprintf(stderr, "%s failed.", #_x); \
      return false;                       \
    }                                     \
  } while (0)

namespace test_utils {

class BsdiffTestEnvironment : public ::testing::Environment {
  public:
    virtual void SetUp();
};

// Reads all the contents of the file |path| into |out|. Returns whether it
// read up to the end of file.
bool ReadFile(const std::string& path, std::vector<uint8_t>* out);

// Overrides the file |path| with the contents passed in |out|. Returns whether
// the operation succeeded.
bool WriteFile(const std::string& path, std::vector<uint8_t> contents);

// Utility class to create and delete a temp file.
class ScopedTempFile {
 public:
  // Creates a temp file with the passed |pattern|. The pattern should end with
  // "XXXXXX", that will be replaced with a random string. The file will be
  // removed when this instance is destroyed.
  explicit ScopedTempFile(const std::string& pattern);
  ~ScopedTempFile();

  std::string filename() const { return filename_; }
  const char* c_str() const { return filename_.c_str(); }

  // Releases the temporary file. It will not be deleted when this instance is
  // destroyed.
  void release() { filename_.clear(); }

 private:
  std::string filename_;
};

// This struct representes a parsed BSDIFF40 file.
struct BsdiffPatchFile {
  static const size_t kHeaderSize = 32;

  // Parses a BSDIFF40 file and stores the contents in the local methods.
  bool LoadFromFile(const std::string& filename);

  // Returns wheter the patch file is valid.
  bool IsValid() const;

  // The magic string in the header file. Normally "BSDIFF40".
  std::string magic;

  // The length of the first (ctrl) bzip2 stream. Negative values are invalid.
  int64_t ctrl_len = -1;

  // The length of the first (diff) bzip2 stream. Negative values are invalid.
  int64_t diff_len = -1;

  // The length of the first (diff) bzip2 stream. This value is not stored in
  // the file, but generated based on the |file_size|.
  uint64_t extra_len = 0;

  // The length of the new file after applying the patch. Negative values are
  // invalid.
  int64_t new_file_len = -1;

  // The three compressed streams.
  std::vector<uint8_t> bz2_ctrl;
  std::vector<uint8_t> bz2_diff;
  std::vector<uint8_t> bz2_extra;

  uint64_t file_size = 0;
};


}  // namespace test_utils


#endif  // _BSDIFF_TEST_UTILS_H_
