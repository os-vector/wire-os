/*
 * Copyright (C) 2015 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef SIMPLE_PERF_UTILS_H_
#define SIMPLE_PERF_UTILS_H_

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <vector>

#define ALIGN(value, alignment) (((value) + (alignment)-1) & ~((alignment)-1))

class LineReader {
 public:
  LineReader(FILE* fp) : fp_(fp), buf_(nullptr), bufsize_(0) {
  }

  ~LineReader() {
    free(buf_);
    fclose(fp_);
  }

  char* ReadLine() {
    if (getline(&buf_, &bufsize_, fp_) != -1) {
      return buf_;
    }
    return nullptr;
  }

  size_t MaxLineSize() {
    return bufsize_;
  }

 private:
  FILE* fp_;
  char* buf_;
  size_t bufsize_;
};

void PrintIndented(size_t indent, const char* fmt, ...);

bool IsPowerOfTwo(uint64_t value);

bool NextArgumentOrError(const std::vector<std::string>& args, size_t* pi);

void GetEntriesInDir(const std::string& dirpath, std::vector<std::string>* files,
                     std::vector<std::string>* subdirs);

#endif  // SIMPLE_PERF_UTILS_H_
