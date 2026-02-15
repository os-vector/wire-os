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

#include "record.h"

#include <inttypes.h>
#include <unordered_map>

#include <base/logging.h>
#include <base/stringprintf.h>

#include "environment.h"
#include "utils.h"

static std::string RecordTypeToString(int record_type) {
  static std::unordered_map<int, std::string> record_type_names = {
      {PERF_RECORD_MMAP, "mmap"},
      {PERF_RECORD_LOST, "lost"},
      {PERF_RECORD_COMM, "comm"},
      {PERF_RECORD_EXIT, "exit"},
      {PERF_RECORD_THROTTLE, "throttle"},
      {PERF_RECORD_UNTHROTTLE, "unthrottle"},
      {PERF_RECORD_FORK, "fork"},
      {PERF_RECORD_READ, "read"},
      {PERF_RECORD_SAMPLE, "sample"},
      {PERF_RECORD_BUILD_ID, "build_id"},
  };

  auto it = record_type_names.find(record_type);
  if (it != record_type_names.end()) {
    return it->second;
  }
  return android::base::StringPrintf("unknown(%d)", record_type);
}

template <class T>
void MoveFromBinaryFormat(T& data, const char*& p) {
  data = *reinterpret_cast<const T*>(p);
  p += sizeof(T);
}

template <class T>
void MoveToBinaryFormat(const T& data, char*& p) {
  *reinterpret_cast<T*>(p) = data;
  p += sizeof(T);
}

SampleId::SampleId() {
  memset(this, 0, sizeof(SampleId));
}

// Return sample_id size in binary format.
size_t SampleId::CreateContent(const perf_event_attr& attr) {
  sample_id_all = attr.sample_id_all;
  sample_type = attr.sample_type;
  // Other data are not necessary. TODO: Set missing SampleId data.
  size_t size = 0;
  if (sample_id_all) {
    if (sample_type & PERF_SAMPLE_TID) {
      size += sizeof(PerfSampleTidType);
    }
    if (sample_type & PERF_SAMPLE_TIME) {
      size += sizeof(PerfSampleTimeType);
    }
    if (sample_type & PERF_SAMPLE_ID) {
      size += sizeof(PerfSampleIdType);
    }
    if (sample_type & PERF_SAMPLE_STREAM_ID) {
      size += sizeof(PerfSampleStreamIdType);
    }
    if (sample_type & PERF_SAMPLE_CPU) {
      size += sizeof(PerfSampleCpuType);
    }
  }
  return size;
}

void SampleId::ReadFromBinaryFormat(const perf_event_attr& attr, const char* p, const char* end) {
  sample_id_all = attr.sample_id_all;
  sample_type = attr.sample_type;
  if (sample_id_all) {
    if (sample_type & PERF_SAMPLE_TID) {
      MoveFromBinaryFormat(tid_data, p);
    }
    if (sample_type & PERF_SAMPLE_TIME) {
      MoveFromBinaryFormat(time_data, p);
    }
    if (sample_type & PERF_SAMPLE_ID) {
      MoveFromBinaryFormat(id_data, p);
    }
    if (sample_type & PERF_SAMPLE_STREAM_ID) {
      MoveFromBinaryFormat(stream_id_data, p);
    }
    if (sample_type & PERF_SAMPLE_CPU) {
      MoveFromBinaryFormat(cpu_data, p);
    }
    // TODO: Add parsing of PERF_SAMPLE_IDENTIFIER.
  }
  CHECK_LE(p, end);
  if (p < end) {
    LOG(DEBUG) << "Record SampleId part has " << end - p << " bytes left\n";
  }
}

void SampleId::WriteToBinaryFormat(char*& p) const {
  if (sample_id_all) {
    if (sample_type & PERF_SAMPLE_TID) {
      MoveToBinaryFormat(tid_data, p);
    }
    if (sample_type & PERF_SAMPLE_TIME) {
      MoveToBinaryFormat(time_data, p);
    }
    if (sample_type & PERF_SAMPLE_ID) {
      MoveToBinaryFormat(id_data, p);
    }
    if (sample_type & PERF_SAMPLE_STREAM_ID) {
      MoveToBinaryFormat(stream_id_data, p);
    }
    if (sample_type & PERF_SAMPLE_CPU) {
      MoveToBinaryFormat(cpu_data, p);
    }
  }
}

