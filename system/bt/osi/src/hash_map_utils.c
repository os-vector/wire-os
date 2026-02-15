/******************************************************************************
 *
 *  Copyright (C) 2015 Google, Inc.
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
#define LOG_TAG "hash_map_utils"

#include "osi/include/hash_map_utils.h"

#include <assert.h>
#include <string.h>

#include "osi/include/allocator.h"
#include "osi/include/hash_functions.h"
#include "osi/include/hash_map.h"
#include "osi/include/log.h"
#include "osi/include/osi.h"

static bool string_equals(const void *key_a, const void *key_b);
static bool dump_entry(hash_map_entry_t *entry, UNUSED_ATTR void *context);

static const size_t BUCKETS_NUM = 5;

hash_map_t *hash_map_utils_new_from_string_params(const char *params) {
  assert(params != NULL);

  hash_map_t *map = hash_map_new(BUCKETS_NUM, hash_function_string, osi_free,
                                 osi_free, string_equals);
  if (!map)
    return NULL;

  char *str = osi_strdup(params);
  if (!str)
    return NULL;

  LOG_VERBOSE(LOG_TAG, "%s: source string: '%s'", __func__, str);

  // Parse |str| and add extracted key-and-value pair(s) in |map|.
  int items = 0;
  char *tmpstr;
  char *kvpair = strtok_r(str, ";", &tmpstr);
  while (kvpair && *kvpair) {
    char *eq = strchr(kvpair, '=');

    if (eq == kvpair)
      goto next_pair;

    char *key;
    char *value;
    if (eq) {
      key = osi_strndup(kvpair, eq - kvpair);

      // The increment of |eq| moves |eq| to the beginning of the value.
      ++eq;
      value = (*eq != '\0') ? osi_strdup(eq) : osi_strdup("");
    } else {
      key = osi_strdup(kvpair);
      value = osi_strdup("");
    }

    hash_map_set(map, key, value);

    items++;
next_pair:
    kvpair = strtok_r(NULL, ";", &tmpstr);
  }

  if (!items)
    LOG_VERBOSE(LOG_TAG, "%s: no items found in string\n", __func__);

  osi_free(str);
  return map;
}

void hash_map_utils_dump_string_keys_string_values(hash_map_t *map) {
  if (!map) {
    LOG_VERBOSE( LOG_TAG, "%s: the given map is NULL\n", __func__);
    return;
  }
  hash_map_foreach(map, dump_entry, NULL);
}

static bool string_equals(const void *key_a, const void *key_b) {
  return !strcmp(key_a, key_b);
}

static bool dump_entry(hash_map_entry_t *entry, UNUSED_ATTR void *context) {
  hash_map_entry_t *hash_map_entry = (hash_map_entry_t *)entry;
  LOG_INFO(LOG_TAG, "key: '%s' value: '%s'\n", (char *)hash_map_entry->key,
           (char *)hash_map_entry->data);
  return true;
}
