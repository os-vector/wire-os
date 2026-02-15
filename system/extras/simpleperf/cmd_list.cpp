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

#include <stdio.h>
#include <string>
#include <vector>

#include <base/logging.h>

#include "command.h"
#include "event_type.h"
#include "perf_event.h"

static void PrintEventTypesOfType(uint32_t type, const char* type_name,
                                  const std::vector<const EventType>& event_types) {
  printf("List of %s:\n", type_name);
  for (auto& event_type : event_types) {
    if (event_type.type == type && event_type.IsSupportedByKernel()) {
      printf("  %s\n", event_type.name);
    }
  }
  printf("\n");
}

class ListCommand : public Command {
 public:
  ListCommand()
      : Command("list", "list all available perf events",
                "Usage: simpleperf list\n"
                "    List all available perf events on this machine.\n") {
  }

  bool Run(const std::vector<std::string>& args) override;
};

bool ListCommand::Run(const std::vector<std::string>& args) {
  if (args.size() != 1) {
    LOG(ERROR) << "malformed command line: list subcommand needs no argument";
    LOG(ERROR) << "try using \"help list\"";
    return false;
  }
  auto& event_types = EventTypeFactory::GetAllEventTypes();

  PrintEventTypesOfType(PERF_TYPE_HARDWARE, "hardware events", event_types);
  PrintEventTypesOfType(PERF_TYPE_SOFTWARE, "software events", event_types);
  PrintEventTypesOfType(PERF_TYPE_HW_CACHE, "hw-cache events", event_types);
  return true;
}

ListCommand list_command;