void SampleId::Dump(size_t indent) const {
  if (sample_id_all) {
    if (sample_type & PERF_SAMPLE_TID) {
      PrintIndented(indent, "sample_id: pid %u, tid %u\n", tid_data.pid, tid_data.tid);
    }
    if (sample_type & PERF_SAMPLE_TIME) {
      PrintIndented(indent, "sample_id: time %" PRId64 "\n", time_data.time);
    }
    if (sample_type & PERF_SAMPLE_ID) {
      PrintIndented(indent, "sample_id: stream_id %" PRId64 "\n", id_data.id);
    }
    if (sample_type & PERF_SAMPLE_STREAM_ID) {
      PrintIndented(indent, "sample_id: stream_id %" PRId64 "\n", stream_id_data.stream_id);
    }
    if (sample_type & PERF_SAMPLE_CPU) {
      PrintIndented(indent, "sample_id: cpu %u, res %u\n", cpu_data.cpu, cpu_data.res);
    }
  }
}

Record::Record() {
  memset(&header, 0, sizeof(header));
}

Record::Record(const perf_event_header* pheader) {
  header = *pheader;
}

void Record::Dump(size_t indent) const {
  PrintIndented(indent, "record %s: type %u, misc %u, size %u\n",
                RecordTypeToString(header.type).c_str(), header.type, header.misc, header.size);
  DumpData(indent + 1);
  sample_id.Dump(indent + 1);
}

MmapRecord::MmapRecord(const perf_event_attr& attr, const perf_event_header* pheader)
    : Record(pheader) {
  const char* p = reinterpret_cast<const char*>(pheader + 1);
  const char* end = reinterpret_cast<const char*>(pheader) + pheader->size;
  MoveFromBinaryFormat(data, p);
  filename = p;
  p += ALIGN(filename.size() + 1, 8);
  CHECK_LE(p, end);
  sample_id.ReadFromBinaryFormat(attr, p, end);
}

void MmapRecord::DumpData(size_t indent) const {
  PrintIndented(indent, "pid %u, tid %u, addr %p, len 0x%" PRIx64 "\n", data.pid, data.tid,
                reinterpret_cast<void*>(data.addr), data.len);
  PrintIndented(indent, "pgoff 0x%" PRIx64 ", filename %s\n", data.pgoff, filename.c_str());
}

std::vector<char> MmapRecord::BinaryFormat() const {
  std::vector<char> buf(header.size);
  char* p = buf.data();
  MoveToBinaryFormat(header, p);
  MoveToBinaryFormat(data, p);
  strcpy(p, filename.c_str());
  p += ALIGN(filename.size() + 1, 8);
  sample_id.WriteToBinaryFormat(p);
  return buf;
}

CommRecord::CommRecord(const perf_event_attr& attr, const perf_event_header* pheader)
    : Record(pheader) {
  const char* p = reinterpret_cast<const char*>(pheader + 1);
  const char* end = reinterpret_cast<const char*>(pheader) + pheader->size;
  MoveFromBinaryFormat(data, p);
  comm = p;
  p += ALIGN(strlen(p) + 1, 8);
  CHECK_LE(p, end);
  sample_id.ReadFromBinaryFormat(attr, p, end);
}

void CommRecord::DumpData(size_t indent) const {
  PrintIndented(indent, "pid %u, tid %u, comm %s\n", data.pid, data.tid, comm.c_str());
}

std::vector<char> CommRecord::BinaryFormat() const {
  std::vector<char> buf(header.size);
  char* p = buf.data();
  MoveToBinaryFormat(header, p);
  MoveToBinaryFormat(data, p);
  strcpy(p, comm.c_str());
  p += ALIGN(comm.size() + 1, 8);
  sample_id.WriteToBinaryFormat(p);
  return buf;
}

