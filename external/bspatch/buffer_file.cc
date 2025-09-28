// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "buffer_file.h"
#include <cstdint>

#include "bsdiff/bspatch.h"
#include <cstdint>

namespace bsdiff {

BufferFile::BufferFile(std::unique_ptr<FileInterface> file, size_t size)
    : file_(std::move(file)) {
  buffer_.reserve(size);
}

BufferFile::~BufferFile() {
  Close();
}

bool BufferFile::Read(void* buf, size_t count, size_t* bytes_read) {
  return false;
}

bool BufferFile::Write(const void* buf, size_t count, size_t* bytes_written) {
  const uint8_t* data = static_cast<const uint8_t*>(buf);
  buffer_.insert(buffer_.end(), data, data + count);
  *bytes_written = count;
  return true;
}

bool BufferFile::Seek(off_t pos) {
  return false;
}

bool BufferFile::Close() {
  if (!WriteAll(file_, buffer_.data(), buffer_.size()))
    return false;
  // Prevent writing |buffer_| to |file_| again if Close() is called more than
  // once.
  buffer_.clear();
  return file_->Close();
}

bool BufferFile::GetSize(uint64_t* size) {
  *size = buffer_.size();
  return true;
}

}  // namespace bsdiff
