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

#include "event_selection_set.h"

#include <base/logging.h>

#include "environment.h"
#include "event_attr.h"
#include "event_type.h"

void EventSelectionSet::AddEventType(const EventType& event_type) {
  EventSelection selection;
  selection.event_type = &event_type;
  selection.event_attr = CreateDefaultPerfEventAttr(event_type);
  selections_.push_back(std::move(selection));
}

void EventSelectionSet::EnableOnExec() {
  for (auto& selection : selections_) {
    selection.event_attr.enable_on_exec = 1;
  }
}

void EventSelectionSet::SampleIdAll() {
  for (auto& selection : selections_) {
    selection.event_attr.sample_id_all = 1;
  }
}

void EventSelectionSet::SetSampleFreq(uint64_t sample_freq) {
  for (auto& selection : selections_) {
    perf_event_attr& attr = selection.event_attr;
    attr.freq = 1;
    attr.sample_freq = sample_freq;
  }
}

void EventSelectionSet::SetSamplePeriod(uint64_t sample_period) {
  for (auto& selection : selections_) {
    perf_event_attr& attr = selection.event_attr;
    attr.freq = 0;
    attr.sample_period = sample_period;
  }
}

bool EventSelectionSet::OpenEventFilesForAllCpus() {
  std::vector<int> cpus = GetOnlineCpus();
  if (cpus.empty()) {
    return false;
  }
  for (auto& selection : selections_) {
    for (auto& cpu : cpus) {
      auto event_fd = EventFd::OpenEventFileForCpu(selection.event_attr, cpu);
      if (event_fd != nullptr) {
        selection.event_fds.push_back(std::move(event_fd));
      }
    }
    // As the online cpus can be enabled or disabled at runtime, we may not open event file for
    // all cpus successfully. But we should open at least one cpu successfully.
    if (selection.event_fds.empty()) {
      LOG(ERROR) << "failed to open perf event file for event_type " << selection.event_type->name
                 << " on all cpus";
      return false;
    }
  }
  return true;
}

bool EventSelectionSet::OpenEventFilesForProcess(pid_t pid) {
  for (auto& selection : selections_) {
    auto event_fd = EventFd::OpenEventFileForProcess(selection.event_attr, pid);
    if (event_fd == nullptr) {
      PLOG(ERROR) << "failed to open perf event file for event type " << selection.event_type->name
                  << " on pid " << pid;
      return false;
    }
    selection.event_fds.push_back(std::move(event_fd));
  }
  return true;
}

bool EventSelectionSet::EnableEvents() {
  for (auto& selection : selections_) {
    for (auto& event_fd : selection.event_fds) {
      if (!event_fd->EnableEvent()) {
        return false;
      }
    }
  }
  return true;
}

bool EventSelectionSet::ReadCounters(
    std::map<const EventType*, std::vector<PerfCounter>>* counters_map) {
  for (auto& selection : selections_) {
    std::vector<PerfCounter> counters;
    for (auto& event_fd : selection.event_fds) {
      PerfCounter counter;
      if (!event_fd->ReadCounter(&counter)) {
        return false;
      }
      counters.push_back(counter);
    }
    counters_map->insert(std::make_pair(selection.event_type, counters));
  }
  return true;
}

void EventSelectionSet::PreparePollForEventFiles(std::vector<pollfd>* pollfds) {
  for (auto& selection : selections_) {
    for (auto& event_fd : selection.event_fds) {
      pollfd poll_fd;
      event_fd->PreparePollForMmapData(&poll_fd);
      pollfds->push_back(poll_fd);
    }
  }
}

bool EventSelectionSet::MmapEventFiles(size_t mmap_pages) {
  for (auto& selection : selections_) {
    for (auto& event_fd : selection.event_fds) {
      if (!event_fd->MmapContent(mmap_pages)) {
        return false;
      }
    }
  }
  return true;
}

static bool ReadMmapEventDataForFd(std::unique_ptr<EventFd>& event_fd,
                                   std::function<bool(const char*, size_t)> callback,
                                   bool* have_data) {
  *have_data = false;
  while (true) {
    char* data;
    size_t size = event_fd->GetAvailableMmapData(&data);
    if (size == 0) {
      break;
    }
    if (!callback(data, size)) {
      return false;
    }
    *have_data = true;
    event_fd->DiscardMmapData(size);
  }
  return true;
}

bool EventSelectionSet::ReadMmapEventData(std::function<bool(const char*, size_t)> callback) {
  for (auto& selection : selections_) {
    for (auto& event_fd : selection.event_fds) {
      while (true) {
        bool have_data;
        if (!ReadMmapEventDataForFd(event_fd, callback, &have_data)) {
          return false;
        }
        if (!have_data) {
          break;
        }
      }
    }
  }
  return true;
}

std::string EventSelectionSet::FindEventFileNameById(uint64_t id) {
  for (auto& selection : selections_) {
    for (auto& event_fd : selection.event_fds) {
      if (event_fd->Id() == id) {
        return event_fd->Name();
      }
    }
  }
  return "";
}

EventSelectionSet::EventSelection* EventSelectionSet::FindSelectionByType(
    const EventType& event_type) {
  for (auto& selection : selections_) {
    if (strcmp(selection.event_type->name, event_type.name) == 0) {
      return &selection;
    }
  }
  return nullptr;
}

const perf_event_attr& EventSelectionSet::FindEventAttrByType(const EventType& event_type) {
  return FindSelectionByType(event_type)->event_attr;
}

const std::vector<std::unique_ptr<EventFd>>& EventSelectionSet::FindEventFdsByType(
    const EventType& event_type) {
  return FindSelectionByType(event_type)->event_fds;
}
