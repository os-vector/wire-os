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

#define LOG_TAG "bt_profile_manager"

#include <assert.h>
#include <stdbool.h>

#include "btcore/include/module.h"
#include "profile/include/manager.h"
#include "osi/include/future.h"
#include "osi/include/hash_functions.h"
#include "osi/include/hash_map.h"
#include "osi/include/log.h"
#include "osi/include/osi.h"

static const size_t number_of_profile_buckets = 15;

static bool initialized;
static hash_map_t *profile_map;

// Lifecycle management functions

static future_t *init(void) {
  profile_map = hash_map_new(
    number_of_profile_buckets,
    hash_function_string,
    NULL,
    NULL,
    NULL);

  initialized = true;
  return NULL;
}

static future_t *clean_up(void) {
  initialized = false;

  hash_map_free(profile_map);
  profile_map = NULL;

  return NULL;
}

//TODO: Fix this
#ifndef ANDROID
#define EXPORT_SYMBOL   __attribute__((visibility("default")))
#endif

EXPORT_SYMBOL const module_t profile_manager_module = {
  .name = PROFILE_MANAGER_MODULE,
  .init = init,
  .start_up = NULL,
  .shut_down = NULL,
  .clean_up = clean_up,
  .dependencies = {
    NULL
  }
};

// Interface functions

void profile_register(const profile_t *profile) {
  assert(initialized);
  assert(profile != NULL);
  assert(profile->name != NULL);
  assert(!hash_map_has_key(profile_map, profile->name));

  hash_map_set(profile_map, profile->name, (void *) profile);
}

const profile_t *profile_by_name(const char *name) {
  assert(initialized);
  assert(name != NULL);

  return (profile_t *)hash_map_get(profile_map, name);
}
