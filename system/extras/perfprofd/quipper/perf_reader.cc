// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "perf_reader.h"

#include <byteswap.h>
#include <limits.h>

#include <bitset>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

#define LOG_TAG "perf_reader"

#include "base/logging.h"

#include "quipper_string.h"
#include "perf_utils.h"

namespace quipper {

struct BufferWithSize {
  char* ptr;
  size_t size;
};

// If the buffer is read-only, it is not sufficient to mark the previous struct
// as const, as this only means that the pointer cannot be changed, and says
// nothing about the contents of the buffer.  So, we need another struct.
struct ConstBufferWithSize {
  const char* ptr;
  size_t size;
};

namespace {

// The type of the number of string data, found in the command line metadata in
// the perf data file.
typedef u32 num_string_data_type;

// Types of the event desc fields that are not found in other structs.
typedef u32 event_desc_num_events;
typedef u32 event_desc_attr_size;
typedef u32 event_desc_num_unique_ids;

// The type of the number of nodes field in NUMA topology.
typedef u32 numa_topology_num_nodes_type;

// A mask that is applied to metadata_mask_ in order to get a mask for
// only the metadata supported by quipper.
const uint32_t kSupportedMetadataMask =
    1 << HEADER_TRACING_DATA |
    1 << HEADER_BUILD_ID |
    1 << HEADER_HOSTNAME |
    1 << HEADER_OSRELEASE |
    1 << HEADER_VERSION |
    1 << HEADER_ARCH |
    1 << HEADER_NRCPUS |
    1 << HEADER_CPUDESC |
    1 << HEADER_CPUID |
    1 << HEADER_TOTAL_MEM |
    1 << HEADER_CMDLINE |
    1 << HEADER_EVENT_DESC |
    1 << HEADER_CPU_TOPOLOGY |
    1 << HEADER_NUMA_TOPOLOGY |
    1 << HEADER_BRANCH_STACK;

// By default, the build ID event has PID = -1.
const uint32_t kDefaultBuildIDEventPid = static_cast<uint32_t>(-1);

template <class T>
void ByteSwap(T* input) {
  switch (sizeof(T)) {
  case sizeof(uint8_t):
    LOG(WARNING) << "Attempting to byte swap on a single byte.";
    break;
  case sizeof(uint16_t):
    *input = bswap_16(*input);
    break;
  case sizeof(uint32_t):
    *input = bswap_32(*input);
    break;
  case sizeof(uint64_t):
    *input = bswap_64(*input);
    break;
  default:
    LOG(FATAL) << "Invalid size for byte swap: " << sizeof(T) << " bytes";
    break;
  }
}

u64 MaybeSwap(u64 value, bool swap) {
  if (swap)
    return bswap_64(value);
  return value;
}

u32 MaybeSwap(u32 value, bool swap) {
  if (swap)
    return bswap_32(value);
  return value;
}

u8 ReverseByte(u8 x) {
  x = (x & 0xf0) >> 4 | (x & 0x0f) << 4;  // exchange nibbles
  x = (x & 0xcc) >> 2 | (x & 0x33) << 2;  // exchange pairs
  x = (x & 0xaa) >> 1 | (x & 0x55) << 1;  // exchange neighbors
  return x;
}

// If field points to the start of a bitfield padded to len bytes, this
// performs an endian swap of the bitfield, assuming the compiler that produced
// it conforms to the same ABI (bitfield layout is not completely specified by
// the language).
void SwapBitfieldOfBits(u8* field, size_t len) {
  for (size_t i = 0; i < len; i++) {
    field[i] = ReverseByte(field[i]);
  }
}

// The code currently assumes that the compiler will not add any padding to the
// various structs.  These CHECKs make sure that this is true.
void CheckNoEventHeaderPadding() {
  perf_event_header header;
  CHECK_EQ(sizeof(header),
           sizeof(header.type) + sizeof(header.misc) + sizeof(header.size));
}

void CheckNoPerfEventAttrPadding() {
  perf_event_attr attr;
  CHECK_EQ(sizeof(attr),
           (reinterpret_cast<u64>(&attr.__reserved_2) -
            reinterpret_cast<u64>(&attr)) +
           sizeof(attr.__reserved_2));
}

void CheckNoEventTypePadding() {
  perf_trace_event_type event_type;
  CHECK_EQ(sizeof(event_type),
           sizeof(event_type.event_id) + sizeof(event_type.name));
}

void CheckNoBuildIDEventPadding() {
  build_id_event event;
  CHECK_EQ(sizeof(event),
           sizeof(event.header.type) + sizeof(event.header.misc) +
           sizeof(event.header.size) + sizeof(event.pid) +
           sizeof(event.build_id));
}

// Creates/updates a build id event with |build_id| and |filename|.
// Passing "" to |build_id| or |filename| will leave the corresponding field
// unchanged (in which case |event| must be non-null).
// If |event| is null or is not large enough, a new event will be created.
// In this case, if |event| is non-null, it will be freed.
// Otherwise, updates the fields of the existing event.
// |new_misc| indicates kernel vs user space, and is only used to fill in the
// |header.misc| field of new events.
// In either case, returns a pointer to the event containing the updated data,
// or NULL in the case of a failure.
build_id_event* CreateOrUpdateBuildID(const string& build_id,
                                      const string& filename,
                                      uint16_t new_misc,
                                      build_id_event* event) {
  // When creating an event from scratch, build id and filename must be present.
  if (!event && (build_id.empty() || filename.empty()))
    return NULL;
  size_t new_len = GetUint64AlignedStringLength(
      filename.empty() ? event->filename : filename);

  // If event is null, or we don't have enough memory, allocate more memory, and
  // switch the new pointer with the existing pointer.
  size_t new_size = sizeof(*event) + new_len;
  if (!event || new_size > event->header.size) {
    build_id_event* new_event = CallocMemoryForBuildID(new_size);

    if (event) {
      // Copy over everything except the filename and free the event.
      // It is guaranteed that we are changing the filename - otherwise, the old
      // size and the new size would be equal.
      *new_event = *event;
      free(event);
    } else {
      // Fill in the fields appropriately.
      new_event->header.type = HEADER_BUILD_ID;
      new_event->header.misc = new_misc;
      new_event->pid = kDefaultBuildIDEventPid;
    }
    event = new_event;
  }

  // Here, event is the pointer to the build_id_event that we are keeping.
  // Update the event's size, build id, and filename.
  if (!build_id.empty() &&
      !StringToHex(build_id, event->build_id, arraysize(event->build_id))) {
    free(event);
    return NULL;
  }

  if (!filename.empty())
    CHECK_GT(snprintf(event->filename, new_len, "%s", filename.c_str()), 0);

  event->header.size = new_size;
  return event;
}

// Reads |size| bytes from |buffer| into |dest| and advances |src_offset|.
bool ReadDataFromBuffer(const ConstBufferWithSize& buffer,
                        size_t size,
                        const string& value_name,
                        size_t* src_offset,
                        void* dest) {
  size_t end_offset = *src_offset + size / sizeof(*buffer.ptr);
  if (buffer.size < end_offset) {
    LOG(ERROR) << "Not enough bytes to read " << value_name
               << ". Requested " << size << " bytes";
    return false;
  }
  memcpy(dest, buffer.ptr + *src_offset, size);
  *src_offset = end_offset;
  return true;
}

// Reads a CStringWithLength from |buffer| into |dest|, and advances the offset.
bool ReadStringFromBuffer(const ConstBufferWithSize& buffer,
                          bool is_cross_endian,
                          size_t* offset,
                          CStringWithLength* dest) {
  if (!ReadDataFromBuffer(buffer, sizeof(dest->len), "string length",
                          offset, &dest->len)) {
    return false;
  }
  if (is_cross_endian)
    ByteSwap(&dest->len);

  if (buffer.size < *offset + dest->len) {
    LOG(ERROR) << "Not enough bytes to read string";
    return false;
  }
  dest->str = string(buffer.ptr + *offset);
  *offset += dest->len / sizeof(*buffer.ptr);
  return true;
}

// Read read info from perf data.  Corresponds to sample format type
// PERF_SAMPLE_READ.
const uint64_t* ReadReadInfo(const uint64_t* array,
                           bool swap_bytes,
                           uint64_t read_format,
                           struct perf_sample* sample) {
  if (read_format & PERF_FORMAT_TOTAL_TIME_ENABLED)
    sample->read.time_enabled = *array++;
  if (read_format & PERF_FORMAT_TOTAL_TIME_RUNNING)
    sample->read.time_running = *array++;
  if (read_format & PERF_FORMAT_ID)
    sample->read.one.id = *array++;

