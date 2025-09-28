// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef _BSDIFF_MEMORY_FILE_H_
#define _BSDIFF_MEMORY_FILE_H_

#include <memory>
#include <cstdint>

#include "bsdiff/file_interface.h"
#include <cstdint>

namespace bsdiff {

class MemoryFile : public FileInterface {
 public:
  // Creates a read only MemoryFile based on the underlying |data| passed.
  // The MemoryFile will use data starting from |data| with length of |size| as
  // the file content. Write is not supported.
  MemoryFile(const uint8_t* data, size_t size);

  ~MemoryFile() = default;

  // FileInterface overrides.
  bool Read(void* buf, size_t count, size_t* bytes_read) override;
  bool Write(const void* buf, size_t count, size_t* bytes_written) override;
  bool Seek(off_t pos) override;
  bool Close() override;
  bool GetSize(uint64_t* size) override;

 private:
  const uint8_t* data_ = nullptr;
  size_t size_ = 0;
  off_t offset_ = 0;
};

}  // namespace bsdiff

#endif  // _BSDIFF_MEMORY_FILE_H_
