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

#ifndef SIMPLE_PERF_RECORD_FILE_H_
#define SIMPLE_PERF_RECORD_FILE_H_

#include <stdio.h>
#include <memory>
#include <string>
#include <vector>

#include <base/macros.h>

#include "perf_event.h"
#include "record.h"
#include "record_file_format.h"

class EventFd;

// RecordFileWriter writes to a perf record file, like perf.data.
class RecordFileWriter {
 public:
  static std::unique_ptr<RecordFileWriter> CreateInstance(
      const std::string& filename, const perf_event_attr& event_attr,
      const std::vector<std::unique_ptr<EventFd>>& event_fds);

  ~RecordFileWriter();

  bool WriteData(const void* buf, size_t len);

  bool WriteData(const std::vector<char>& data) {
    return WriteData(data.data(), data.size());
  }

  // Use MmapRecords and SampleRecords in record file to conclude which modules/files were executing
  // at sample times.
  bool GetHitModules(std::vector<std::string>* hit_kernel_modules,
                     std::vector<std::string>* hit_user_files);

  bool WriteFeatureHeader(size_t feature_count);
  bool WriteBuildIdFeature(const std::vector<BuildIdRecord>& build_id_records);

  // Normally, Close() should be called after writing. But if something
  // wrong happens and we need to finish in advance, the destructor
  // will take care of calling Close().
  bool Close();

 private:
  RecordFileWriter(const std::string& filename, FILE* fp);
  bool WriteAttrSection(const perf_event_attr& event_attr,
                        const std::vector<std::unique_ptr<EventFd>>& event_fds);
  void GetHitModulesInBuffer(const char* p, const char* end,
                             std::vector<std::string>* hit_kernel_modules,
                             std::vector<std::string>* hit_user_files);
  bool WriteFileHeader();
  bool Write(const void* buf, size_t len);

  const std::string filename_;
  FILE* record_fp_;

  perf_event_attr event_attr_;
  uint64_t attr_section_offset_;
  uint64_t attr_section_size_;
  uint64_t data_section_offset_;
  uint64_t data_section_size_;

  std::vector<int> features_;
  int feature_count_;
  int current_feature_index_;

  DISALLOW_COPY_AND_ASSIGN(RecordFileWriter);
};

// RecordFileReader read contents from a perf record file, like perf.data.
class RecordFileReader {
 public:
  static std::unique_ptr<RecordFileReader> CreateInstance(const std::string& filename);

  ~RecordFileReader();

  const PerfFileFormat::FileHeader* FileHeader();
  std::vector<const PerfFileFormat::FileAttr*> AttrSection();
  std::vector<uint64_t> IdsForAttr(const PerfFileFormat::FileAttr* attr);
  std::vector<std::unique_ptr<const Record>> DataSection();
  std::vector<PerfFileFormat::SectionDesc> FeatureSectionDescriptors();
  const char* DataAtOffset(uint64_t offset) {
    return mmap_addr_ + offset;
  }
  bool Close();

 private:
  RecordFileReader(const std::string& filename, int fd);
  bool MmapFile();

  const std::string filename_;
  int record_fd_;

  const char* mmap_addr_;
  size_t mmap_len_;

  DISALLOW_COPY_AND_ASSIGN(RecordFileReader);
};

#endif  // SIMPLE_PERF_RECORD_FILE_H_