  if (swap_bytes) {
    ByteSwap(&sample->read.time_enabled);
    ByteSwap(&sample->read.time_running);
    ByteSwap(&sample->read.one.id);
  }

  return array;
}

// Read call chain info from perf data.  Corresponds to sample format type
// PERF_SAMPLE_CALLCHAIN.
const uint64_t* ReadCallchain(const uint64_t* array,
                            bool swap_bytes,
                            struct perf_sample* sample) {
  // Make sure there is no existing allocated memory in |sample->callchain|.
  CHECK_EQ(static_cast<void*>(NULL), sample->callchain);

  // The callgraph data consists of a uint64_t value |nr| followed by |nr|
  // addresses.
  uint64_t callchain_size = *array++;
  if (swap_bytes)
    ByteSwap(&callchain_size);
  struct ip_callchain* callchain =
      reinterpret_cast<struct ip_callchain*>(new uint64_t[callchain_size + 1]);
  callchain->nr = callchain_size;
  for (size_t i = 0; i < callchain_size; ++i) {
    callchain->ips[i] = *array++;
    if (swap_bytes)
      ByteSwap(&callchain->ips[i]);
  }
  sample->callchain = callchain;

  return array;
}

// Read raw info from perf data.  Corresponds to sample format type
// PERF_SAMPLE_RAW.
const uint64_t* ReadRawData(const uint64_t* array,
                          bool swap_bytes,
                          struct perf_sample* sample) {
  // First read the size.
  const uint32_t* ptr = reinterpret_cast<const uint32_t*>(array);
  sample->raw_size = *ptr++;
  if (swap_bytes)
    ByteSwap(&sample->raw_size);

  // Allocate space for and read the raw data bytes.
  sample->raw_data = new uint8_t[sample->raw_size];
  memcpy(sample->raw_data, ptr, sample->raw_size);

  // Determine the bytes that were read, and align to the next 64 bits.
  int bytes_read = AlignSize(sizeof(sample->raw_size) + sample->raw_size,
                             sizeof(uint64_t));
  array += bytes_read / sizeof(uint64_t);

  return array;
}

// Read call chain info from perf data.  Corresponds to sample format type
// PERF_SAMPLE_CALLCHAIN.
const uint64_t* ReadBranchStack(const uint64_t* array,
                              bool swap_bytes,
                              struct perf_sample* sample) {
  // Make sure there is no existing allocated memory in
  // |sample->branch_stack|.
  CHECK_EQ(static_cast<void*>(NULL), sample->branch_stack);

  // The branch stack data consists of a uint64_t value |nr| followed by |nr|
  // branch_entry structs.
  uint64_t branch_stack_size = *array++;
  if (swap_bytes)
    ByteSwap(&branch_stack_size);
  struct branch_stack* branch_stack =
      reinterpret_cast<struct branch_stack*>(
          new uint8_t[sizeof(uint64_t) +
                    branch_stack_size * sizeof(struct branch_entry)]);
  branch_stack->nr = branch_stack_size;
  for (size_t i = 0; i < branch_stack_size; ++i) {
    memcpy(&branch_stack->entries[i], array, sizeof(struct branch_entry));
    array += sizeof(struct branch_entry) / sizeof(*array);
    if (swap_bytes) {
      ByteSwap(&branch_stack->entries[i].from);
      ByteSwap(&branch_stack->entries[i].to);
    }
  }
  sample->branch_stack = branch_stack;

  return array;
}

size_t ReadPerfSampleFromData(const perf_event_type event_type,
                              const uint64_t* array,
                              const uint64_t sample_fields,
                              const uint64_t read_format,
                              bool swap_bytes,
                              struct perf_sample* sample) {
  const uint64_t* initial_array_ptr = array;

  union {
    uint32_t val32[sizeof(uint64_t) / sizeof(uint32_t)];
    uint64_t val64;
  };

  // See structure for PERF_RECORD_SAMPLE in kernel/perf_event.h
  // and compare sample_id when sample_id_all is set.

  // NB: For sample_id, sample_fields has already been masked to the set
  // of fields in that struct by GetSampleFieldsForEventType. That set
  // of fields is mostly in the same order as PERF_RECORD_SAMPLE, with
  // the exception of PERF_SAMPLE_IDENTIFIER.

  // PERF_SAMPLE_IDENTIFIER is in a different location depending on
  // if this is a SAMPLE event or the sample_id of another event.
  if (event_type == PERF_RECORD_SAMPLE) {
    // { u64                   id;       } && PERF_SAMPLE_IDENTIFIER
    if (sample_fields & PERF_SAMPLE_IDENTIFIER) {
      sample->id = MaybeSwap(*array++, swap_bytes);
    }
  }

  // { u64                   ip;       } && PERF_SAMPLE_IP
  if (sample_fields & PERF_SAMPLE_IP) {
    sample->ip = MaybeSwap(*array++, swap_bytes);
  }

  // { u32                   pid, tid; } && PERF_SAMPLE_TID
  if (sample_fields & PERF_SAMPLE_TID) {
    val64 = *array++;
    sample->pid = MaybeSwap(val32[0], swap_bytes);
    sample->tid = MaybeSwap(val32[1], swap_bytes);
  }

  // { u64                   time;     } && PERF_SAMPLE_TIME
  if (sample_fields & PERF_SAMPLE_TIME) {
    sample->time = MaybeSwap(*array++, swap_bytes);
  }

  // { u64                   addr;     } && PERF_SAMPLE_ADDR
  if (sample_fields & PERF_SAMPLE_ADDR) {
    sample->addr = MaybeSwap(*array++, swap_bytes);
  }

  // { u64                   id;       } && PERF_SAMPLE_ID
  if (sample_fields & PERF_SAMPLE_ID) {
    sample->id = MaybeSwap(*array++, swap_bytes);
  }

  // { u64                   stream_id;} && PERF_SAMPLE_STREAM_ID
  if (sample_fields & PERF_SAMPLE_STREAM_ID) {
    sample->stream_id = MaybeSwap(*array++, swap_bytes);
  }

  // { u32                   cpu, res; } && PERF_SAMPLE_CPU
  if (sample_fields & PERF_SAMPLE_CPU) {
    val64 = *array++;
    sample->cpu = MaybeSwap(val32[0], swap_bytes);
    // sample->res = MaybeSwap(*val32[1], swap_bytes);  // not implemented?
  }

