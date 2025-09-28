// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sink_file.h"
#include <cstdint>

namespace bsdiff {

SinkFile::SinkFile(const sink_func& sink)
    : sink_(sink) {}

bool SinkFile::Read(void* buf, size_t count, size_t* bytes_read) {
  return false;
}

bool SinkFile::Write(const void* buf, size_t count, size_t* bytes_written) {
  *bytes_written = sink_(static_cast<const uint8_t*>(buf), count);
  return true;
}

bool SinkFile::Seek(off_t pos) {
  return false;
}

bool SinkFile::Close() {
  return true;
}

bool SinkFile::GetSize(uint64_t* size) {
  return false;
}

}  // namespace bsdiff
