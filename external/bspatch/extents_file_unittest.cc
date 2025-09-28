// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bsdiff/extents_file.h"
#include <cstdint>

#include <gtest/gtest.h>
#include <cstdint>
#include <gmock/gmock.h>
#include <cstdint>
#include <string>
#include <cstdint>
#include <vector>
#include <cstdint>

#include "bsdiff/file_interface.h"
#include <cstdint>

using std::string;
using std::vector;
using testing::AnyNumber;
using testing::StrictMock;
using testing::Return;
using testing::InSequence;
using testing::_;

namespace bsdiff {

// Mock class for the underlying file interface.
class MockFile : public FileInterface {
 public:
  MOCK_METHOD3(Read, bool(void*, size_t, size_t*));
  MOCK_METHOD3(Write, bool(const void*, size_t, size_t*));
  MOCK_METHOD1(Seek, bool(off_t));
  MOCK_METHOD0(Close, bool());
  MOCK_METHOD1(GetSize, bool(uint64_t*));
};

ACTION(SucceedIO) {
  // Check that arg1 (count) can be converted
  *arg2 = arg1;
  return true;
}

ACTION_P(SucceedPartialIO, bytes) {
  // Check that arg1 (count) can be converted
  *arg2 = bytes;
  return true;
}

class ExtentsFileTest : public testing::Test {
 protected:
  void SetUp() {
    mock_file_ = new StrictMock<MockFile>();
    mock_file_ptr_.reset(mock_file_);
    // The destructor of the ExtentsFile will call Close once.
    EXPECT_CALL(*mock_file_, Close()).WillOnce(Return(true));
  }

  // Pointer to the underlying File owned by the ExtentsFile under test. This
  // pointer is invalidated whenever the ExtentsFile is destroyed.
  StrictMock<MockFile>* mock_file_;
  std::unique_ptr<FileInterface> mock_file_ptr_;
};

TEST_F(ExtentsFileTest, DestructorCloses) {
  ExtentsFile file(std::move(mock_file_ptr_), {});
}

TEST_F(ExtentsFileTest, CloseIsForwarded) {
  ExtentsFile file(std::move(mock_file_ptr_), {});
  EXPECT_TRUE(file.Close());
  EXPECT_CALL(*mock_file_, Close()).WillOnce(Return(false));
}

TEST_F(ExtentsFileTest, GetSizeSumExtents) {
  ExtentsFile file(std::move(mock_file_ptr_),
                   {ex_t{10, 5}, ex_t{20, 5}, {25, 2}});
  uint64_t size;
  EXPECT_TRUE(file.GetSize(&size));
  EXPECT_EQ(12U, size);
}

TEST_F(ExtentsFileTest, SeekToRightOffsets) {
  ExtentsFile file(std::move(mock_file_ptr_),
                   {ex_t{10, 5}, ex_t{20, 5}, {25, 2}});
  vector<std::pair<off_t, off_t>> tests = {
      // Seek to the beginning of the file.
      {0, 10},
      // Seek to the middle of a extent.
      {3, 13},
      {11, 26},
      // Seek to the extent boundary.
      {5, 20},  // Seeks to the first byte in the second extent.
      {10, 25},
  };
  for (const auto& offset_pair : tests) {
    // We use a failing Read() call to trigger the actual seek call to the
    // underlying file.
    EXPECT_CALL(*mock_file_, Seek(offset_pair.second)).WillOnce(Return(true));
    EXPECT_CALL(*mock_file_, Read(_, _, _)).WillOnce(Return(false));

    EXPECT_TRUE(file.Seek(offset_pair.first));
    size_t bytes_read;
    EXPECT_FALSE(file.Read(nullptr, 1, &bytes_read));
  }

  // Seeking to the end of the file is ok, but not past it.
  EXPECT_TRUE(file.Seek(12));
  EXPECT_FALSE(file.Seek(13));

  EXPECT_FALSE(file.Seek(-1));
}

TEST_F(ExtentsFileTest, ReadAcrossAllExtents) {
  ExtentsFile file(std::move(mock_file_ptr_),
                   {ex_t{10, 5}, ex_t{20, 7}, {27, 3}});
  InSequence s;
  char* buf = reinterpret_cast<char*>(0x1234);

  EXPECT_CALL(*mock_file_, Seek(10)).WillOnce(Return(true));
  EXPECT_CALL(*mock_file_, Read(buf, 5, _)).WillOnce(SucceedIO());
  EXPECT_CALL(*mock_file_, Seek(20)).WillOnce(Return(true));
  EXPECT_CALL(*mock_file_, Read(buf + 5, 7, _)).WillOnce(SucceedIO());
  EXPECT_CALL(*mock_file_, Seek(27)).WillOnce(Return(true));
  EXPECT_CALL(*mock_file_, Read(buf + 12, 3, _)).WillOnce(SucceedIO());

  // FileExtents::Read() should read everything in one shot, by reading all
  // the little chunks. Note that it doesn't attempt to read past the end of the
  // FileExtents.
  size_t bytes_read = 0;
  EXPECT_TRUE(file.Read(buf, 100, &bytes_read));
  EXPECT_EQ(15U, bytes_read);
}

TEST_F(ExtentsFileTest, MultiReadAcrossAllExtents) {
  ExtentsFile file(std::move(mock_file_ptr_),
                   {ex_t{10, 5}, ex_t{20, 7}, {27, 3}});
  InSequence s;
  char* buf = reinterpret_cast<char*>(0x1234);

  EXPECT_CALL(*mock_file_, Seek(10)).WillOnce(Return(true));
  EXPECT_CALL(*mock_file_, Read(buf, 2, _)).WillOnce(SucceedIO());
  EXPECT_CALL(*mock_file_, Seek(12)).WillOnce(Return(true));
  EXPECT_CALL(*mock_file_, Read(buf, 3, _)).WillOnce(SucceedIO());
  EXPECT_CALL(*mock_file_, Seek(20)).WillOnce(Return(true));
  EXPECT_CALL(*mock_file_, Read(buf + 3, 5, _)).WillOnce(SucceedIO());
  EXPECT_CALL(*mock_file_, Seek(25)).WillOnce(Return(true));
  EXPECT_CALL(*mock_file_, Read(buf, 2, _)).WillOnce(SucceedIO());
  EXPECT_CALL(*mock_file_, Seek(27)).WillOnce(Return(true));
  EXPECT_CALL(*mock_file_, Read(buf + 2, 3, _)).WillOnce(SucceedIO());

  size_t bytes_read = 0;
  EXPECT_TRUE(file.Read(buf, 2, &bytes_read));
  EXPECT_EQ(2U, bytes_read);
  EXPECT_TRUE(file.Read(buf, 8, &bytes_read));
  EXPECT_EQ(8U, bytes_read);
  EXPECT_TRUE(file.Read(buf, 100, &bytes_read));
  EXPECT_EQ(5U, bytes_read);
}

TEST_F(ExtentsFileTest, ReadSmallChunks) {
  ExtentsFile file(std::move(mock_file_ptr_), {ex_t{10, 1}, ex_t{20, 10}});
  InSequence s;
  char* buf = reinterpret_cast<char*>(0x1234);

  EXPECT_CALL(*mock_file_, Seek(10)).WillOnce(Return(true));
  EXPECT_CALL(*mock_file_, Read(buf, 1, _)).WillOnce(SucceedIO());
  EXPECT_CALL(*mock_file_, Seek(20)).WillOnce(Return(true));
  // We expect to read only part of the second extent.
  EXPECT_CALL(*mock_file_, Read(buf + 1, 1, _)).WillOnce(SucceedIO());

  size_t bytes_read = 0;
  EXPECT_TRUE(file.Read(buf, 2, &bytes_read));
  EXPECT_EQ(2U, bytes_read);
}

TEST_F(ExtentsFileTest, ReadFailureFails) {
  ExtentsFile file(std::move(mock_file_ptr_), {ex_t{10, 1}, ex_t{20, 10}});
  EXPECT_CALL(*mock_file_, Seek(_))
      .Times(AnyNumber())
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*mock_file_, Read(_, 1, _)).WillOnce(SucceedIO());
  // A second read that fails will succeed if there was partial data read.
  EXPECT_CALL(*mock_file_, Read(_, 10, _)).WillOnce(Return(false));

