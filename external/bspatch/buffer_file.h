// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef _BSDIFF_BUFFER_FILE_H_
#define _BSDIFF_BUFFER_FILE_H_

#include <memory>
#include <cstdint>
#include <vector>
#include <cstdint>

#include "bsdiff/file_interface.h"
#include <cstdint>

namespace bsdiff {

class BufferFile : public FileInterface {
 public:
  // Creates a write only BufferFile based on the underlying |file| passed.
  // The BufferFile will cache all the write in a buffer and write everything
  // to |file| at once upon closing. Read and Seek are not supported.
  // |size| should be the estimated total file size, it is used to reserve
  // buffer space.
  BufferFile(std::unique_ptr<FileInterface> file, size_t size);

  ~BufferFile() override;

  // FileInterface overrides.
  bool Read(void* buf, size_t count, size_t* bytes_read) override;
  bool Write(const void* buf, size_t count, size_t* bytes_written) override;
  bool Seek(off_t pos) override;
  bool Close() override;
  bool GetSize(uint64_t* size) override;

 private:
  // The underlying FileInterace instance.
  std::unique_ptr<FileInterface> file_ = nullptr;
  std::vector<uint8_t> buffer_;
};

}  // namespace bsdiff

#endif  // _BSDIFF_BUFFER_FILE_H_
