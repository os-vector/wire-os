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

#include "event_type.h"

#include <unistd.h>
#include <string>
#include <vector>

#include <base/logging.h>

#include "event_attr.h"
#include "event_fd.h"

#define EVENT_TYPE_TABLE_ENTRY(name, type, config) \
  { name, type, config }                           \
  ,

static std::vector<const EventType> event_type_array = {
#include "event_type_table.h"
};

static bool IsEventTypeSupportedByKernel(const EventType& event_type) {
  auto event_fd = EventFd::OpenEventFileForProcess(CreateDefaultPerfEventAttr(event_type), getpid());
  return event_fd != nullptr;
}

bool EventType::IsSupportedByKernel() const {
  return IsEventTypeSupportedByKernel(*this);
}

const std::vector<const EventType>& EventTypeFactory::GetAllEventTypes() {
  return event_type_array;
}

const EventType* EventTypeFactory::FindEventTypeByName(const std::string& name,
                                                       bool report_unsupported_type) {
  const EventType* result = nullptr;
  for (auto& event_type : event_type_array) {
    if (event_type.name == name) {
      result = &event_type;
      break;
    }
  }
  if (result == nullptr) {
    LOG(ERROR) << "Unknown event_type '" << name
               << "', try `simpleperf list` to list all possible event type names";
    return nullptr;
  }
  if (!result->IsSupportedByKernel()) {
    (report_unsupported_type ? PLOG(ERROR) : PLOG(DEBUG)) << "Event type '" << result->name
                                                          << "' is not supported by the kernel";
    return nullptr;
  }
  return result;
}

const EventType* EventTypeFactory::FindEventTypeByConfig(uint32_t type, uint64_t config) {
  for (auto& event_type : event_type_array) {
    if (event_type.type == type && event_type.config == config) {
      return &event_type;
    }
  }
  return nullptr;
}
