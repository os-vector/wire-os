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

#ifndef SIMPLE_PERF_EVENT_SELECTION_SET_H_
#define SIMPLE_PERF_EVENT_SELECTION_SET_H_

#include <poll.h>
#include <functional>
#include <map>
#include <vector>

#include <base/macros.h>

#include "event_fd.h"
#include "perf_event.h"

struct EventType;

// EventSelectionSet helps to monitor events.
// Firstly, the user creates an EventSelectionSet, and adds the specific event types to monitor.
// Secondly, the user defines how to monitor the events (by setting enable_on_exec flag,
// sample frequency, etc).
// Then, the user can start monitoring by ordering the EventSelectionSet to open perf event files
// and enable events (if enable_on_exec flag isn't used).
// After that, the user can read counters or read mapped event records.
// At last, the EventSelectionSet will clean up resources at destruction automatically.

class EventSelectionSet {
 public:
  EventSelectionSet() {
  }

  bool Empty() const {
    return selections_.empty();
  }

  void AddEventType(const EventType& event_type);

  void EnableOnExec();
  void SampleIdAll();
  void SetSampleFreq(uint64_t sample_freq);
  void SetSamplePeriod(uint64_t sample_period);

  bool OpenEventFilesForAllCpus();
  bool OpenEventFilesForProcess(pid_t pid);
  bool EnableEvents();
  bool ReadCounters(std::map<const EventType*, std::vector<PerfCounter>>* counters_map);
  void PreparePollForEventFiles(std::vector<pollfd>* pollfds);
  bool MmapEventFiles(size_t mmap_pages);
  bool ReadMmapEventData(std::function<bool(const char*, size_t)> callback);

  std::string FindEventFileNameById(uint64_t id);
  const perf_event_attr& FindEventAttrByType(const EventType& event_type);
  const std::vector<std::unique_ptr<EventFd>>& FindEventFdsByType(const EventType& event_type);

 private:
  struct EventSelection {
    const EventType* event_type;
    perf_event_attr event_attr;
    std::vector<std::unique_ptr<EventFd>> event_fds;
  };
  EventSelection* FindSelectionByType(const EventType& event_type);

  std::vector<EventSelection> selections_;

  DISALLOW_COPY_AND_ASSIGN(EventSelectionSet);
};

#endif  // SIMPLE_PERF_EVENT_SELECTION_SET_H_
