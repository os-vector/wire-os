/******************************************************************************
 *
 *  Copyright (C) 2014 Google, Inc.
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at:
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 ******************************************************************************/

#pragma once

static const char PROFILE_MANAGER_MODULE[] = "profile_manager_module";

typedef enum {
  PROFILE_POWER_ACTIVE,
  PROFILE_POWER_HOLD,
  PROFILE_POWER_SNIFF,
  PROFILE_POWER_PARK
} profile_power_level_t;

typedef struct profile_t {
  const char *name;
  profile_power_level_t lowest_acceptable_power_mode;
  bool in_use;
} profile_t;

// Registers a given Bluetooth |profile| with the manager.
void profile_register(const profile_t *profile);

// Looks up a previously registered profile by |name|. If no profile was
// registered by the given |name|, then this function returns NULL.
const profile_t *profile_by_name(const char *name);
