/*-
 * Copyright 2003-2005 Colin Percival
 * All rights reserved
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted providing that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#if 0
__FBSDID("$FreeBSD: src/usr.bin/bsdiff/bspatch/bspatch.c,v 1.1 2005/08/06 01:59:06 cperciva Exp $");
#endif

#include "bsdiff/bspatch.h"
#include <cstdint>

#include <bzlib.h>
#include <cstdint>
#include <errno.h>
#include <cstdint>
#include <fcntl.h>
#include <cstdint>
#include <inttypes.h>
#include <cstdint>
#include <stdlib.h>
#include <cstdint>
#include <string.h>
#include <cstdint>
#include <unistd.h>
#include <cstdint>
#include <sys/stat.h>
#include <cstdint>
#include <sys/types.h>
#include <cstdint>

#include <algorithm>
#include <cstdint>
#include <memory>
#include <cstdint>
#include <limits>
#include <cstdint>
#include <vector>
#include <cstdint>

#include "bsdiff/extents_file.h"
#include <cstdint>
#include "bsdiff/file_interface.h"
#include <cstdint>
#include "buffer_file.h"
#include <cstdint>
#include "extents.h"
#include <cstdint>
#include "file.h"
#include <cstdint>
#include "memory_file.h"
#include <cstdint>
#include "sink_file.h"
#include <cstdint>

namespace {

int64_t ParseInt64(const u_char* buf) {
  int64_t y;

  y = buf[7] & 0x7F;
  y = y * 256;
  y += buf[6];
  y = y * 256;
  y += buf[5];
  y = y * 256;
  y += buf[4];
  y = y * 256;
  y += buf[3];
  y = y * 256;
  y += buf[2];
  y = y * 256;
  y += buf[1];
  y = y * 256;
  y += buf[0];

  if (buf[7] & 0x80)
    y = -y;

  return y;
}

bool ReadBZ2(bz_stream* stream, uint8_t* data, size_t size) {
  stream->next_out = (char*)data;
  while (size > 0) {
    unsigned int read_size = std::min(
        static_cast<size_t>(std::numeric_limits<unsigned int>::max()), size);
    stream->avail_out = read_size;
    int bz2err = BZ2_bzDecompress(stream);
    if (bz2err != BZ_OK && bz2err != BZ_STREAM_END)
      return false;
    size -= read_size - stream->avail_out;
  }
  return true;
}

int ReadBZ2AndWriteAll(const std::unique_ptr<bsdiff::FileInterface>& file,
                       bz_stream* stream,
                       size_t size,
                       uint8_t* buf,
                       size_t buf_size) {
  while (size > 0) {
    size_t bytes_to_read = std::min(size, buf_size);
    if (!ReadBZ2(stream, buf, bytes_to_read)) {
      fprintf(stderr, "Failed to read bzip stream.\n");
      return 2;
    }
    if (!WriteAll(file, buf, bytes_to_read)) {
      perror("WriteAll() failed");
      return 1;
    }
    size -= bytes_to_read;
  }
  return 0;
}

}  // namespace

namespace bsdiff {

bool ReadAll(const std::unique_ptr<FileInterface>& file,
             uint8_t* data,
             size_t size) {
  size_t offset = 0, read;
  while (offset < size) {
    if (!file->Read(data + offset, size - offset, &read) || read == 0)
      return false;
    offset += read;
  }
  return true;
}

bool WriteAll(const std::unique_ptr<FileInterface>& file,
              const uint8_t* data,
              size_t size) {
  size_t offset = 0, written;
  while (offset < size) {
    if (!file->Write(data + offset, size - offset, &written) || written == 0)
      return false;
    offset += written;
  }
  return true;
}

bool IsOverlapping(const char* old_filename,
                   const char* new_filename,
                   const std::vector<ex_t>& old_extents,
                   const std::vector<ex_t>& new_extents) {
  struct stat old_stat, new_stat;
  if (stat(new_filename, &new_stat) == -1) {
    if (errno == ENOENT)
      return false;
    fprintf(stderr, "Error stat the new file %s: %s\n", new_filename,
            strerror(errno));
    return true;
  }
  if (stat(old_filename, &old_stat) == -1) {
    fprintf(stderr, "Error stat the old file %s: %s\n", old_filename,
            strerror(errno));
    return true;
  }

  if (old_stat.st_dev != new_stat.st_dev || old_stat.st_ino != new_stat.st_ino)
    return false;

  if (old_extents.empty() && new_extents.empty())
    return true;

  for (ex_t old_ex : old_extents)
    for (ex_t new_ex : new_extents)
      if (static_cast<uint64_t>(old_ex.off) < new_ex.off + new_ex.len &&
          static_cast<uint64_t>(new_ex.off) < old_ex.off + old_ex.len)
        return true;

  return false;
}

// Patch |old_filename| with |patch_filename| and save it to |new_filename|.
// |old_extents| and |new_extents| are comma-separated lists of "offset:length"
// extents of |old_filename| and |new_filename|.
// Returns 0 on success, 1 on I/O error and 2 on data error.
int bspatch(const char* old_filename,
            const char* new_filename,
            const char* patch_filename,
            const char* old_extents,
            const char* new_extents) {
  std::unique_ptr<FileInterface> patch_file =
      File::FOpen(patch_filename, O_RDONLY);
  if (!patch_file) {
    fprintf(stderr, "Error opening the patch file %s: %s\n", patch_filename,
            strerror(errno));
    return 1;
  }
  uint64_t patch_size;
  patch_file->GetSize(&patch_size);
  std::vector<uint8_t> patch(patch_size);
  if (!ReadAll(patch_file, patch.data(), patch_size)) {
    fprintf(stderr, "Error reading the patch file %s: %s\n", patch_filename,
            strerror(errno));
    return 1;
  }
  patch_file.reset();

  return bspatch(old_filename, new_filename, patch.data(), patch_size,
                 old_extents, new_extents);
}

// Patch |old_filename| with |patch_data| and save it to |new_filename|.
// |old_extents| and |new_extents| are comma-separated lists of "offset:length"
// extents of |old_filename| and |new_filename|.
// Returns 0 on success, 1 on I/O error and 2 on data error.
int bspatch(const char* old_filename,
            const char* new_filename,
            const uint8_t* patch_data,
            size_t patch_size,
            const char* old_extents,
            const char* new_extents) {
  int using_extents = (old_extents != NULL || new_extents != NULL);

  // Open input file for reading.
  std::unique_ptr<FileInterface> old_file = File::FOpen(old_filename, O_RDONLY);
  if (!old_file) {
    fprintf(stderr, "Error opening the old file %s: %s\n", old_filename,
            strerror(errno));
    return 1;
  }

  std::vector<ex_t> parsed_old_extents;
  if (using_extents) {
    if (!ParseExtentStr(old_extents, &parsed_old_extents)) {
      fprintf(stderr, "Error parsing the old extents\n");
      return 2;
    }
    old_file.reset(new ExtentsFile(std::move(old_file), parsed_old_extents));
  }

  // Open output file for writing.
  std::unique_ptr<FileInterface> new_file =
      File::FOpen(new_filename, O_CREAT | O_WRONLY);
  if (!new_file) {
    fprintf(stderr, "Error opening the new file %s: %s\n", new_filename,
            strerror(errno));
    return 1;
  }

  std::vector<ex_t> parsed_new_extents;
  if (using_extents) {
    if (!ParseExtentStr(new_extents, &parsed_new_extents)) {
      fprintf(stderr, "Error parsing the new extents\n");
      return 2;
    }
    new_file.reset(new ExtentsFile(std::move(new_file), parsed_new_extents));
  }

  if (IsOverlapping(old_filename, new_filename, parsed_old_extents,
                    parsed_new_extents)) {
    // New and old file is overlapping, we can not stream output to new file,
    // cache it in a buffer and write to the file at the end.
    uint64_t newsize = ParseInt64(patch_data + 24);
    new_file.reset(new BufferFile(std::move(new_file), newsize));
  }

  return bspatch(old_file, new_file, patch_data, patch_size);
}

// Patch |old_data| with |patch_data| and save it by calling sink function.
// Returns 0 on success, 1 on I/O error and 2 on data error.
int bspatch(const uint8_t* old_data,
            size_t old_size,
            const uint8_t* patch_data,
            size_t patch_size,
            const sink_func& sink) {
  std::unique_ptr<FileInterface> old_file(new MemoryFile(old_data, old_size));
  std::unique_ptr<FileInterface> new_file(new SinkFile(sink));

  return bspatch(old_file, new_file, patch_data, patch_size);
}

// Patch |old_file| with |patch_data| and save it to |new_file|.
// Returns 0 on success, 1 on I/O error and 2 on data error.
int bspatch(const std::unique_ptr<FileInterface>& old_file,
            const std::unique_ptr<FileInterface>& new_file,
            const uint8_t* patch_data,
            size_t patch_size) {
  int bz2err;
  u_char buf[8];
  off_t ctrl[3];

  // File format:
  //   0       8    "BSDIFF40"
  //   8       8    X
  //   16      8    Y
  //   24      8    sizeof(new_filename)
  //   32      X    bzip2(control block)
  //   32+X    Y    bzip2(diff block)
  //   32+X+Y  ???  bzip2(extra block)
  // with control block a set of triples (x,y,z) meaning "add x bytes
  // from oldfile to x bytes from the diff block; copy y bytes from the
  // extra block; seek forwards in oldfile by z bytes".

  // Check for appropriate magic.
  if (memcmp(patch_data, "BSDIFF40", 8) != 0) {
    fprintf(stderr, "Not a bsdiff patch.\n");
    return 2;
  }

  // Read lengths from header.
  uint64_t oldsize, newsize;
  int64_t ctrl_len = ParseInt64(patch_data + 8);
  int64_t data_len = ParseInt64(patch_data + 16);
  int64_t signed_newsize = ParseInt64(patch_data + 24);
  newsize = signed_newsize;
  if ((ctrl_len < 0) || (data_len < 0) || (signed_newsize < 0) ||
      (32 + ctrl_len + data_len > static_cast<int64_t>(patch_size))) {
    fprintf(stderr, "Corrupt patch.\n");
    return 2;
  }

  bz_stream cstream;
  cstream.next_in = (char*)patch_data + 32;
  cstream.avail_in = ctrl_len;
  cstream.bzalloc = nullptr;
  cstream.bzfree = nullptr;
  cstream.opaque = nullptr;
  if ((bz2err = BZ2_bzDecompressInit(&cstream, 0, 0)) != BZ_OK) {
    fprintf(stderr, "Failed to bzinit control stream (%d)\n", bz2err);
    return 2;
  }

  bz_stream dstream;
  dstream.next_in = (char*)patch_data + 32 + ctrl_len;
  dstream.avail_in = data_len;
  dstream.bzalloc = nullptr;
  dstream.bzfree = nullptr;
  dstream.opaque = nullptr;
  if ((bz2err = BZ2_bzDecompressInit(&dstream, 0, 0)) != BZ_OK) {
    fprintf(stderr, "Failed to bzinit diff stream (%d)\n", bz2err);
    return 2;
  }

  bz_stream estream;
  estream.next_in = (char*)patch_data + 32 + ctrl_len + data_len;
  estream.avail_in = patch_size - (32 + ctrl_len + data_len);
  estream.bzalloc = nullptr;
  estream.bzfree = nullptr;
  estream.opaque = nullptr;
  if ((bz2err = BZ2_bzDecompressInit(&estream, 0, 0)) != BZ_OK) {
    fprintf(stderr, "Failed to bzinit extra stream (%d)\n", bz2err);
    return 2;
  }

  uint64_t old_file_pos = 0;

  if (!old_file->GetSize(&oldsize)) {
    fprintf(stderr, "Cannot obtain the size of old file.\n");
    return 1;
  }

  // The oldpos can be negative, but the new pos is only incremented linearly.
  int64_t oldpos = 0;
  uint64_t newpos = 0;
  std::vector<uint8_t> old_buf(1024 * 1024), new_buf(1024 * 1024);
  while (newpos < newsize) {
    int64_t i;
    // Read control data.
    for (i = 0; i <= 2; i++) {
      if (!ReadBZ2(&cstream, buf, 8)) {
        fprintf(stderr, "Failed to read control stream.\n");
        return 2;
      }
      ctrl[i] = ParseInt64(buf);
    }

    // Sanity-check.
    if (ctrl[0] < 0 || ctrl[1] < 0) {
      fprintf(stderr, "Corrupt patch.\n");
      return 2;
    }

    // Sanity-check.
    if (newpos + ctrl[0] > newsize) {
      fprintf(stderr, "Corrupt patch.\n");
      return 2;
    }

    int ret = 0;
    // Add old data to diff string. It is enough to fseek once, at
    // the beginning of the sequence, to avoid unnecessary overhead.
    if ((i = oldpos) < 0) {
      // Write diff block directly to new file without adding old data,
      // because we will skip part where |oldpos| < 0.
      ret = ReadBZ2AndWriteAll(new_file, &dstream, -i, new_buf.data(),
                               new_buf.size());
      if (ret)
        return ret;
      i = 0;
    }

    // We just checked that |i| is not negative.
    if (static_cast<uint64_t>(i) != old_file_pos && !old_file->Seek(i)) {
      fprintf(stderr, "Error seeking input file to offset %" PRId64 ": %s\n", i,
              strerror(errno));
      return 1;
    }
    if ((old_file_pos = oldpos + ctrl[0]) > oldsize)
      old_file_pos = oldsize;

    size_t chunk_size = old_file_pos - i;
    while (chunk_size > 0) {
      size_t read_bytes;
      size_t bytes_to_read = std::min(chunk_size, old_buf.size());
      if (!old_file->Read(old_buf.data(), bytes_to_read, &read_bytes)) {
        perror("Error reading from input file");
        return 1;
      }
      if (!read_bytes) {
        fprintf(stderr, "EOF reached while reading from input file.\n");
        return 2;
      }
      // Read same amount of bytes from diff block
      if (!ReadBZ2(&dstream, new_buf.data(), read_bytes)) {
        fprintf(stderr, "Failed to read diff stream.\n");
        return 2;
      }
      // new_buf already has data from diff block, adds old data to it.
      for (size_t k = 0; k < read_bytes; k++)
        new_buf[k] += old_buf[k];
      if (!WriteAll(new_file, new_buf.data(), read_bytes)) {
        perror("Error writing to new file");
        return 1;
      }
      chunk_size -= read_bytes;
    }

    // Adjust pointers.
    newpos += ctrl[0];
    oldpos += ctrl[0];

    if (oldpos > static_cast<int64_t>(oldsize)) {
      // Write diff block directly to new file without adding old data,
      // because we skipped part where |oldpos| > oldsize.
      ret = ReadBZ2AndWriteAll(new_file, &dstream, oldpos - oldsize,
                               new_buf.data(), new_buf.size());
      if (ret)
        return ret;
    }

    // Sanity-check.
    if (newpos + ctrl[1] > newsize) {
      fprintf(stderr, "Corrupt patch.\n");
      return 2;
    }

    // Read extra block.
    ret = ReadBZ2AndWriteAll(new_file, &estream, ctrl[1], new_buf.data(),
                             new_buf.size());
    if (ret)
      return ret;

    // Adjust pointers.
    newpos += ctrl[1];
    oldpos += ctrl[2];
  }

  // Close input file.
  old_file->Close();

  // Clean up the bzip2 reads.
  BZ2_bzDecompressEnd(&cstream);
  BZ2_bzDecompressEnd(&dstream);
  BZ2_bzDecompressEnd(&estream);

  if (!new_file->Close()) {
    perror("Error closing new file");
    return 1;
  }

  return 0;
}

}  // namespace bsdiff
