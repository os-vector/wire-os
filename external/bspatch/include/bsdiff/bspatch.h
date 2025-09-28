// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef _BSDIFF_BSPATCH_H_
#define _BSDIFF_BSPATCH_H_

#include <functional>
#include <cstdint>
#include <memory>
#include <cstdint>
#include <vector>
#include <cstdint>

#include "bsdiff/extents_file.h"
#include <cstdint>

namespace bsdiff {

int bspatch(const char* old_filename,
            const char* new_filename,
            const char* patch_filename,
            const char* old_extents,
            const char* new_extents);

int bspatch(const char* old_filename,
            const char* new_filename,
            const uint8_t* patch_data,
            size_t patch_size,
            const char* old_extents,
            const char* new_extents);

int bspatch(const uint8_t* old_data,
            size_t old_size,
            const uint8_t* patch_data,
            size_t patch_size,
            const std::function<size_t(const uint8_t*, size_t)>& sink);

int bspatch(const std::unique_ptr<FileInterface>& old_file,
            const std::unique_ptr<FileInterface>& new_file,
            const uint8_t* patch_data,
            size_t patch_size);

bool WriteAll(const std::unique_ptr<FileInterface>& file,
              const uint8_t* data,
              size_t size);

bool IsOverlapping(const char* old_filename,
                   const char* new_filename,
                   const std::vector<ex_t>& old_extents,
                   const std::vector<ex_t>& new_extents);

}  // namespace bsdiff

#endif  // _BSDIFF_BSPATCH_H_
