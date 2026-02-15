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

#include "event_attr.h"

#include <inttypes.h>
#include <stdio.h>
#include <string>
#include <unordered_map>

#include <base/logging.h>

#include "event_type.h"
#include "utils.h"

static std::string BitsToString(const std::string& name, uint64_t bits,
                                const std::vector<std::pair<int, std::string>>& bit_names) {
  std::string result;
  for (auto& p : bit_names) {
    if (bits & p.first) {
      bits &= ~p.first;
      if (!result.empty()) {
        result += ", ";
      }
      result += p.second;
    }
  }
  if (bits != 0) {
    LOG(DEBUG) << "unknown " << name << " bits: " << std::hex << bits;
  }
  return result;
}

static std::string SampleTypeToString(uint64_t sample_type) {
  static std::vector<std::pair<int, std::string>> sample_type_names = {
      {PERF_SAMPLE_IP, "ip"},
      {PERF_SAMPLE_TID, "tid"},
      {PERF_SAMPLE_TIME, "time"},
      {PERF_SAMPLE_ADDR, "addr"},
      {PERF_SAMPLE_READ, "read"},
      {PERF_SAMPLE_CALLCHAIN, "callchain"},
      {PERF_SAMPLE_ID, "id"},
      {PERF_SAMPLE_CPU, "cpu"},
      {PERF_SAMPLE_PERIOD, "period"},
      {PERF_SAMPLE_STREAM_ID, "stream_id"},
      {PERF_SAMPLE_RAW, "raw"},
  };
  return BitsToString("sample_type", sample_type, sample_type_names);
}

static std::string ReadFormatToString(uint64_t read_format) {
  static std::vector<std::pair<int, std::string>> read_format_names = {
      {PERF_FORMAT_TOTAL_TIME_ENABLED, "total_time_enabled"},
      {PERF_FORMAT_TOTAL_TIME_RUNNING, "total_time_running"},
      {PERF_FORMAT_ID, "id"},
      {PERF_FORMAT_GROUP, "group"},
  };
  return BitsToString("read_format", read_format, read_format_names);
}

perf_event_attr CreateDefaultPerfEventAttr(const EventType& event_type) {
  perf_event_attr attr;
  memset(&attr, 0, sizeof(attr));
  attr.size = sizeof(perf_event_attr);
  attr.type = event_type.type;
  attr.config = event_type.config;
  attr.mmap = 1;
  attr.comm = 1;
  attr.disabled = 1;
  // Changing read_format affects the layout of the data read from perf_event_file, namely
  // PerfCounter in event_fd.h.
  attr.read_format =
      PERF_FORMAT_TOTAL_TIME_ENABLED | PERF_FORMAT_TOTAL_TIME_RUNNING | PERF_FORMAT_ID;
  attr.sample_type |= PERF_SAMPLE_IP | PERF_SAMPLE_TID | PERF_SAMPLE_TIME | PERF_SAMPLE_PERIOD;
  return attr;
}

void DumpPerfEventAttr(const perf_event_attr& attr, size_t indent) {
  std::string event_name = "unknown";
  const EventType* event_type = EventTypeFactory::FindEventTypeByConfig(attr.type, attr.config);
  if (event_type != nullptr) {
    event_name = event_type->name;
  }

  PrintIndented(indent, "event_attr: for event %s\n", event_name.c_str());

  PrintIndented(indent + 1, "type %u, size %u, config %llu\n", attr.type, attr.size, attr.config);

  if (attr.freq != 0) {
    PrintIndented(indent + 1, "sample_freq %llu\n", attr.sample_freq);
  } else {
    PrintIndented(indent + 1, "sample_period %llu\n", attr.sample_period);
  }

  PrintIndented(indent + 1, "sample_type (0x%llx) %s\n", attr.sample_type,
                SampleTypeToString(attr.sample_type).c_str());

  PrintIndented(indent + 1, "read_format (0x%llx) %s\n", attr.read_format,
                ReadFormatToString(attr.read_format).c_str());

  PrintIndented(indent + 1, "disabled %llu, inherit %llu, pinned %llu, exclusive %llu\n",
                attr.disabled, attr.inherit, attr.pinned, attr.exclusive);

  PrintIndented(indent + 1, "exclude_user %llu, exclude_kernel %llu, exclude_hv %llu\n",
                attr.exclude_user, attr.exclude_kernel, attr.exclude_hv);

  PrintIndented(indent + 1, "exclude_idle %llu, mmap %llu, comm %llu, freq %llu\n",
                attr.exclude_idle, attr.mmap, attr.comm, attr.freq);

  PrintIndented(indent + 1, "inherit_stat %llu, enable_on_exec %llu, task %llu\n",
                attr.inherit_stat, attr.enable_on_exec, attr.task);

  PrintIndented(indent + 1, "watermark %llu, precise_ip %llu, mmap_data %llu\n", attr.watermark,
                attr.precise_ip, attr.mmap_data);

  PrintIndented(indent + 1, "sample_id_all %llu, exclude_host %llu, exclude_guest %llu\n",
                attr.sample_id_all, attr.exclude_host, attr.exclude_guest);
}