  // This is the location of PERF_SAMPLE_IDENTIFIER in struct sample_id.
  if (event_type != PERF_RECORD_SAMPLE) {
    // { u64                   id;       } && PERF_SAMPLE_IDENTIFIER
    if (sample_fields & PERF_SAMPLE_IDENTIFIER) {
      sample->id = MaybeSwap(*array++, swap_bytes);
    }
  }

  //
  // The remaining fields are only in PERF_RECORD_SAMPLE
  //

  // { u64                   period;   } && PERF_SAMPLE_PERIOD
  if (sample_fields & PERF_SAMPLE_PERIOD) {
    sample->period = MaybeSwap(*array++, swap_bytes);
  }

  // { struct read_format    values;   } && PERF_SAMPLE_READ
  if (sample_fields & PERF_SAMPLE_READ) {
    // TODO(cwp-team): support grouped read info.
    if (read_format & PERF_FORMAT_GROUP)
      return 0;
    array = ReadReadInfo(array, swap_bytes, read_format, sample);
  }

  // { u64                   nr,
  //   u64                   ips[nr];  } && PERF_SAMPLE_CALLCHAIN
  if (sample_fields & PERF_SAMPLE_CALLCHAIN) {
    array = ReadCallchain(array, swap_bytes, sample);
  }

  // { u32                   size;
  //   char                  data[size];}&& PERF_SAMPLE_RAW
  if (sample_fields & PERF_SAMPLE_RAW) {
    array = ReadRawData(array, swap_bytes, sample);
  }

  // { u64                   nr;
  //   { u64 from, to, flags } lbr[nr];} && PERF_SAMPLE_BRANCH_STACK
  if (sample_fields & PERF_SAMPLE_BRANCH_STACK) {
    array = ReadBranchStack(array, swap_bytes, sample);
  }

  static const u64 kUnimplementedSampleFields =
      PERF_SAMPLE_REGS_USER  |
      PERF_SAMPLE_STACK_USER |
      PERF_SAMPLE_WEIGHT     |
      PERF_SAMPLE_DATA_SRC   |
      PERF_SAMPLE_TRANSACTION;

  if (sample_fields & kUnimplementedSampleFields) {
    LOG(WARNING) << "Unimplemented sample fields 0x"
                 << std::hex << (sample_fields & kUnimplementedSampleFields);
  }

  if (sample_fields & ~(PERF_SAMPLE_MAX-1)) {
    LOG(WARNING) << "Unrecognized sample fields 0x"
                 << std::hex << (sample_fields & ~(PERF_SAMPLE_MAX-1));
  }