ExitRecord::ExitRecord(const perf_event_attr& attr, const perf_event_header* pheader)
    : Record(pheader) {
  const char* p = reinterpret_cast<const char*>(pheader + 1);
  const char* end = reinterpret_cast<const char*>(pheader) + pheader->size;
  MoveFromBinaryFormat(data, p);
  CHECK_LE(p, end);
  sample_id.ReadFromBinaryFormat(attr, p, end);
}

void ExitRecord::DumpData(size_t indent) const {
  PrintIndented(indent, "pid %u, ppid %u, tid %u, ptid %u\n", data.pid, data.ppid, data.tid,
                data.ptid);
}

SampleRecord::SampleRecord(const perf_event_attr& attr, const perf_event_header* pheader)
    : Record(pheader) {
  const char* p = reinterpret_cast<const char*>(pheader + 1);
  const char* end = reinterpret_cast<const char*>(pheader) + pheader->size;
  sample_type = attr.sample_type;

  if (sample_type & PERF_SAMPLE_IP) {
    MoveFromBinaryFormat(ip_data, p);
  }
  if (sample_type & PERF_SAMPLE_TID) {
    MoveFromBinaryFormat(tid_data, p);
  }
  if (sample_type & PERF_SAMPLE_TIME) {
    MoveFromBinaryFormat(time_data, p);
  }
  if (sample_type & PERF_SAMPLE_ADDR) {
    MoveFromBinaryFormat(addr_data, p);
  }
  if (sample_type & PERF_SAMPLE_ID) {
    MoveFromBinaryFormat(id_data, p);
  }
  if (sample_type & PERF_SAMPLE_STREAM_ID) {
    MoveFromBinaryFormat(stream_id_data, p);
  }
  if (sample_type & PERF_SAMPLE_CPU) {
    MoveFromBinaryFormat(cpu_data, p);
  }
  if (sample_type & PERF_SAMPLE_PERIOD) {
    MoveFromBinaryFormat(period_data, p);
  }
  // TODO: Add parsing of other PERF_SAMPLE_*.
  CHECK_LE(p, end);
  if (p < end) {
    LOG(DEBUG) << "Record has " << end - p << " bytes left\n";
  }
}

void SampleRecord::DumpData(size_t indent) const {
  PrintIndented(indent, "sample_type: 0x%" PRIx64 "\n", sample_type);
  if (sample_type & PERF_SAMPLE_IP) {
    PrintIndented(indent, "ip %p\n", reinterpret_cast<void*>(ip_data.ip));
  }
  if (sample_type & PERF_SAMPLE_TID) {
    PrintIndented(indent, "pid %u, tid %u\n", tid_data.pid, tid_data.tid);
  }
  if (sample_type & PERF_SAMPLE_TIME) {
    PrintIndented(indent, "time %" PRId64 "\n", time_data.time);
  }
  if (sample_type & PERF_SAMPLE_ADDR) {
    PrintIndented(indent, "addr %p\n", reinterpret_cast<void*>(addr_data.addr));
  }
  if (sample_type & PERF_SAMPLE_ID) {
    PrintIndented(indent, "id %" PRId64 "\n", id_data.id);
  }
  if (sample_type & PERF_SAMPLE_STREAM_ID) {
    PrintIndented(indent, "stream_id %" PRId64 "\n", stream_id_data.stream_id);
  }
  if (sample_type & PERF_SAMPLE_CPU) {
    PrintIndented(indent, "cpu %u, res %u\n", cpu_data.cpu, cpu_data.res);
  }
  if (sample_type & PERF_SAMPLE_PERIOD) {
    PrintIndented(indent, "period %" PRId64 "\n", period_data.period);
  }
}

