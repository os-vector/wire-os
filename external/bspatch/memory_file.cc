// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "memory_file.h"
#include <cstdint>

#include <algorithm>
#include <cstdint>
#include <string.h>
#include <cstdint>

namespace bsdiff {

MemoryFile::MemoryFile(const uint8_t* data, size_t size)
    : data_(data), size_(size) {}

bool MemoryFile::Read(void* buf, size_t count, size_t* bytes_read) {
  count = std::min(count, static_cast<size_t>(size_ - offset_));
  memcpy(buf, data_ + offset_, count);
  offset_ += count;
  *bytes_read = count;
  return true;
}

bool MemoryFile::Write(const void* buf, size_t count, size_t* bytes_written) {
  return false;
}

bool MemoryFile::Seek(off_t pos) {
  if (pos > static_cast<off_t>(size_) || pos < 0)
    return false;
  offset_ = pos;
  return true;
}

bool MemoryFile::Close() {
  return true;
}

bool MemoryFile::GetSize(uint64_t* size) {
  *size = size_;
  return true;
}

}  // namespace bsdiff
