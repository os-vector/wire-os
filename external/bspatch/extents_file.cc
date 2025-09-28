// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bsdiff/extents_file.h"
#include <cstdint>

#include <string.h>
#include <cstdint>

#include <algorithm>
#include <cstdint>

// Extent files implementation extending FileInterface.
//
// This class allows to map linear positions in a file to a list of regions in
// another file. All the reads and writes are unbuffered, passed directly to the
// underlying file. Seeking is done in O(log(N)), where N is the number of
// extents in the file, but sequential reads jump to the next extent in O(1).

namespace bsdiff {

ExtentsFile::ExtentsFile(std::unique_ptr<FileInterface> file,
                         const std::vector<ex_t>& extents)
    : file_(std::move(file)), extents_(extents) {
  acc_len_.reserve(extents.size());
  for (const ex_t& extent : extents) {
    acc_len_.push_back(total_ex_len_);
    total_ex_len_ += extent.len;
  }
}

ExtentsFile::~ExtentsFile() {
  Close();
}

bool ExtentsFile::Read(void* buf, size_t count, size_t* bytes_read) {
  return IOOperation(&FileInterface::Read, buf, count, bytes_read);
}


bool ExtentsFile::Write(const void* buf, size_t count, size_t* bytes_written) {
  return IOOperation(&FileInterface::Write, buf, count, bytes_written);
}

bool ExtentsFile::Seek(off_t pos) {
  if (pos < 0 || static_cast<uint64_t>(pos) > total_ex_len_)
    return false;
  if (acc_len_.empty())
    return true;
  // Note that the first element of acc_len_ is always 0, and pos is at least 0,
  // so the upper_bound will never return acc_len_.begin().
  curr_pos_ = pos;
  curr_ex_idx_ = std::upper_bound(acc_len_.begin(), acc_len_.end(), pos) -
                 acc_len_.begin();
  // We handle the corner case where |pos| is the size of all the extents by
  // leaving the value of curr_ex_idx_ the same way AdvancePos(0) would leave it
  // after the seek.
  if (curr_pos_ < total_ex_len_)
    curr_ex_idx_--;
  return true;
}

bool ExtentsFile::Close() {
  return file_->Close();
}

bool ExtentsFile::GetSize(uint64_t* size) {
  *size = total_ex_len_;
  return true;
}

void ExtentsFile::AdvancePos(uint64_t size) {
  curr_pos_ += size;
  for (; curr_ex_idx_ < extents_.size(); curr_ex_idx_++) {
    if (curr_pos_ < acc_len_[curr_ex_idx_] + extents_[curr_ex_idx_].len)
      return;
  }
  return;
}

template <typename T>
bool ExtentsFile::IOOperation(bool (FileInterface::*io_op)(T*, size_t, size_t*),
                              T* buf,
                              size_t count,
                              size_t* bytes_processed) {
  bool result = true;
  size_t processed = 0;
  AdvancePos(0);
  while (count > 0 && curr_ex_idx_ < extents_.size()) {
    const ex_t& ex = extents_[curr_ex_idx_];
    off_t curr_ex_off = curr_pos_ - acc_len_[curr_ex_idx_];
    size_t chunk_size =
        std::min(static_cast<uint64_t>(count), ex.len - curr_ex_off);
    size_t chunk_processed = 0;
    if (ex.off < 0) {
      chunk_processed = chunk_size;
    } else {
      if (!file_->Seek(ex.off + curr_ex_off) ||
          !(file_.get()->*io_op)(buf, chunk_size, &chunk_processed)) {
        processed += chunk_processed;
        result = processed > 0;
        break;
      }
    }
    processed += chunk_processed;
    count -= chunk_processed;
    // T can be either const void* or void*. We const_cast it back to void* and
    // then char* to do the arithmetic operation, but |buf| will continue to be
    // const void* if it was defined that way.
    buf = static_cast<char*>(const_cast<void*>(buf)) + chunk_processed;
    AdvancePos(chunk_processed);
    if (!chunk_processed)
      break;
  }
  *bytes_processed = processed;
  return result;
}

}  // namespace bsdiff