  return (array - initial_array_ptr) * sizeof(uint64_t);
}

size_t WritePerfSampleToData(const perf_event_type event_type,
                             const struct perf_sample& sample,
                             const uint64_t sample_fields,
                             const uint64_t read_format,
                             uint64_t* array) {
  const uint64_t* initial_array_ptr = array;

  union {
    uint32_t val32[sizeof(uint64_t) / sizeof(uint32_t)];
    uint64_t val64;
  };

  // See notes at the top of ReadPerfSampleFromData regarding the structure
  // of PERF_RECORD_SAMPLE, sample_id, and PERF_SAMPLE_IDENTIFIER, as they
  // all apply here as well.

  // PERF_SAMPLE_IDENTIFIER is in a different location depending on
  // if this is a SAMPLE event or the sample_id of another event.
  if (event_type == PERF_RECORD_SAMPLE) {
    // { u64                   id;       } && PERF_SAMPLE_IDENTIFIER
    if (sample_fields & PERF_SAMPLE_IDENTIFIER) {
      *array++ = sample.id;
    }
  }

  // { u64                   ip;       } && PERF_SAMPLE_IP
  if (sample_fields & PERF_SAMPLE_IP) {
    *array++ = sample.ip;
  }

  // { u32                   pid, tid; } && PERF_SAMPLE_TID
  if (sample_fields & PERF_SAMPLE_TID) {
    val32[0] = sample.pid;
    val32[1] = sample.tid;
    *array++ = val64;
  }

  // { u64                   time;     } && PERF_SAMPLE_TIME
  if (sample_fields & PERF_SAMPLE_TIME) {
    *array++ = sample.time;
  }

  // { u64                   addr;     } && PERF_SAMPLE_ADDR
  if (sample_fields & PERF_SAMPLE_ADDR) {
    *array++ = sample.addr;
  }

  // { u64                   id;       } && PERF_SAMPLE_ID
  if (sample_fields & PERF_SAMPLE_ID) {
    *array++ = sample.id;
  }

  // { u64                   stream_id;} && PERF_SAMPLE_STREAM_ID
  if (sample_fields & PERF_SAMPLE_STREAM_ID) {
    *array++ = sample.stream_id;
  }

  // { u32                   cpu, res; } && PERF_SAMPLE_CPU
  if (sample_fields & PERF_SAMPLE_CPU) {
    val32[0] = sample.cpu;
    // val32[1] = sample.res;  // not implemented?
    val32[1] = 0;
    *array++ = val64;
  }

  // This is the location of PERF_SAMPLE_IDENTIFIER in struct sample_id.
  if (event_type != PERF_RECORD_SAMPLE) {
    // { u64                   id;       } && PERF_SAMPLE_IDENTIFIER
    if (sample_fields & PERF_SAMPLE_IDENTIFIER) {
      *array++ = sample.id;
    }
  }

  //
  // The remaining fields are only in PERF_RECORD_SAMPLE
  //

  // { u64                   period;   } && PERF_SAMPLE_PERIOD
  if (sample_fields & PERF_SAMPLE_PERIOD) {
    *array++ = sample.period;
  }

  // { struct read_format    values;   } && PERF_SAMPLE_READ
  if (sample_fields & PERF_SAMPLE_READ) {
    // TODO(cwp-team): support grouped read info.
    if (read_format & PERF_FORMAT_GROUP)
      return 0;
    if (read_format & PERF_FORMAT_TOTAL_TIME_ENABLED)
      *array++ = sample.read.time_enabled;
    if (read_format & PERF_FORMAT_TOTAL_TIME_RUNNING)
      *array++ = sample.read.time_running;
    if (read_format & PERF_FORMAT_ID)
      *array++ = sample.read.one.id;
  }

  // { u64                   nr,
  //   u64                   ips[nr];  } && PERF_SAMPLE_CALLCHAIN
  if (sample_fields & PERF_SAMPLE_CALLCHAIN) {
    if (!sample.callchain) {
      LOG(ERROR) << "Expecting callchain data, but none was found.";
    } else {
      *array++ = sample.callchain->nr;
      for (size_t i = 0; i < sample.callchain->nr; ++i)
        *array++ = sample.callchain->ips[i];
    }
  }

  // { u32                   size;
  //   char                  data[size];}&& PERF_SAMPLE_RAW
  if (sample_fields & PERF_SAMPLE_RAW) {
    uint32_t* ptr = reinterpret_cast<uint32_t*>(array);
    *ptr++ = sample.raw_size;
    memcpy(ptr, sample.raw_data, sample.raw_size);

    // Update the data read pointer after aligning to the next 64 bytes.
    int num_bytes = AlignSize(sizeof(sample.raw_size) + sample.raw_size,
                              sizeof(uint64_t));
    array += num_bytes / sizeof(uint64_t);
  }

  // { u64                   nr;
  //   { u64 from, to, flags } lbr[nr];} && PERF_SAMPLE_BRANCH_STACK
  if (sample_fields & PERF_SAMPLE_BRANCH_STACK) {
    if (!sample.branch_stack) {
      LOG(ERROR) << "Expecting branch stack data, but none was found.";
    } else {
      *array++ = sample.branch_stack->nr;
      for (size_t i = 0; i < sample.branch_stack->nr; ++i) {
        *array++ = sample.branch_stack->entries[i].from;
        *array++ = sample.branch_stack->entries[i].to;
        memcpy(array++, &sample.branch_stack->entries[i].flags,
               sizeof(uint64_t));
      }
    }
  }

  return (array - initial_array_ptr) * sizeof(uint64_t);
}

}  // namespace

PerfReader::~PerfReader() {
  // Free allocated memory.
  for (size_t i = 0; i < build_id_events_.size(); ++i)
    if (build_id_events_[i])
      free(build_id_events_[i]);
}

void PerfReader::PerfizeBuildIDString(string* build_id) {
  build_id->resize(kBuildIDStringLength, '0');
}

void PerfReader::UnperfizeBuildIDString(string* build_id) {
  const size_t kPaddingSize = 8;
  const string kBuildIDPadding = string(kPaddingSize, '0');

  // Remove kBuildIDPadding from the end of build_id until we cannot remove any
  // more, or removing more would cause the build id to be empty.
  while (build_id->size() > kPaddingSize &&
         build_id->substr(build_id->size() - kPaddingSize) == kBuildIDPadding) {
    build_id->resize(build_id->size() - kPaddingSize);
  }
}

bool PerfReader::ReadFile(const string& filename) {
  std::vector<char> data;
  if (!ReadFileToData(filename, &data))
    return false;
  return ReadFromVector(data);
}

bool PerfReader::ReadFromVector(const std::vector<char>& data) {
  return ReadFromPointer(&data[0], data.size());
}

bool PerfReader::ReadFromString(const string& str) {
  return ReadFromPointer(str.c_str(), str.size());
}

bool PerfReader::ReadFromPointer(const char* perf_data, size_t size) {
  const ConstBufferWithSize data = { perf_data, size };

  if (data.size == 0)
    return false;
  if (!ReadHeader(data))
    return false;

  // Check if it is normal perf data.
  if (header_.size == sizeof(header_)) {
    DLOG(INFO) << "Perf data is in normal format.";
    metadata_mask_ = header_.adds_features[0];
    return (ReadAttrs(data) && ReadEventTypes(data) && ReadData(data)
            && ReadMetadata(data));
  }

  // Otherwise it is piped data.
  LOG(ERROR) << "Internal error: no support for piped data";
  return false;
}

bool PerfReader::Localize(
    const std::map<string, string>& build_ids_to_filenames) {
  std::map<string, string> perfized_build_ids_to_filenames;
  std::map<string, string>::const_iterator it;
  for (it = build_ids_to_filenames.begin();
       it != build_ids_to_filenames.end();
       ++it) {
    string build_id = it->first;
    PerfizeBuildIDString(&build_id);
    perfized_build_ids_to_filenames[build_id] = it->second;
  }

  std::map<string, string> filename_map;
  for (size_t i = 0; i < build_id_events_.size(); ++i) {
    build_id_event* event = build_id_events_[i];
    string build_id = HexToString(event->build_id, kBuildIDArraySize);
    if (perfized_build_ids_to_filenames.find(build_id) ==
        perfized_build_ids_to_filenames.end()) {
      continue;
    }

    string new_name = perfized_build_ids_to_filenames.at(build_id);
    filename_map[string(event->filename)] = new_name;
    build_id_event* new_event = CreateOrUpdateBuildID("", new_name, 0, event);
    CHECK(new_event);
    build_id_events_[i] = new_event;
  }

  LocalizeUsingFilenames(filename_map);
  return true;
}

bool PerfReader::LocalizeUsingFilenames(
    const std::map<string, string>& filename_map) {
  LocalizeMMapFilenames(filename_map);
  for (size_t i = 0; i < build_id_events_.size(); ++i) {
    build_id_event* event = build_id_events_[i];
    string old_name = event->filename;

    if (filename_map.find(event->filename) != filename_map.end()) {
      const string& new_name = filename_map.at(old_name);
      build_id_event* new_event = CreateOrUpdateBuildID("", new_name, 0, event);
      CHECK(new_event);
      build_id_events_[i] = new_event;
    }
  }
  return true;
}

void PerfReader::GetFilenames(std::vector<string>* filenames) const {
  std::set<string> filename_set;
  GetFilenamesAsSet(&filename_set);
  filenames->clear();
  filenames->insert(filenames->begin(), filename_set.begin(),
                    filename_set.end());
}

void PerfReader::GetFilenamesAsSet(std::set<string>* filenames) const {
  filenames->clear();
  for (size_t i = 0; i < events_.size(); ++i) {
    const event_t& event = *events_[i];
    if (event.header.type == PERF_RECORD_MMAP)
      filenames->insert(event.mmap.filename);
    if (event.header.type == PERF_RECORD_MMAP2)
      filenames->insert(event.mmap2.filename);
  }
}

void PerfReader::GetFilenamesToBuildIDs(
    std::map<string, string>* filenames_to_build_ids) const {
  filenames_to_build_ids->clear();
  for (size_t i = 0; i < build_id_events_.size(); ++i) {
    const build_id_event& event = *build_id_events_[i];
    string build_id = HexToString(event.build_id, kBuildIDArraySize);
    (*filenames_to_build_ids)[event.filename] = build_id;
  }
}

bool PerfReader::IsSupportedEventType(uint32_t type) {
  switch (type) {
  case PERF_RECORD_SAMPLE:
  case PERF_RECORD_MMAP:
  case PERF_RECORD_MMAP2:
  case PERF_RECORD_FORK:
  case PERF_RECORD_EXIT:
  case PERF_RECORD_COMM:
  case PERF_RECORD_LOST:
  case PERF_RECORD_THROTTLE:
  case PERF_RECORD_UNTHROTTLE:
    return true;
  case PERF_RECORD_READ:
  case PERF_RECORD_MAX:
    return false;
  default:
    LOG(FATAL) << "Unknown event type " << type;
    return false;
  }
}

bool PerfReader::ReadPerfSampleInfo(const event_t& event,
                                    struct perf_sample* sample) const {
  CHECK(sample);

  if (!IsSupportedEventType(event.header.type)) {
    LOG(ERROR) << "Unsupported event type " << event.header.type;
    return false;
  }

  uint64_t sample_format = GetSampleFieldsForEventType(event.header.type,
                                                       sample_type_);
  uint64_t offset = GetPerfSampleDataOffset(event);
  size_t size_read = ReadPerfSampleFromData(
      static_cast<perf_event_type>(event.header.type),
      reinterpret_cast<const uint64_t*>(&event) + offset / sizeof(uint64_t),
      sample_format,
      read_format_,
      is_cross_endian_,
      sample);

  size_t expected_size = event.header.size - offset;
  if (size_read != expected_size) {
    LOG(ERROR) << "Read " << size_read << " bytes, expected "
               << expected_size << " bytes.";
  }

