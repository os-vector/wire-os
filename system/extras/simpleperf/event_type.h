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

#ifndef SIMPLE_PERF_EVENT_H_
#define SIMPLE_PERF_EVENT_H_

#include <stdint.h>
#include <string>
#include <vector>

// EventType represents one type of event, like cpu_cycle_event, cache_misses_event.
// The user knows one event type by its name, and the kernel knows one event type by its
// (type, config) pair. EventType connects the two representations, and tells the user if
// the event type is supported by the kernel.

struct EventType {
  bool IsSupportedByKernel() const;

  const char* name;
  uint32_t type;
  uint64_t config;
};

class EventTypeFactory {
 public:
  static const std::vector<const EventType>& GetAllEventTypes();
  static const EventType* FindEventTypeByName(const std::string& name,
                                              bool report_unsupported_type = true);
  static const EventType* FindEventTypeByConfig(uint32_t type, uint64_t config);
};

#endif  // SIMPLE_PERF_EVENT_H_
