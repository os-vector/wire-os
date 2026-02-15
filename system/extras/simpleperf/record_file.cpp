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

#include "record_file.h"

#include <fcntl.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include <set>
#include <vector>

#include <base/logging.h>

#include "event_fd.h"
#include "perf_event.h"
#include "record.h"
#include "utils.h"

using namespace PerfFileFormat;

std::unique_ptr<RecordFileWriter> RecordFileWriter::CreateInstance(
    const std::string& filename, const perf_event_attr& event_attr,
    const std::vector<std::unique_ptr<EventFd>>& event_fds) {
  FILE* fp = fopen(filename.c_str(), "web+");
  if (fp == nullptr) {
    PLOG(ERROR) << "failed to open record file '" << filename << "'";
    return nullptr;
  }

  auto writer = std::unique_ptr<RecordFileWriter>(new RecordFileWriter(filename, fp));
  if (!writer->WriteAttrSection(event_attr, event_fds)) {
    return nullptr;
  }
  return writer;
}

RecordFileWriter::RecordFileWriter(const std::string& filename, FILE* fp)
    : filename_(filename),
      record_fp_(fp),
      attr_section_offset_(0),
      attr_section_size_(0),
      data_section_offset_(0),
      data_section_size_(0),
      feature_count_(0),
      current_feature_index_(0) {
}

RecordFileWriter::~RecordFileWriter() {
  if (record_fp_ != nullptr) {
    Close();
  }
}

bool RecordFileWriter::WriteAttrSection(const perf_event_attr& event_attr,
                                        const std::vector<std::unique_ptr<EventFd>>& event_fds) {
  // Skip file header part.
  if (fseek(record_fp_, sizeof(FileHeader), SEEK_SET) == -1) {
    return false;
  }

  // Write id section.
  std::vector<uint64_t> ids;
  for (auto& event_fd : event_fds) {
    ids.push_back(event_fd->Id());
  }
  long id_section_offset = ftell(record_fp_);
  if (id_section_offset == -1) {
    return false;
  }
  if (!Write(ids.data(), ids.size() * sizeof(uint64_t))) {
    return false;
  }

  // Write attr section.
  FileAttr attr;
  attr.attr = event_attr;
  attr.ids.offset = id_section_offset;
  attr.ids.size = ids.size() * sizeof(uint64_t);

  long attr_section_offset = ftell(record_fp_);
  if (attr_section_offset == -1) {
    return false;
  }
  if (!Write(&attr, sizeof(attr))) {
    return false;
  }

  long data_section_offset = ftell(record_fp_);
  if (data_section_offset == -1) {
    return false;
  }

  attr_section_offset_ = attr_section_offset;
  attr_section_size_ = sizeof(attr);
  data_section_offset_ = data_section_offset;

  // Save event_attr for use when reading records.
  event_attr_ = event_attr;
  return true;
}

bool RecordFileWriter::WriteData(const void* buf, size_t len) {
  if (!Write(buf, len)) {
    return false;
  }
  data_section_size_ += len;
  return true;
}

bool RecordFileWriter::Write(const void* buf, size_t len) {
  if (fwrite(buf, len, 1, record_fp_) != 1) {
    PLOG(ERROR) << "failed to write to record file '" << filename_ << "'";
    return false;
  }
  return true;
}

void RecordFileWriter::GetHitModulesInBuffer(const char* p, const char* end,
                                             std::vector<std::string>* hit_kernel_modules,
                                             std::vector<std::string>* hit_user_files) {
  std::vector<std::unique_ptr<const Record>> kernel_mmaps;
  std::vector<std::unique_ptr<const Record>> user_mmaps;
  std::set<std::string> hit_kernel_set;
  std::set<std::string> hit_user_set;

  while (p < end) {
    auto header = reinterpret_cast<const perf_event_header*>(p);
    CHECK_LE(p + header->size, end);
    p += header->size;
    std::unique_ptr<const Record> record = ReadRecordFromBuffer(event_attr_, header);
    CHECK(record != nullptr);
    if (record->header.type == PERF_RECORD_MMAP) {
      if (record->header.misc & PERF_RECORD_MISC_KERNEL) {
        kernel_mmaps.push_back(std::move(record));
      } else {
        user_mmaps.push_back(std::move(record));
      }
    } else if (record->header.type == PERF_RECORD_SAMPLE) {
      auto& r = *static_cast<const SampleRecord*>(record.get());
      if (!(r.sample_type & PERF_SAMPLE_IP) || !(r.sample_type & PERF_SAMPLE_TID)) {
        continue;
      }
      uint32_t pid = r.tid_data.pid;
      uint64_t ip = r.ip_data.ip;
      if (r.header.misc & PERF_RECORD_MISC_KERNEL) {
        // Loop from back to front, because new MmapRecords are inserted at the end of the mmaps,
        // and we want to match the newest one.
        for (auto it = kernel_mmaps.rbegin(); it != kernel_mmaps.rend(); ++it) {
          auto& m_record = *reinterpret_cast<const MmapRecord*>(it->get());
          if (ip >= m_record.data.addr && ip < m_record.data.addr + m_record.data.len) {
            hit_kernel_set.insert(m_record.filename);
            break;
          }
        }
      } else {
        for (auto it = user_mmaps.rbegin(); it != user_mmaps.rend(); ++it) {
          auto& m_record = *reinterpret_cast<const MmapRecord*>(it->get());
          if (pid == m_record.data.pid && ip >= m_record.data.addr &&
              ip < m_record.data.addr + m_record.data.len) {
            hit_user_set.insert(m_record.filename);
            break;
          }
        }
      }
    }
  }
  hit_kernel_modules->clear();
  hit_kernel_modules->insert(hit_kernel_modules->begin(), hit_kernel_set.begin(),
                             hit_kernel_set.end());
  hit_user_files->clear();
  hit_user_files->insert(hit_user_files->begin(), hit_user_set.begin(), hit_user_set.end());
}