  size_t bytes_read = 0;
  EXPECT_TRUE(file.Read(nullptr, 100, &bytes_read));
  EXPECT_EQ(1U, bytes_read);
}

TEST_F(ExtentsFileTest, ReadFails) {
  ExtentsFile file(std::move(mock_file_ptr_), {ex_t{10, 1}, ex_t{20, 10}});
  EXPECT_CALL(*mock_file_, Seek(10)).WillOnce(Return(true));
  EXPECT_CALL(*mock_file_, Read(_, 1, _)).WillOnce(Return(false));
  size_t bytes_read;
  EXPECT_FALSE(file.Read(nullptr, 1, &bytes_read));
}

TEST_F(ExtentsFileTest, ReadPartialReadsAndEOF) {
  ExtentsFile file(std::move(mock_file_ptr_), {ex_t{10, 1}, ex_t{20, 10}});
  EXPECT_CALL(*mock_file_, Seek(_))
      .Times(AnyNumber())
      .WillRepeatedly(Return(true));
  char* buf = reinterpret_cast<char*>(0x1234);
  InSequence s;
  EXPECT_CALL(*mock_file_, Read(buf, 1, _)).WillOnce(SucceedIO());
  EXPECT_CALL(*mock_file_, Read(buf + 1, _, _)).WillOnce(SucceedPartialIO(3));
  EXPECT_CALL(*mock_file_, Read(buf + 4, _, _)).WillOnce(SucceedPartialIO(0));

  size_t bytes_read = 0;
  EXPECT_TRUE(file.Read(buf, 100, &bytes_read));
  EXPECT_EQ(4U, bytes_read);
}

}  // namespace bsdiff