  return (size_read == expected_size);
}

bool PerfReader::WritePerfSampleInfo(const perf_sample& sample,
                                     event_t* event) const {
  CHECK(event);

  if (!IsSupportedEventType(event->header.type)) {
    LOG(ERROR) << "Unsupported event type " << event->header.type;
    return false;
  }

  uint64_t sample_format = GetSampleFieldsForEventType(event->header.type,
                                                       sample_type_);
  uint64_t offset = GetPerfSampleDataOffset(*event);

  size_t expected_size = event->header.size - offset;
  memset(reinterpret_cast<uint8_t*>(event) + offset, 0, expected_size);
  size_t size_written = WritePerfSampleToData(
      static_cast<perf_event_type>(event->header.type),
      sample,
      sample_format,
      read_format_,
      reinterpret_cast<uint64_t*>(event) + offset / sizeof(uint64_t));
  if (size_written != expected_size) {
    LOG(ERROR) << "Wrote " << size_written << " bytes, expected "
               << expected_size << " bytes.";
  }

  return (size_written == expected_size);
}

bool PerfReader::ReadHeader(const ConstBufferWithSize& data) {
  CheckNoEventHeaderPadding();
  size_t offset = 0;
  if (!ReadDataFromBuffer(data, sizeof(piped_header_), "header magic",
                          &offset, &piped_header_)) {
    return false;
  }
  if (piped_header_.magic != kPerfMagic &&
      piped_header_.magic != bswap_64(kPerfMagic)) {
    LOG(ERROR) << "Read wrong magic. Expected: 0x" << std::hex << kPerfMagic
               << " or 0x" << std::hex << bswap_64(kPerfMagic)
               << " Got: 0x" << std::hex << piped_header_.magic;
    return false;
  }
  is_cross_endian_ = (piped_header_.magic != kPerfMagic);
  if (is_cross_endian_)
    ByteSwap(&piped_header_.size);

  // Header can be a piped header.
  if (piped_header_.size == sizeof(piped_header_))
    return true;

  // Re-read full header
  offset = 0;
  if (!ReadDataFromBuffer(data, sizeof(header_), "header data",
                          &offset, &header_)) {
    return false;
  }
  if (is_cross_endian_)
    ByteSwap(&header_.size);

  DLOG(INFO) << "event_types.size: " << header_.event_types.size;
  DLOG(INFO) << "event_types.offset: " << header_.event_types.offset;

  return true;
}

bool PerfReader::ReadAttrs(const ConstBufferWithSize& data) {
  size_t num_attrs = header_.attrs.size / header_.attr_size;
  size_t offset = header_.attrs.offset;
  for (size_t i = 0; i < num_attrs; i++) {
    if (!ReadAttr(data, &offset))
      return false;
  }
  return true;
}

bool PerfReader::ReadAttr(const ConstBufferWithSize& data, size_t* offset) {
  PerfFileAttr attr;
  if (!ReadEventAttr(data, offset, &attr.attr))
    return false;

  perf_file_section ids;
  if (!ReadDataFromBuffer(data, sizeof(ids), "ID section info", offset, &ids))
    return false;
  if (is_cross_endian_) {
    ByteSwap(&ids.offset);
    ByteSwap(&ids.size);
  }

  size_t num_ids = ids.size / sizeof(decltype(attr.ids)::value_type);
  // Convert the offset from u64 to size_t.
  size_t ids_offset = ids.offset;
  if (!ReadUniqueIDs(data, num_ids, &ids_offset, &attr.ids))
    return false;
  attrs_.push_back(attr);
  return true;
}

u32 PerfReader::ReadPerfEventAttrSize(const ConstBufferWithSize& data,
                                      size_t attr_offset) {
  static_assert(std::is_same<decltype(perf_event_attr::size), u32>::value,
                "ReadPerfEventAttrSize return type should match "
                "perf_event_attr.size");
  u32 attr_size;
  size_t attr_size_offset = attr_offset + offsetof(perf_event_attr, size);
  if (!ReadDataFromBuffer(data, sizeof(perf_event_attr::size),
                          "attr.size", &attr_size_offset, &attr_size)) {
    return kuint32max;
  }
  return MaybeSwap(attr_size, is_cross_endian_);
}

bool PerfReader::ReadEventAttr(const ConstBufferWithSize& data, size_t* offset,
                               perf_event_attr* attr) {
  CheckNoPerfEventAttrPadding();

  std::memset(attr, 0, sizeof(*attr));
  //*attr = {0};

  // read just size first
  u32 attr_size = ReadPerfEventAttrSize(data, *offset);
  if (attr_size == kuint32max) {
    return false;
  }

  // now read the the struct.
  if (!ReadDataFromBuffer(data, attr_size, "attribute", offset,
                          reinterpret_cast<char*>(attr))) {
    return false;
  }

  if (is_cross_endian_) {
    // Depending on attr->size, some of these might not have actually been
    // read. This is okay: they are zero.
    ByteSwap(&attr->type);
    ByteSwap(&attr->size);
    ByteSwap(&attr->config);
    ByteSwap(&attr->sample_period);
    ByteSwap(&attr->sample_type);
    ByteSwap(&attr->read_format);

    // NB: This will also reverse precise_ip : 2 as if it was two fields:
    auto *const bitfield_start = &attr->read_format + 1;
    SwapBitfieldOfBits(reinterpret_cast<u8*>(bitfield_start),
                       sizeof(u64));
    // ... So swap it back:
    const auto tmp = attr->precise_ip;
    attr->precise_ip = (tmp & 0x2) >> 1 | (tmp & 0x1) << 1;

    ByteSwap(&attr->wakeup_events);  // union with wakeup_watermark
    ByteSwap(&attr->bp_type);
    ByteSwap(&attr->bp_addr);        // union with config1
    ByteSwap(&attr->bp_len);         // union with config2
    ByteSwap(&attr->branch_sample_type);
    ByteSwap(&attr->sample_regs_user);
    ByteSwap(&attr->sample_stack_user);
  }

  CHECK_EQ(attr_size, attr->size);
  // The actual perf_event_attr data size might be different from the size of
  // the struct definition.  Check against perf_event_attr's |size| field.
  attr->size = sizeof(*attr);

  // Assign sample type if it hasn't been assigned, otherwise make sure all
  // subsequent attributes have the same sample type bits set.
  if (sample_type_ == 0) {
    sample_type_ = attr->sample_type;
  } else {
    CHECK_EQ(sample_type_, attr->sample_type)
        << "Event type sample format does not match sample format of other "
        << "event type.";
  }

  if (read_format_ == 0) {
    read_format_ = attr->read_format;
  } else {
    CHECK_EQ(read_format_, attr->read_format)
        << "Event type read format does not match read format of other event "
        << "types.";
  }

  return true;
}

bool PerfReader::ReadUniqueIDs(const ConstBufferWithSize& data, size_t num_ids,
                               size_t* offset, std::vector<u64>* ids) {
  ids->resize(num_ids);
  for (size_t j = 0; j < num_ids; j++) {
    if (!ReadDataFromBuffer(data, sizeof(ids->at(j)), "ID", offset,
                            &ids->at(j))) {
      return false;
    }
    if (is_cross_endian_)
      ByteSwap(&ids->at(j));
  }
  return true;
}

bool PerfReader::ReadEventTypes(const ConstBufferWithSize& data) {
  size_t num_event_types = header_.event_types.size /
      sizeof(struct perf_trace_event_type);
  CHECK_EQ(sizeof(perf_trace_event_type) * num_event_types,
           header_.event_types.size);
  size_t offset = header_.event_types.offset;
  for (size_t i = 0; i < num_event_types; ++i) {
    if (!ReadEventType(data, &offset))
      return false;
  }
  return true;
}

bool PerfReader::ReadEventType(const ConstBufferWithSize& data,
                               size_t* offset) {
  CheckNoEventTypePadding();
  perf_trace_event_type type;
  memset(&type, 0, sizeof(type));
  if (!ReadDataFromBuffer(data, sizeof(type.event_id), "event id",
                          offset, &type.event_id)) {
    return false;
  }
  const char* event_name = reinterpret_cast<const char*>(data.ptr + *offset);
  CHECK_GT(snprintf(type.name, sizeof(type.name), "%s", event_name), 0);
  *offset += sizeof(type.name);
  event_types_.push_back(type);
  return true;
}

bool PerfReader::ReadData(const ConstBufferWithSize& data) {
  u64 data_remaining_bytes = header_.data.size;
  size_t offset = header_.data.offset;
  while (data_remaining_bytes != 0) {
    if (data.size < offset) {
      LOG(ERROR) << "Not enough data to read a perf event.";
      return false;
    }

    const event_t* event = reinterpret_cast<const event_t*>(data.ptr + offset);
    if (!ReadPerfEventBlock(*event))
      return false;
    data_remaining_bytes -= event->header.size;
    offset += event->header.size;
  }

  DLOG(INFO) << "Number of events stored: "<< events_.size();
  return true;
}

bool PerfReader::ReadMetadata(const ConstBufferWithSize& data) {
  size_t offset = header_.data.offset + header_.data.size;

  for (u32 type = HEADER_FIRST_FEATURE; type != HEADER_LAST_FEATURE; ++type) {
    if ((metadata_mask_ & (1 << type)) == 0)
      continue;

    if (data.size < offset) {
      LOG(ERROR) << "Not enough data to read offset and size of metadata.";
      return false;
    }

    u64 metadata_offset, metadata_size;
    if (!ReadDataFromBuffer(data, sizeof(metadata_offset), "metadata offset",
                            &offset, &metadata_offset) ||
        !ReadDataFromBuffer(data, sizeof(metadata_size), "metadata size",
                            &offset, &metadata_size)) {
      return false;
    }

    if (data.size < metadata_offset + metadata_size) {
      LOG(ERROR) << "Not enough data to read metadata.";
      return false;
    }

    switch (type) {
    case HEADER_TRACING_DATA:
      if (!ReadTracingMetadata(data, metadata_offset, metadata_size)) {
        return false;
      }
      break;
    case HEADER_BUILD_ID:
      if (!ReadBuildIDMetadata(data, type, metadata_offset, metadata_size))
        return false;
      break;
    case HEADER_HOSTNAME:
    case HEADER_OSRELEASE:
    case HEADER_VERSION:
    case HEADER_ARCH:
    case HEADER_CPUDESC:
    case HEADER_CPUID:
    case HEADER_CMDLINE:
      if (!ReadStringMetadata(data, type, metadata_offset, metadata_size))
        return false;
      break;
    case HEADER_NRCPUS:
      if (!ReadUint32Metadata(data, type, metadata_offset, metadata_size))
        return false;
      break;
    case HEADER_TOTAL_MEM:
      if (!ReadUint64Metadata(data, type, metadata_offset, metadata_size))
        return false;
      break;
    case HEADER_EVENT_DESC:
      break;
    case HEADER_CPU_TOPOLOGY:
      if (!ReadCPUTopologyMetadata(data, type, metadata_offset, metadata_size))
        return false;
      break;
    case HEADER_NUMA_TOPOLOGY:
      if (!ReadNUMATopologyMetadata(data, type, metadata_offset, metadata_size))
        return false;
      break;
    case HEADER_PMU_MAPPINGS:
      // ignore for now
      continue;
      break;
    case HEADER_BRANCH_STACK:
      continue;
    default: LOG(INFO) << "Unsupported metadata type: " << type;
      break;
    }
  }

  // Event type events are optional in some newer versions of perf. They
  // contain the same information that is already in |attrs_|. Make sure the
  // number of event types matches the number of attrs, but only if there are
  // event type events present.
  if (event_types_.size() > 0) {
    if (event_types_.size() != attrs_.size()) {
      LOG(ERROR) << "Mismatch between number of event type events and attr "
                 << "events: " << event_types_.size() << " vs "
                 << attrs_.size();
      return false;
    }
    metadata_mask_ |= (1 << HEADER_EVENT_DESC);
  }
  return true;
}

bool PerfReader::ReadBuildIDMetadata(const ConstBufferWithSize& data, u32 /*type*/,
                                     size_t offset, size_t size) {
  CheckNoBuildIDEventPadding();
  while (size > 0) {
    // Make sure there is enough data for everything but the filename.
    if (data.size < offset + sizeof(build_id_event) / sizeof(*data.ptr)) {
      LOG(ERROR) << "Not enough bytes to read build id event";
      return false;
    }

    const build_id_event* temp_ptr =
        reinterpret_cast<const build_id_event*>(data.ptr + offset);
    u16 event_size = temp_ptr->header.size;
    if (is_cross_endian_)
      ByteSwap(&event_size);

    // Make sure there is enough data for the rest of the event.
    if (data.size < offset + event_size / sizeof(*data.ptr)) {
      LOG(ERROR) << "Not enough bytes to read build id event";
      return false;
    }

    // Allocate memory for the event and copy over the bytes.
    build_id_event* event = CallocMemoryForBuildID(event_size);
    if (!ReadDataFromBuffer(data, event_size, "build id event",
                            &offset, event)) {
      return false;
    }
    if (is_cross_endian_) {
      ByteSwap(&event->header.type);
      ByteSwap(&event->header.misc);
      ByteSwap(&event->header.size);
      ByteSwap(&event->pid);
    }
    size -= event_size;

    // Perf tends to use more space than necessary, so fix the size.
    event->header.size =
        sizeof(*event) + GetUint64AlignedStringLength(event->filename);
    build_id_events_.push_back(event);
  }

  return true;
}

bool PerfReader::ReadStringMetadata(const ConstBufferWithSize& data, u32 type,
                                    size_t offset, size_t size) {
  PerfStringMetadata str_data;
  str_data.type = type;

  size_t start_offset = offset;
  // Skip the number of string data if it is present.
  if (NeedsNumberOfStringData(type))
    offset += sizeof(num_string_data_type) / sizeof(*data.ptr);

  while ((offset - start_offset) < size) {
    CStringWithLength single_string;
    if (!ReadStringFromBuffer(data, is_cross_endian_, &offset, &single_string))
      return false;
    str_data.data.push_back(single_string);
  }

  string_metadata_.push_back(str_data);
  return true;
}

bool PerfReader::ReadUint32Metadata(const ConstBufferWithSize& data, u32 type,
                                    size_t offset, size_t size) {
  PerfUint32Metadata uint32_data;
  uint32_data.type = type;

  size_t start_offset = offset;
  while (size > offset - start_offset) {
    uint32_t item;
    if (!ReadDataFromBuffer(data, sizeof(item), "uint32_t data", &offset,
                            &item))
      return false;

    if (is_cross_endian_)
      ByteSwap(&item);

    uint32_data.data.push_back(item);
  }

  uint32_metadata_.push_back(uint32_data);
  return true;
}

bool PerfReader::ReadUint64Metadata(const ConstBufferWithSize& data, u32 type,
                                    size_t offset, size_t size) {
  PerfUint64Metadata uint64_data;
  uint64_data.type = type;

  size_t start_offset = offset;
  while (size > offset - start_offset) {
    uint64_t item;
    if (!ReadDataFromBuffer(data, sizeof(item), "uint64_t data", &offset,
                            &item))
      return false;

    if (is_cross_endian_)
      ByteSwap(&item);

    uint64_data.data.push_back(item);
  }

  uint64_metadata_.push_back(uint64_data);
  return true;
}

bool PerfReader::ReadCPUTopologyMetadata(
    const ConstBufferWithSize& data, u32 /*type*/, size_t offset, size_t /*size*/) {
  num_siblings_type num_core_siblings;
  if (!ReadDataFromBuffer(data, sizeof(num_core_siblings), "num cores",
                          &offset, &num_core_siblings)) {
    return false;
  }
  if (is_cross_endian_)
    ByteSwap(&num_core_siblings);

  cpu_topology_.core_siblings.resize(num_core_siblings);
  for (size_t i = 0; i < num_core_siblings; ++i) {
    if (!ReadStringFromBuffer(data, is_cross_endian_, &offset,
                              &cpu_topology_.core_siblings[i])) {
      return false;
    }
  }

  num_siblings_type num_thread_siblings;
  if (!ReadDataFromBuffer(data, sizeof(num_thread_siblings), "num threads",
                          &offset, &num_thread_siblings)) {
    return false;
  }
  if (is_cross_endian_)
    ByteSwap(&num_thread_siblings);

  cpu_topology_.thread_siblings.resize(num_thread_siblings);
  for (size_t i = 0; i < num_thread_siblings; ++i) {
    if (!ReadStringFromBuffer(data, is_cross_endian_, &offset,
                              &cpu_topology_.thread_siblings[i])) {
      return false;
    }
  }

  return true;
}

bool PerfReader::ReadNUMATopologyMetadata(
    const ConstBufferWithSize& data, u32 /*type*/, size_t offset, size_t /*size*/) {
  numa_topology_num_nodes_type num_nodes;
  if (!ReadDataFromBuffer(data, sizeof(num_nodes), "num nodes",
                          &offset, &num_nodes)) {
    return false;
  }
  if (is_cross_endian_)
    ByteSwap(&num_nodes);

  for (size_t i = 0; i < num_nodes; ++i) {
    PerfNodeTopologyMetadata node;
    if (!ReadDataFromBuffer(data, sizeof(node.id), "node id",
                            &offset, &node.id) ||
        !ReadDataFromBuffer(data, sizeof(node.total_memory),
                            "node total memory", &offset,
                            &node.total_memory) ||
        !ReadDataFromBuffer(data, sizeof(node.free_memory),
                            "node free memory", &offset, &node.free_memory) ||
        !ReadStringFromBuffer(data, is_cross_endian_, &offset,
                              &node.cpu_list)) {
      return false;
    }
    if (is_cross_endian_) {
      ByteSwap(&node.id);
      ByteSwap(&node.total_memory);
      ByteSwap(&node.free_memory);
    }
    numa_topology_.push_back(node);
  }
  return true;
}

bool PerfReader::ReadTracingMetadata(
    const ConstBufferWithSize& data, size_t offset, size_t size) {
  size_t tracing_data_offset = offset;
  tracing_data_.resize(size);
  return ReadDataFromBuffer(data, tracing_data_.size(), "tracing_data",
                            &tracing_data_offset, tracing_data_.data());
}

bool PerfReader::ReadTracingMetadataEvent(
    const ConstBufferWithSize& data, size_t offset) {
  // TRACING_DATA's header.size is a lie. It is the size of only the event
  // struct. The size of the data is in the event struct, and followed
  // immediately by the tracing header data.

  // Make a copy of the event (but not the tracing data)
  tracing_data_event tracing_event =
      *reinterpret_cast<const tracing_data_event*>(data.ptr + offset);

  if (is_cross_endian_) {
    ByteSwap(&tracing_event.header.type);
    ByteSwap(&tracing_event.header.misc);
    ByteSwap(&tracing_event.header.size);
    ByteSwap(&tracing_event.size);
  }

  return ReadTracingMetadata(data, offset + tracing_event.header.size,
                             tracing_event.size);
}

bool PerfReader::ReadAttrEventBlock(const ConstBufferWithSize& data,
                                    size_t offset, size_t size) {
  const size_t initial_offset = offset;
  PerfFileAttr attr;
  if (!ReadEventAttr(data, &offset, &attr.attr))
    return false;

  // attr.attr.size has been upgraded to the current size of perf_event_attr.
  const size_t actual_attr_size = offset - initial_offset;

  const size_t num_ids =
      (size - actual_attr_size) / sizeof(decltype(attr.ids)::value_type);
  if (!ReadUniqueIDs(data, num_ids, &offset, &attr.ids))
    return false;

  // Event types are found many times in the perf data file.
  // Only add this event type if it is not already present.
  for (size_t i = 0; i < attrs_.size(); ++i) {
    if (attrs_[i].ids[0] == attr.ids[0])
      return true;
  }
  attrs_.push_back(attr);
  return true;
}

// When this method is called, |event| is a reference to the bytes in the data
// vector that contains the entire perf.data file.  As a result, we need to be
// careful to only copy event.header.size bytes.
// In particular, something like
// event_t event_copy = event;
// would be bad, because it would read past the end of the event, and possibly
// pass the end of the data vector as well.
bool PerfReader::ReadPerfEventBlock(const event_t& event) {
  u16 size = event.header.size;
  if (is_cross_endian_)
    ByteSwap(&size);

  if (size > sizeof(event_t)) {
    LOG(INFO) << "Data size: " << size << " sizeof(event_t): "
              << sizeof(event_t);
    return false;
  }

  // Copy only the part of the event that is needed.
  malloced_unique_ptr<event_t> event_copy(CallocMemoryForEvent(size));
  memcpy(event_copy.get(), &event, size);
  if (is_cross_endian_) {
    ByteSwap(&event_copy->header.type);
    ByteSwap(&event_copy->header.misc);
    ByteSwap(&event_copy->header.size);
  }

  uint32_t type = event_copy->header.type;
  if (is_cross_endian_) {
    switch (type) {
    case PERF_RECORD_SAMPLE:
      break;
    case PERF_RECORD_MMAP:
      ByteSwap(&event_copy->mmap.pid);
      ByteSwap(&event_copy->mmap.tid);
      ByteSwap(&event_copy->mmap.start);
      ByteSwap(&event_copy->mmap.len);
      ByteSwap(&event_copy->mmap.pgoff);
      break;
    case PERF_RECORD_MMAP2:
      ByteSwap(&event_copy->mmap2.pid);
      ByteSwap(&event_copy->mmap2.tid);
      ByteSwap(&event_copy->mmap2.start);
      ByteSwap(&event_copy->mmap2.len);
      ByteSwap(&event_copy->mmap2.pgoff);
      ByteSwap(&event_copy->mmap2.maj);
      ByteSwap(&event_copy->mmap2.min);
      ByteSwap(&event_copy->mmap2.ino);
      ByteSwap(&event_copy->mmap2.ino_generation);
      break;
    case PERF_RECORD_FORK:
    case PERF_RECORD_EXIT:
      ByteSwap(&event_copy->fork.pid);
      ByteSwap(&event_copy->fork.tid);
      ByteSwap(&event_copy->fork.ppid);
      ByteSwap(&event_copy->fork.ptid);
      break;
    case PERF_RECORD_COMM:
      ByteSwap(&event_copy->comm.pid);
      ByteSwap(&event_copy->comm.tid);
      break;
    case PERF_RECORD_LOST:
      ByteSwap(&event_copy->lost.id);
      ByteSwap(&event_copy->lost.lost);
      break;
    case PERF_RECORD_READ:
      ByteSwap(&event_copy->read.pid);
      ByteSwap(&event_copy->read.tid);
      ByteSwap(&event_copy->read.value);
      ByteSwap(&event_copy->read.time_enabled);
      ByteSwap(&event_copy->read.time_running);
      ByteSwap(&event_copy->read.id);
      break;
    default:
      LOG(FATAL) << "Unknown event type: " << type;
    }
  }

  events_.push_back(std::move(event_copy));

  return true;
}

size_t PerfReader::GetNumMetadata() const {
  // This is just the number of 1s in the binary representation of the metadata
  // mask.  However, make sure to only use supported metadata, and don't include
  // branch stack (since it doesn't have an entry in the metadata section).
  uint64_t new_mask = metadata_mask_;
  new_mask &= kSupportedMetadataMask & ~(1 << HEADER_BRANCH_STACK);
  std::bitset<sizeof(new_mask) * CHAR_BIT> bits(new_mask);
  return bits.count();
}

size_t PerfReader::GetEventDescMetadataSize() const {
  size_t size = 0;
  if (event_types_.empty()) {
    return size;
  }
  if (metadata_mask_ & (1 << HEADER_EVENT_DESC)) {
    if (event_types_.size() > 0 && event_types_.size() != attrs_.size()) {
      LOG(ERROR) << "Mismatch between number of event type events and attr "
                 << "events: " << event_types_.size() << " vs "
                 << attrs_.size();
      return size;
    }
    size += sizeof(event_desc_num_events) + sizeof(event_desc_attr_size);
    CStringWithLength dummy;
    for (size_t i = 0; i < attrs_.size(); ++i) {
      size += sizeof(perf_event_attr) + sizeof(dummy.len);
      size += sizeof(event_desc_num_unique_ids);
      size += GetUint64AlignedStringLength(event_types_[i].name) * sizeof(char);
      size += attrs_[i].ids.size() * sizeof(attrs_[i].ids[0]);
    }
  }
  return size;
}

size_t PerfReader::GetBuildIDMetadataSize() const {
  size_t size = 0;
  for (size_t i = 0; i < build_id_events_.size(); ++i)
    size += build_id_events_[i]->header.size;
  return size;
}

size_t PerfReader::GetStringMetadataSize() const {
  size_t size = 0;
  for (size_t i = 0; i < string_metadata_.size(); ++i) {
    const PerfStringMetadata& metadata = string_metadata_[i];
    if (NeedsNumberOfStringData(metadata.type))
      size += sizeof(num_string_data_type);

    for (size_t j = 0; j < metadata.data.size(); ++j) {
      const CStringWithLength& str = metadata.data[j];
      size += sizeof(str.len) + (str.len * sizeof(char));
    }
  }
  return size;
}

size_t PerfReader::GetUint32MetadataSize() const {
  size_t size = 0;
  for (size_t i = 0; i < uint32_metadata_.size(); ++i) {
    const PerfUint32Metadata& metadata = uint32_metadata_[i];
    size += metadata.data.size() * sizeof(metadata.data[0]);
  }
  return size;
}

size_t PerfReader::GetUint64MetadataSize() const {
  size_t size = 0;
  for (size_t i = 0; i < uint64_metadata_.size(); ++i) {
    const PerfUint64Metadata& metadata = uint64_metadata_[i];
    size += metadata.data.size() * sizeof(metadata.data[0]);
  }
  return size;
}

size_t PerfReader::GetCPUTopologyMetadataSize() const {
  // Core siblings.
  size_t size = sizeof(num_siblings_type);
  for (size_t i = 0; i < cpu_topology_.core_siblings.size(); ++i) {
    const CStringWithLength& str = cpu_topology_.core_siblings[i];
    size += sizeof(str.len) + (str.len * sizeof(char));
  }

  // Thread siblings.
  size += sizeof(num_siblings_type);
  for (size_t i = 0; i < cpu_topology_.thread_siblings.size(); ++i) {
    const CStringWithLength& str = cpu_topology_.thread_siblings[i];
    size += sizeof(str.len) + (str.len * sizeof(char));
  }

  return size;
}

size_t PerfReader::GetNUMATopologyMetadataSize() const {
  size_t size = sizeof(numa_topology_num_nodes_type);
  for (size_t i = 0; i < numa_topology_.size(); ++i) {
    const PerfNodeTopologyMetadata& node = numa_topology_[i];
    size += sizeof(node.id);
    size += sizeof(node.total_memory) + sizeof(node.free_memory);
    size += sizeof(node.cpu_list.len) + node.cpu_list.len * sizeof(char);
  }
  return size;
}

bool PerfReader::NeedsNumberOfStringData(u32 type) const {
  return type == HEADER_CMDLINE;
}

bool PerfReader::LocalizeMMapFilenames(
    const std::map<string, string>& filename_map) {
  // Search for mmap/mmap2 events for which the filename needs to be updated.
  for (size_t i = 0; i < events_.size(); ++i) {
    string filename;
    size_t size_of_fixed_event_parts;
    event_t* event = events_[i].get();
    if (event->header.type == PERF_RECORD_MMAP) {
      filename = string(event->mmap.filename);
      size_of_fixed_event_parts =
          sizeof(event->mmap) - sizeof(event->mmap.filename);
    } else if (event->header.type == PERF_RECORD_MMAP2) {
      filename = string(event->mmap2.filename);
      size_of_fixed_event_parts =
          sizeof(event->mmap2) - sizeof(event->mmap2.filename);
    } else {
      continue;
    }

    const auto it = filename_map.find(filename);
    if (it == filename_map.end())  // not found
      continue;

    const string& new_filename = it->second;
    size_t old_len = GetUint64AlignedStringLength(filename);
    size_t new_len = GetUint64AlignedStringLength(new_filename);
    size_t old_offset = GetPerfSampleDataOffset(*event);
    size_t sample_size = event->header.size - old_offset;

    int size_change = new_len - old_len;
    size_t new_size = event->header.size + size_change;
    size_t new_offset = old_offset + size_change;

    if (size_change > 0) {
      // Allocate memory for a new event.
      event_t* old_event = event;
      malloced_unique_ptr<event_t> new_event(CallocMemoryForEvent(new_size));

      // Copy over everything except filename and sample info.
      memcpy(new_event.get(), old_event, size_of_fixed_event_parts);

      // Copy over the sample info to the correct location.
      char* old_addr = reinterpret_cast<char*>(old_event);
      char* new_addr = reinterpret_cast<char*>(new_event.get());
      memcpy(new_addr + new_offset, old_addr + old_offset, sample_size);

      events_[i] = std::move(new_event);
      event = events_[i].get();
    } else if (size_change < 0) {
      // Move the perf sample data to its new location.
      // Since source and dest could overlap, use memmove instead of memcpy.
      char* start_addr = reinterpret_cast<char*>(event);
      memmove(start_addr + new_offset, start_addr + old_offset, sample_size);
    }

    // Copy over the new filename and fix the size of the event.
    char *event_filename = nullptr;
    if (event->header.type == PERF_RECORD_MMAP) {
      event_filename = event->mmap.filename;
    } else if (event->header.type == PERF_RECORD_MMAP2) {
      event_filename = event->mmap2.filename;
    } else {
      LOG(FATAL) << "Unexpected event type";  // Impossible
    }
    CHECK_GT(snprintf(event_filename, new_filename.size() + 1, "%s",
                      new_filename.c_str()),
             0);
    event->header.size = new_size;
  }

  return true;
}

}  // namespace quipper