bool RecordFileWriter::GetHitModules(std::vector<std::string>* hit_kernel_modules,
                                     std::vector<std::string>* hit_user_files) {
  if (fflush(record_fp_) != 0) {
    PLOG(ERROR) << "fflush() failed";
    return false;
  }
  if (fseek(record_fp_, 0, SEEK_END) == -1) {
    PLOG(ERROR) << "fseek() failed";
    return false;
  }
  long file_size = ftell(record_fp_);
  if (file_size == -1) {
    PLOG(ERROR) << "ftell() failed";
    return false;
  }
  size_t mmap_len = file_size;
  void* mmap_addr = mmap(nullptr, mmap_len, PROT_READ, MAP_SHARED, fileno(record_fp_), 0);
  if (mmap_addr == MAP_FAILED) {
    PLOG(ERROR) << "mmap() failed";
    return false;
  }
  const char* data_section_p = reinterpret_cast<const char*>(mmap_addr) + data_section_offset_;
  const char* data_section_end = data_section_p + data_section_size_;
  GetHitModulesInBuffer(data_section_p, data_section_end, hit_kernel_modules, hit_user_files);

  if (munmap(mmap_addr, mmap_len) == -1) {
    PLOG(ERROR) << "munmap() failed";
    return false;
  }
  return true;
}

bool RecordFileWriter::WriteFeatureHeader(size_t feature_count) {
  feature_count_ = feature_count;
  current_feature_index_ = 0;
  uint64_t feature_header_size = feature_count * sizeof(SectionDesc);

  // Reserve enough space in the record file for the feature header.
  std::vector<unsigned char> zero_data(feature_header_size);
  if (fseek(record_fp_, data_section_offset_ + data_section_size_, SEEK_SET) == -1) {
    PLOG(ERROR) << "fseek() failed";
    return false;
  }
  return Write(zero_data.data(), zero_data.size());
}

bool RecordFileWriter::WriteBuildIdFeature(const std::vector<BuildIdRecord>& build_id_records) {
  if (current_feature_index_ >= feature_count_) {
    return false;
  }
  // Always write features at the end of the file.
  if (fseek(record_fp_, 0, SEEK_END) == -1) {
    PLOG(ERROR) << "fseek() failed";
    return false;
  }
  long section_start = ftell(record_fp_);
  if (section_start == -1) {
    PLOG(ERROR) << "ftell() failed";
    return false;
  }
  for (auto& record : build_id_records) {
    std::vector<char> data = record.BinaryFormat();
    if (!Write(data.data(), data.size())) {
      return false;
    }
  }
  long section_end = ftell(record_fp_);
  if (section_end == -1) {
    return false;
  }

  // Write feature section descriptor for build_id feature.
  SectionDesc desc;
  desc.offset = section_start;
  desc.size = section_end - section_start;
  uint64_t feature_offset = data_section_offset_ + data_section_size_;
  if (fseek(record_fp_, feature_offset + current_feature_index_ * sizeof(SectionDesc), SEEK_SET) ==
      -1) {
    PLOG(ERROR) << "fseek() failed";
    return false;
  }
  if (fwrite(&desc, sizeof(SectionDesc), 1, record_fp_) != 1) {
    PLOG(ERROR) << "fwrite() failed";
    return false;
  }
  ++current_feature_index_;
  features_.push_back(FEAT_BUILD_ID);
  return true;
}

bool RecordFileWriter::WriteFileHeader() {
  FileHeader header;
  memset(&header, 0, sizeof(header));
  memcpy(header.magic, PERF_MAGIC, sizeof(header.magic));
  header.header_size = sizeof(header);
  header.attr_size = sizeof(FileAttr);
  header.attrs.offset = attr_section_offset_;
  header.attrs.size = attr_section_size_;
  header.data.offset = data_section_offset_;
  header.data.size = data_section_size_;
  for (auto& feature : features_) {
    int i = feature / 8;
    int j = feature % 8;
    header.features[i] |= (1 << j);
  }

  if (fseek(record_fp_, 0, SEEK_SET) == -1) {
    return false;
  }
  if (!Write(&header, sizeof(header))) {
    return false;
  }
  return true;
}

