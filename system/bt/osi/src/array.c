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

#define LOG_TAG "bt_osi_array"

#include "osi/include/array.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "osi/include/allocator.h"
#include "osi/include/log.h"

struct array_t {
  size_t element_size;
  size_t length;
  size_t capacity;
  uint8_t *data;
  uint8_t internal_storage[];
};

static bool grow(array_t *array);

static const size_t INTERNAL_ELEMENTS = 16;

array_t *array_new(size_t element_size) {
  assert(element_size > 0);

  array_t *array = osi_calloc(sizeof(array_t) + element_size * INTERNAL_ELEMENTS);

  array->element_size = element_size;
  array->capacity = INTERNAL_ELEMENTS;
  array->data = array->internal_storage;
  return array;
}

void array_free(array_t *array) {
  if (!array)
    return;

  if (array->data != array->internal_storage)
    free(array->data);

  osi_free(array);
}

void *array_ptr(const array_t *array) {
  return array_at(array, 0);
}

void *array_at(const array_t *array, size_t index) {
  assert(array != NULL);
  assert(index < array->length);
  return array->data + (index * array->element_size);
}

size_t array_length(const array_t *array) {
  assert(array != NULL);
  return array->length;
}

bool array_append_value(array_t *array, uint32_t value) {
  return array_append_ptr(array, &value);
}

bool array_append_ptr(array_t *array, void *data) {
  assert(array != NULL);
  assert(data != NULL);

  if (array->length == array->capacity && !grow(array)) {
    LOG_ERROR(LOG_TAG, "%s unable to grow array past current capacity of %zu elements of size %zu.", __func__, array->capacity, array->element_size);
    return false;
  }

  ++array->length;
  memcpy(array_at(array, array->length - 1), data, array->element_size);
  return true;
}

static bool grow(array_t *array) {
  const size_t new_capacity = array->capacity + (array->capacity / 2);
  const bool is_moving = (array->data == array->internal_storage);

  void *new_data = realloc(is_moving ? NULL : array->data, new_capacity * array->element_size);
  if (!new_data)
    return false;

  if (is_moving)
    memcpy(new_data, array->internal_storage, array->length * array->element_size);

  array->data = new_data;
  array->capacity = new_capacity;
  return true;
}
