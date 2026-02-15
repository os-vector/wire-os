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

#ifndef SIMPLE_PERF_RECORD_H_
#define SIMPLE_PERF_RECORD_H_

#include <string>
#include <vector>

#include "build_id.h"
#include "perf_event.h"

struct KernelMmap;
struct ModuleMmap;
struct ThreadComm;
struct ThreadMmap;

enum user_record_type {
  PERF_RECORD_ATTR = 64,
  PERF_RECORD_EVENT_TYPE,
  PERF_RECORD_TRACING_DATA,
  PERF_RECORD_BUILD_ID,
  PERF_RECORD_FINISHED_ROUND,
};

struct PerfSampleIpType {
  uint64_t ip;
};

struct PerfSampleTidType {
  uint32_t pid, tid;
};

struct PerfSampleTimeType {
  uint64_t time;
};

struct PerfSampleAddrType {
  uint64_t addr;
};

struct PerfSampleIdType {
  uint64_t id;
};

struct PerfSampleStreamIdType {
  uint64_t stream_id;
};

struct PerfSampleCpuType {
  uint32_t cpu, res;
};

struct PerfSamplePeriodType {
  uint64_t period;
};

// SampleId is optional at the end of a record in binary format. Its content is determined by
// sample_id_all and sample_type in perf_event_attr. To avoid the complexity of referring to
// perf_event_attr each time, we copy sample_id_all and sample_type inside the SampleId structure.
struct SampleId {
  bool sample_id_all;
  uint64_t sample_type;

  PerfSampleTidType tid_data;             // Valid if sample_id_all && PERF_SAMPLE_TID.
  PerfSampleTimeType time_data;           // Valid if sample_id_all && PERF_SAMPLE_TIME.
  PerfSampleIdType id_data;               // Valid if sample_id_all && PERF_SAMPLE_ID.
  PerfSampleStreamIdType stream_id_data;  // Valid if sample_id_all && PERF_SAMPLE_STREAM_ID.
  PerfSampleCpuType cpu_data;             // Valid if sample_id_all && PERF_SAMPLE_CPU.

  SampleId();

  // Create the content of sample_id. It depends on the attr we use.
  size_t CreateContent(const perf_event_attr& attr);

  // Parse sample_id from binary format in the buffer pointed by p.
  void ReadFromBinaryFormat(const perf_event_attr& attr, const char* p, const char* end);

  // Write the binary format of sample_id to the buffer pointed by p.
  void WriteToBinaryFormat(char*& p) const;
  void Dump(size_t indent) const;
};

// Usually one record contains the following three parts in order in binary format:
//   perf_event_header (at the head of a record, containing type and size information)
//   data depends on the record type
//   sample_id (optional part at the end of a record)
// We hold the common parts (perf_event_header and sample_id) in the base class Record, and
// hold the type specific data part in classes derived from Record.
struct Record {
  perf_event_header header;
  SampleId sample_id;

  Record();
  Record(const perf_event_header* pheader);

  virtual ~Record() {
  }

  void Dump(size_t indent = 0) const;

 protected:
  virtual void DumpData(size_t) const {
  }
};

struct MmapRecord : public Record {
  struct MmapRecordDataType {
    uint32_t pid, tid;
    uint64_t addr;
    uint64_t len;
    uint64_t pgoff;
  } data;
  std::string filename;

  MmapRecord() {  // For storage in std::vector.
  }

  MmapRecord(const perf_event_attr& attr, const perf_event_header* pheader);
  std::vector<char> BinaryFormat() const;

 protected:
  void DumpData(size_t indent) const override;
};

struct CommRecord : public Record {
  struct CommRecordDataType {
    uint32_t pid, tid;
  } data;
  std::string comm;

  CommRecord() {
  }

  CommRecord(const perf_event_attr& attr, const perf_event_header* pheader);
  std::vector<char> BinaryFormat() const;

 protected:
  void DumpData(size_t indent) const override;
};

struct ExitRecord : public Record {
  struct ExitRecordDataType {
    uint32_t pid, ppid;
    uint32_t tid, ptid;
    uint64_t time;
  } data;

  ExitRecord(const perf_event_attr& attr, const perf_event_header* pheader);

 protected:
  void DumpData(size_t indent) const override;
};

struct SampleRecord : public Record {
  uint64_t sample_type;  // sample_type is a bit mask determining which fields below are valid.

  PerfSampleIpType ip_data;               // Valid if PERF_SAMPLE_IP.
  PerfSampleTidType tid_data;             // Valid if PERF_SAMPLE_TID.
  PerfSampleTimeType time_data;           // Valid if PERF_SAMPLE_TIME.
  PerfSampleAddrType addr_data;           // Valid if PERF_SAMPLE_ADDR.
  PerfSampleIdType id_data;               // Valid if PERF_SAMPLE_ID.
  PerfSampleStreamIdType stream_id_data;  // Valid if PERF_SAMPLE_STREAM_ID.
  PerfSampleCpuType cpu_data;             // Valid if PERF_SAMPLE_CPU.
  PerfSamplePeriodType period_data;       // Valid if PERF_SAMPLE_PERIOD.

  SampleRecord(const perf_event_attr& attr, const perf_event_header* pheader);

 protected:
  void DumpData(size_t indent) const override;
};

// BuildIdRecord is defined in user-space, stored in BuildId feature section in record file.
struct BuildIdRecord : public Record {
  uint32_t pid;
  BuildId build_id;
  std::string filename;

  BuildIdRecord() {
  }

  BuildIdRecord(const perf_event_header* pheader);
  std::vector<char> BinaryFormat() const;

 protected:
  void DumpData(size_t indent) const override;
};

std::unique_ptr<const Record> ReadRecordFromBuffer(const perf_event_attr& attr,
                                                   const perf_event_header* pheader);
MmapRecord CreateMmapRecord(const perf_event_attr& attr, bool in_kernel, uint32_t pid, uint32_t tid,
                            uint64_t addr, uint64_t len, uint64_t pgoff,
                            const std::string& filename);
CommRecord CreateCommRecord(const perf_event_attr& attr, uint32_t pid, uint32_t tid,
                            const std::string& comm);
BuildIdRecord CreateBuildIdRecord(bool in_kernel, pid_t pid, const BuildId& build_id,
                                  const std::string& filename);
#endif  // SIMPLE_PERF_RECORD_H_
