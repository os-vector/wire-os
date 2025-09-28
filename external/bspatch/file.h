// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef _BSDIFF_FILE_H_
#define _BSDIFF_FILE_H_

#include <memory>
#include <cstdint>

#include "bsdiff/file_interface.h"
#include <cstdint>

namespace bsdiff {

class File : public FileInterface {
 public:
  // Opens a file |pathname| with flags |flags| as defined by open(2). In case
  // of error, an empty unique_ptr is returned and errno is set accordingly.
  static std::unique_ptr<File> FOpen(const char* pathname, int flags);

  ~File() override;

  // FileInterface overrides.
  bool Read(void* buf, size_t count, size_t* bytes_read) override;
  bool Write(const void* buf, size_t count, size_t* bytes_written) override;
  bool Seek(off_t pos) override;
  bool Close() override;
  bool GetSize(uint64_t* size) override;

 private:
  // Creates the File instance for the |fd|. Takes ownership of the file
  // descriptor.
  explicit File(int fd);

  int fd_;
};

}  // namespace bsdiff

#endif  // _BSDIFF_FILE_H_
