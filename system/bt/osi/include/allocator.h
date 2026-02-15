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

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

typedef void *(*alloc_fn)(size_t size);
typedef void (*free_fn)(void *ptr);

typedef struct {
  alloc_fn alloc;
  free_fn  free;
} allocator_t;

// allocator_t abstractions for the osi_*alloc and osi_free functions
extern const allocator_t allocator_malloc;
extern const allocator_t allocator_calloc;

char *osi_strdup(const char *str);
char *osi_strndup(const char *str, size_t len);

void *osi_malloc(size_t size);
void *osi_calloc(size_t size);
void osi_free(void *ptr);

// Free a buffer that was previously allocated with function |osi_malloc|
// or |osi_calloc| and reset the pointer to that buffer to NULL.
// |p_ptr| is a pointer to the buffer pointer to be reset.
// |p_ptr| cannot be NULL.
void osi_free_and_reset(void **p_ptr);
