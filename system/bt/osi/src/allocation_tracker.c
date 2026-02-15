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

#define LOG_TAG "bt_osi_allocation_tracker"

#include "osi/include/allocation_tracker.h"

#include <assert.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>

#include "osi/include/allocator.h"
#include "osi/include/hash_functions.h"
#include "osi/include/hash_map.h"
#include "osi/include/log.h"
#include "osi/include/osi.h"

typedef struct {
  uint8_t allocator_id;
  void *ptr;
  size_t size;
  bool freed;
} allocation_t;

// Hidden constructor for hash map for our use only. Everything else should use the
// normal interface.
hash_map_t *hash_map_new_internal(
    size_t size,
    hash_index_fn hash_fn,
    key_free_fn key_fn,
    data_free_fn,
    key_equality_fn equality_fn,
    const allocator_t *zeroed_allocator);

static bool allocation_entry_freed_checker(hash_map_entry_t *entry, void *context);
static void *untracked_calloc(size_t size);

static const size_t allocation_hash_map_size = 1024;
static const char *canary = "tinybird";
static const allocator_t untracked_calloc_allocator = {
  untracked_calloc,
  free
};

static size_t canary_size;
static hash_map_t *allocations;
static pthread_mutex_t lock;

void allocation_tracker_init(void) {
  if (allocations)
    return;

  canary_size = strlen(canary);

  pthread_mutex_init(&lock, NULL);

  pthread_mutex_lock(&lock);
  allocations = hash_map_new_internal(
    allocation_hash_map_size,
    hash_function_pointer,
    NULL,
    free,
    NULL,
    &untracked_calloc_allocator);
  pthread_mutex_unlock(&lock);
}

// Test function only. Do not call in the normal course of operations.
void allocation_tracker_uninit(void) {
  if (!allocations)
    return;

  pthread_mutex_lock(&lock);
  hash_map_free(allocations);
  allocations = NULL;
  pthread_mutex_unlock(&lock);
}

void allocation_tracker_reset(void) {
  if (!allocations)
    return;

  pthread_mutex_lock(&lock);
  hash_map_clear(allocations);
  pthread_mutex_unlock(&lock);
}

size_t allocation_tracker_expect_no_allocations(void) {
  if (!allocations)
    return 0;

  pthread_mutex_lock(&lock);

  size_t unfreed_memory_size = 0;
  hash_map_foreach(allocations, allocation_entry_freed_checker, &unfreed_memory_size);

  pthread_mutex_unlock(&lock);

  return unfreed_memory_size;
}

void *allocation_tracker_notify_alloc(uint8_t allocator_id, void *ptr, size_t requested_size) {
  if (!allocations || !ptr)
    return ptr;

  char *return_ptr = (char *)ptr;

  return_ptr += canary_size;

  pthread_mutex_lock(&lock);

  allocation_t *allocation = (allocation_t *)hash_map_get(allocations, return_ptr);
  if (allocation) {
    assert(allocation->freed); // Must have been freed before
  } else {
    allocation = (allocation_t *)calloc(1, sizeof(allocation_t));
    hash_map_set(allocations, return_ptr, allocation);
  }

  if (allocation) {
    allocation->allocator_id = allocator_id;
    allocation->freed = false;
    allocation->size = requested_size;
    allocation->ptr = return_ptr;
  } else {
    LOG_ERROR(LOG_TAG, "%s Memory not allocated for allocation." ,__func__);
  }

  pthread_mutex_unlock(&lock);

  // Add the canary on both sides
  memcpy(return_ptr - canary_size, canary, canary_size);
  memcpy(return_ptr + requested_size, canary, canary_size);

  return return_ptr;
}

void *allocation_tracker_notify_free(UNUSED_ATTR uint8_t allocator_id, void *ptr) {
  if (!allocations || !ptr)
    return ptr;

  pthread_mutex_lock(&lock);

  allocation_t *allocation = (allocation_t *)hash_map_get(allocations, ptr);
  assert(allocation);                               // Must have been tracked before
  assert(!allocation->freed);                       // Must not be a double free
  assert(allocation->allocator_id == allocator_id); // Must be from the same allocator
  allocation->freed = true;

  UNUSED_ATTR const char *beginning_canary = ((char *)ptr) - canary_size;
  UNUSED_ATTR const char *end_canary = ((char *)ptr) + allocation->size;

  for (size_t i = 0; i < canary_size; i++) {
    assert(beginning_canary[i] == canary[i]);
    assert(end_canary[i] == canary[i]);
  }

  // Free the hash map entry to avoid unlimited memory usage growth.
  // Double-free of memory is detected with "assert(allocation)" above
  // as the allocation entry will not be present.
  hash_map_erase(allocations, ptr);

  pthread_mutex_unlock(&lock);

  return ((char *)ptr) - canary_size;
}

size_t allocation_tracker_resize_for_canary(size_t size) {
  return (!allocations) ? size : size + (2 * canary_size);
}

static bool allocation_entry_freed_checker(hash_map_entry_t *entry, void *context) {
  allocation_t *allocation = (allocation_t *)entry->data;
  if (!allocation->freed) {
    *((size_t *)context) += allocation->size; // Report back the unfreed byte count
    LOG_ERROR(LOG_TAG, "%s found unfreed allocation. address: 0x%zx size: %zd bytes", __func__, (uintptr_t)allocation->ptr, allocation->size);
  }

  return true;
}

static void *untracked_calloc(size_t size) {
  return calloc(size, 1);
}