bool RecordFileWriter::Close() {
  CHECK(record_fp_ != nullptr);
  bool result = true;

  // Write file header. We gather enough information to write file header only after
  // writing data section and feature section.
  if (!WriteFileHeader()) {
    result = false;
  }

  if (fclose(record_fp_) != 0) {
    PLOG(ERROR) << "failed to close record file '" << filename_ << "'";
    result = false;
  }
  record_fp_ = nullptr;
  return result;
}

std::unique_ptr<RecordFileReader> RecordFileReader::CreateInstance(const std::string& filename) {
  int fd = open(filename.c_str(), O_RDONLY | O_CLOEXEC);
  if (fd == -1) {
    PLOG(ERROR) << "failed to open record file '" << filename << "'";
    return nullptr;
  }
  auto reader = std::unique_ptr<RecordFileReader>(new RecordFileReader(filename, fd));
  if (!reader->MmapFile()) {
    return nullptr;
  }
  return reader;
}

RecordFileReader::RecordFileReader(const std::string& filename, int fd)
    : filename_(filename), record_fd_(fd), mmap_addr_(nullptr), mmap_len_(0) {
}

RecordFileReader::~RecordFileReader() {
  if (record_fd_ != -1) {
    Close();
  }
}

bool RecordFileReader::Close() {
  bool result = true;
  if (munmap(const_cast<char*>(mmap_addr_), mmap_len_) == -1) {
    PLOG(ERROR) << "failed to munmap() record file '" << filename_ << "'";
    result = false;
  }
  if (close(record_fd_) == -1) {
    PLOG(ERROR) << "failed to close record file '" << filename_ << "'";
    result = false;
  }
  record_fd_ = -1;
  return result;
}

bool RecordFileReader::MmapFile() {
  off64_t file_size = lseek64(record_fd_, 0, SEEK_END);
  if (file_size == -1) {
    return false;
  }
  size_t mmap_len = file_size;
  void* mmap_addr = mmap(nullptr, mmap_len, PROT_READ, MAP_SHARED, record_fd_, 0);
  if (mmap_addr == MAP_FAILED) {
    PLOG(ERROR) << "failed to mmap() record file '" << filename_ << "'";
    return false;
  }

  mmap_addr_ = reinterpret_cast<const char*>(mmap_addr);
  mmap_len_ = mmap_len;
  return true;
}

const FileHeader* RecordFileReader::FileHeader() {
  return reinterpret_cast<const struct FileHeader*>(mmap_addr_);
}

std::vector<const FileAttr*> RecordFileReader::AttrSection() {
  std::vector<const FileAttr*> result;
  const struct FileHeader* header = FileHeader();
  size_t attr_count = header->attrs.size / header->attr_size;
  const FileAttr* attr = reinterpret_cast<const FileAttr*>(mmap_addr_ + header->attrs.offset);
  for (size_t i = 0; i < attr_count; ++i) {
    result.push_back(attr++);
  }
  return result;
}

std::vector<uint64_t> RecordFileReader::IdsForAttr(const FileAttr* attr) {
  std::vector<uint64_t> result;
  size_t id_count = attr->ids.size / sizeof(uint64_t);
  const uint64_t* id = reinterpret_cast<const uint64_t*>(mmap_addr_ + attr->ids.offset);
  for (size_t i = 0; i < id_count; ++i) {
    result.push_back(*id++);
  }
  return result;
}

std::vector<std::unique_ptr<const Record>> RecordFileReader::DataSection() {
  std::vector<std::unique_ptr<const Record>> result;
  const struct FileHeader* header = FileHeader();
  auto file_attrs = AttrSection();
  CHECK(file_attrs.size() > 0);
  perf_event_attr attr = file_attrs[0]->attr;

  const char* end = mmap_addr_ + header->data.offset + header->data.size;
  const char* p = mmap_addr_ + header->data.offset;
  while (p < end) {
    const perf_event_header* header = reinterpret_cast<const perf_event_header*>(p);
    if (p + header->size <= end) {
      result.push_back(std::move(ReadRecordFromBuffer(attr, header)));
    }
    p += header->size;
  }
  return result;
}

std::vector<SectionDesc> RecordFileReader::FeatureSectionDescriptors() {
  std::vector<SectionDesc> result;
  const struct FileHeader* header = FileHeader();
  size_t feature_count = 0;
  for (size_t i = 0; i < sizeof(header->features); ++i) {
    for (size_t j = 0; j < 8; ++j) {
      if (header->features[i] & (1 << j)) {
        ++feature_count;
      }
    }
  }
  uint64_t feature_section_offset = header->data.offset + header->data.size;
  const SectionDesc* p = reinterpret_cast<const SectionDesc*>(mmap_addr_ + feature_section_offset);
  for (size_t i = 0; i < feature_count; ++i) {
    result.push_back(*p++);
  }
  return result;
}