BuildIdRecord::BuildIdRecord(const perf_event_header* pheader) : Record(pheader) {
  const char* p = reinterpret_cast<const char*>(pheader + 1);
  const char* end = reinterpret_cast<const char*>(pheader) + pheader->size;
  MoveFromBinaryFormat(pid, p);
  std::copy_n(p, build_id.size(), build_id.begin());
  p += ALIGN(build_id.size(), 8);
  filename = p;
  p += ALIGN(filename.size() + 1, 64);
  CHECK_EQ(p, end);
}

void BuildIdRecord::DumpData(size_t indent) const {
  PrintIndented(indent, "pid %u\n", pid);
  PrintIndented(indent, "build_id 0x");
  for (auto& c : build_id) {
    printf("%02x", c);
  }
  printf("\n");
  PrintIndented(indent, "filename %s\n", filename.c_str());
}

std::vector<char> BuildIdRecord::BinaryFormat() const {
  std::vector<char> buf(header.size);
  char* p = buf.data();
  MoveToBinaryFormat(header, p);
  MoveToBinaryFormat(pid, p);
  memcpy(p, build_id.data(), build_id.size());
  p += ALIGN(build_id.size(), 8);
  strcpy(p, filename.c_str());
  p += ALIGN(filename.size() + 1, 64);
  return buf;
}

std::unique_ptr<const Record> ReadRecordFromBuffer(const perf_event_attr& attr,
                                                   const perf_event_header* pheader) {
  switch (pheader->type) {
    case PERF_RECORD_MMAP:
      return std::unique_ptr<const Record>(new MmapRecord(attr, pheader));
    case PERF_RECORD_COMM:
      return std::unique_ptr<const Record>(new CommRecord(attr, pheader));
    case PERF_RECORD_EXIT:
      return std::unique_ptr<const Record>(new ExitRecord(attr, pheader));
    case PERF_RECORD_SAMPLE:
      return std::unique_ptr<const Record>(new SampleRecord(attr, pheader));
    default:
      return std::unique_ptr<const Record>(new Record(pheader));
  }
}

MmapRecord CreateMmapRecord(const perf_event_attr& attr, bool in_kernel, uint32_t pid, uint32_t tid,
                            uint64_t addr, uint64_t len, uint64_t pgoff,
                            const std::string& filename) {
  MmapRecord record;
  record.header.type = PERF_RECORD_MMAP;
  record.header.misc = (in_kernel ? PERF_RECORD_MISC_KERNEL : PERF_RECORD_MISC_USER);
  record.data.pid = pid;
  record.data.tid = tid;
  record.data.addr = addr;
  record.data.len = len;
  record.data.pgoff = pgoff;
  record.filename = filename;
  size_t sample_id_size = record.sample_id.CreateContent(attr);
  record.header.size = sizeof(record.header) + sizeof(record.data) +
                       ALIGN(record.filename.size() + 1, 8) + sample_id_size;
  return record;
}

CommRecord CreateCommRecord(const perf_event_attr& attr, uint32_t pid, uint32_t tid,
                            const std::string& comm) {
  CommRecord record;
  record.header.type = PERF_RECORD_COMM;
  record.header.misc = 0;
  record.data.pid = pid;
  record.data.tid = tid;
  record.comm = comm;
  size_t sample_id_size = record.sample_id.CreateContent(attr);
  record.header.size = sizeof(record.header) + sizeof(record.data) +
                       ALIGN(record.comm.size() + 1, 8) + sample_id_size;
  return record;
}

BuildIdRecord CreateBuildIdRecord(bool in_kernel, pid_t pid, const BuildId& build_id,
                                  const std::string& filename) {
  BuildIdRecord record;
  record.header.type = PERF_RECORD_BUILD_ID;
  record.header.misc = (in_kernel ? PERF_RECORD_MISC_KERNEL : PERF_RECORD_MISC_USER);
  record.pid = pid;
  record.build_id = build_id;
  record.filename = filename;
  record.header.size = sizeof(record.header) + sizeof(record.pid) +
                       ALIGN(record.build_id.size(), 8) + ALIGN(filename.size() + 1, 64);
  return record;
}
