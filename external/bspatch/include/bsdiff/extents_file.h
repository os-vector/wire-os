// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef _BSDIFF_EXTENTS_FILE_H_
#define _BSDIFF_EXTENTS_FILE_H_

#include <stdio.h>
#include <cstdint>

#include <memory>
#include <cstdint>
#include <vector>
#include <cstdint>

#include "bsdiff/file_interface.h"
#include <cstdint>

/*
 * Extent files.
 *
 * This modules provides a familiar interface for handling files through an
 * indirection layer of extents, which are contiguous chunks of variable length
 * at arbitrary offsets within a file.  Once an extent file handle is obtained,
 * users may read, write and seek as they do with ordinary files, having the I/O
 * with the underlying file done for them by the extent file implementation. The
 * implementation supports "sparse extents", which are assumed to contain zeros
 * but otherwise have no actual representation in the underlying file; these are
 * denoted by negative offset values.
 *
 * Unlike ordinary files, the size of an extent file is fixed; it is not
 * truncated on open, nor is writing past the extent span allowed. Also, writing
 * to a sparse extent has no effect and will not raise an error.
 */

namespace bsdiff {

/* An extent, defined by an offset and a length. */
struct ex_t {
  off_t off;     // the extent offset; negative indicates a sparse extent.
  uint64_t len;  // the extent length.
};

class ExtentsFile : public FileInterface {
 public:
  // Creates an ExtentsFile based on the underlying |file| passed. The positions
  // in the ExtentsFile will be linearly mapped to the extents provided in
  // |extents|. The created ExtentsFile takes ownership of the |file| will close
  // it on destruction.
  ExtentsFile(std::unique_ptr<FileInterface> file,
              const std::vector<ex_t>& extents);

  ~ExtentsFile() override;

  // FileInterface overrides.
  bool Read(void* buf, size_t count, size_t* bytes_read) override;
  bool Write(const void* buf, size_t count, size_t* bytes_written) override;
  bool Seek(off_t pos) override;
  bool Close() override;
  bool GetSize(uint64_t* size) override;

 private:
  void AdvancePos(uint64_t size);

  // Performs an I/O operation (either read or write). This template shares the
  // code for both Read() and Write() implementations.
  template <typename T>
  bool IOOperation(bool (FileInterface::*io_op)(T*, size_t, size_t*),
                   T* buf,
                   size_t count,
                   size_t* bytes_processed);

  // The underlying FileInterace instance.
  std::unique_ptr<FileInterface> file_;

  // The list of extents mapping this instance to |file_|.
  const std::vector<ex_t> extents_;

  // The accumulated length of the extents. The i-th element contains the sum of
  // the length of all the extents from 0 up to but not including the i-th
  // extent. This reduces the complexity for random-access Seek() calls.
  std::vector<uint64_t> acc_len_;

  // Current extent index.
  size_t curr_ex_idx_{0};

  // Current logical file position.
  uint64_t curr_pos_{0};

  // Total length of all extents (constant).
  uint64_t total_ex_len_{0};
};

}  // namespace bsdiff

#endif  // _BSDIFF_EXTENTS_FILE_H_
