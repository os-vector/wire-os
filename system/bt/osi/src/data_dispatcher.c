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

#define LOG_TAG "bt_osi_data_dispatcher"

#include "osi/include/data_dispatcher.h"

#include <assert.h>

#include "osi/include/allocator.h"
#include "osi/include/hash_functions.h"
#include "osi/include/hash_map.h"
#include "osi/include/osi.h"
#include "osi/include/log.h"

#define DEFAULT_TABLE_BUCKETS 10

struct data_dispatcher_t {
  char *name;
  hash_map_t *dispatch_table;
  fixed_queue_t *default_queue; // We don't own this queue
};

data_dispatcher_t *data_dispatcher_new(const char *name) {
  assert(name != NULL);

  data_dispatcher_t *ret = osi_calloc(sizeof(data_dispatcher_t));

  ret->dispatch_table = hash_map_new(DEFAULT_TABLE_BUCKETS, hash_function_naive, NULL, NULL, NULL);
  if (!ret->dispatch_table) {
    LOG_ERROR(LOG_TAG, "%s unable to create dispatch table.", __func__);
    goto error;
  }

  ret->name = osi_strdup(name);
  if (!ret->name) {
    LOG_ERROR(LOG_TAG, "%s unable to duplicate provided name.", __func__);
    goto error;
  }

  return ret;

error:;
  data_dispatcher_free(ret);
  return NULL;
}

void data_dispatcher_free(data_dispatcher_t *dispatcher) {
  if (!dispatcher)
    return;

  hash_map_free(dispatcher->dispatch_table);
  osi_free(dispatcher->name);
  osi_free(dispatcher);
}

void data_dispatcher_register(data_dispatcher_t *dispatcher, data_dispatcher_type_t type, fixed_queue_t *queue) {
  assert(dispatcher != NULL);

  hash_map_erase(dispatcher->dispatch_table, (void *)type);
  if (queue)
    hash_map_set(dispatcher->dispatch_table, (void *)type, queue);
}

void data_dispatcher_register_default(data_dispatcher_t *dispatcher, fixed_queue_t *queue) {
  assert(dispatcher != NULL);

  dispatcher->default_queue = queue;
}

bool data_dispatcher_dispatch(data_dispatcher_t *dispatcher, data_dispatcher_type_t type, void *data) {
  assert(dispatcher != NULL);
  assert(data != NULL);

  fixed_queue_t *queue = hash_map_get(dispatcher->dispatch_table, (void *)type);
  if (!queue)
    queue = dispatcher->default_queue;

  if (queue)
    fixed_queue_enqueue(queue, data);
  else
    LOG_WARN(LOG_TAG, "%s has no handler for type (%zd) in data dispatcher named: %s", __func__, type, dispatcher->name);

  return queue != NULL;
}
