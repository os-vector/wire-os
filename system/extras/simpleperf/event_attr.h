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

#ifndef SIMPLE_PERF_EVENT_ATTR_H_
#define SIMPLE_PERF_EVENT_ATTR_H_

#include <stdint.h>
#include <string>

#include "perf_event.h"

struct EventType;

perf_event_attr CreateDefaultPerfEventAttr(const EventType& event_type);
void DumpPerfEventAttr(const perf_event_attr& attr, size_t indent = 0);

#endif  // SIMPLE_PERF_EVENT_ATTR_H_
