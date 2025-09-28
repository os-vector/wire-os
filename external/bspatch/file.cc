// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "file.h"
#include <cstdint>

#include <errno.h>
#include <cstdint>
#include <fcntl.h>
#include <cstdint>
#ifdef __linux__
#include <linux/fs.h>
#include <cstdint>
#endif  // __linux__
#include <string.h>
#include <cstdint>
#include <sys/ioctl.h>
#include <cstdint>
#include <sys/stat.h>
#include <cstdint>
#include <sys/types.h>
#include <cstdint>
#include <unistd.h>
#include <cstdint>

// TEMP_FAILURE_RETRY is defined by some versions of <unistd.h>.
#ifndef TEMP_FAILURE_RETRY
#include <utils/Compat.h>
#include <cstdint>
#endif

#include <algorithm>
#include <cstdint>

#include <limits>
#include <cstdint>


namespace bsdiff {

std::unique_ptr<File> File::FOpen(const char* pathname, int flags) {
  int fd = TEMP_FAILURE_RETRY(open(pathname, flags, 0644));
  if (fd < 0)
    return std::unique_ptr<File>();
  return std::unique_ptr<File>(new File(fd));
}

File::~File() {
  Close();
}

bool File::Read(void* buf, size_t count, size_t* bytes_read) {
  if (fd_ < 0) {
    errno = EBADF;
    return false;
  }
  ssize_t rc = TEMP_FAILURE_RETRY(read(fd_, buf, count));
  if (rc == -1)
    return false;
  *bytes_read = static_cast<size_t>(rc);
  return true;
}

bool File::Write(const void* buf, size_t count, size_t* bytes_written) {
  if (fd_ < 0) {
    errno = EBADF;
    return false;
  }
  ssize_t rc = TEMP_FAILURE_RETRY(write(fd_, buf, count));
  if (rc == -1)
    return false;
  *bytes_written = static_cast<size_t>(rc);
  return true;
}

bool File::Seek(off_t pos) {
  if (fd_ < 0) {
    errno = EBADF;
    return false;
  }
  // fseek() uses a long value for the offset which could be smaller than off_t.
  if (pos > std::numeric_limits<long>::max()) {
    errno = EOVERFLOW;
    return false;
  }
  off_t newpos = lseek(fd_, pos, SEEK_SET);
  if (newpos < 0)
    return false;
  if (newpos != pos) {
    errno = EINVAL;
    return false;
  }
  return true;
}

bool File::Close() {
  if (fd_ < 0) {
    errno = EBADF;
    return false;
  }
  bool success = close(fd_) == 0;
  if (!success && errno == EINTR)
    success = true;
  fd_ = -1;
  return success;
}

bool File::GetSize(uint64_t* size) {
  struct stat stbuf;
  if (fstat(fd_, &stbuf) == -1)
    return false;
  if (S_ISREG(stbuf.st_mode)) {
    *size = stbuf.st_size;
    return true;
  }
  if (S_ISBLK(stbuf.st_mode)) {
#if defined(BLKGETSIZE64)
    return ioctl(fd_, BLKGETSIZE64, size);
#elif defined(DKIOCGETBLOCKCOUNT)
    uint64_t sectors = 0;
    if (ioctl(fd_, DKIOCGETBLOCKCOUNT, &sectors) == 0) {
      *size = sectors << 9;
      return true;
    }
    return false;
#else
    // Fall back to doing seeks to know the EOF.
    off_t pos = lseek(fd_, 0, SEEK_CUR);
    if (pos == -1)
      return false;
    off_t end_pos = lseek(fd_, 0, SEEK_END);
    if (end_pos == -1)
      return false;
    *size = end_pos;
    lseek(fd_, 0, SEEK_END);
    return true;
#endif
  }
  return false;
}

File::File(int fd)
    : fd_(fd) {}

}  // namespace bsdiff
