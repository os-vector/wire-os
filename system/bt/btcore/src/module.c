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

#define LOG_TAG "bt_core_module"

#include <assert.h>
#include <dlfcn.h>
#include <pthread.h>
#include <string.h>

#include "btcore/include/module.h"
#include "osi/include/allocator.h"
#include "osi/include/hash_functions.h"
#include "osi/include/hash_map.h"
#include "osi/include/log.h"
#include "osi/include/osi.h"

typedef enum {
  MODULE_STATE_NONE = 0,
  MODULE_STATE_INITIALIZED = 1,
  MODULE_STATE_STARTED = 2,
  MODULE_STATE_STARTUP_ERROR = 3
} module_state_t;

static const size_t number_of_metadata_buckets = 42;
static hash_map_t *metadata;
// Include this lock for now for correctness, while the startup sequence is being refactored
static pthread_mutex_t metadata_lock;

static bool call_lifecycle_function(module_lifecycle_fn function);
static module_state_t get_module_state(const module_t *module);
static void set_module_state(const module_t *module, module_state_t state);

void module_management_start(void) {
  metadata = hash_map_new(
    number_of_metadata_buckets,
    hash_function_pointer,
    NULL,
    osi_free,
    NULL
  );

  pthread_mutex_init(&metadata_lock, NULL);
}

void module_management_stop(void) {
  if (!metadata)
    return;

  hash_map_free(metadata);
  metadata = NULL;

  pthread_mutex_destroy(&metadata_lock);
}

const module_t *get_module(const char *name) {
  module_t* module = (module_t *)dlsym(RTLD_DEFAULT, name);
  assert(module);
  return module;
}

bool module_init(const module_t *module) {
  assert(metadata != NULL);
  assert(module != NULL);
  assert(get_module_state(module) == MODULE_STATE_NONE);

  LOG_INFO(LOG_TAG, "%s Initializing module \"%s\"", __func__, module->name);
  if (!call_lifecycle_function(module->init)) {
    LOG_ERROR(LOG_TAG, "%s Failed to initialize module \"%s\"",
              __func__, module->name);
    return false;
  }
  LOG_INFO(LOG_TAG, "%s Initialized module \"%s\"", __func__, module->name);

  set_module_state(module, MODULE_STATE_INITIALIZED);
  return true;
}

bool module_start_up(const module_t *module) {
  assert(metadata != NULL);
  assert(module != NULL);
  // TODO(zachoverflow): remove module->init check once automagic order/call is in place.
  // This hack is here so modules which don't require init don't have to have useless calls
  // as we're converting the startup sequence.
  assert(get_module_state(module) == MODULE_STATE_INITIALIZED || module->init == NULL);

  LOG_INFO(LOG_TAG, "%s Starting module \"%s\"", __func__, module->name);
  if (!call_lifecycle_function(module->start_up)) {
    LOG_ERROR(LOG_TAG, "%s failed to start up \"%s\"", __func__, module->name);
    set_module_state(module, MODULE_STATE_STARTUP_ERROR);
    return false;
  }
  LOG_INFO(LOG_TAG, "%s Started module \"%s\"", __func__, module->name);

  set_module_state(module, MODULE_STATE_STARTED);
  return true;
}

void module_shut_down(const module_t *module) {
  assert(metadata != NULL);
  assert(module != NULL);
  module_state_t state = get_module_state(module);

  // Only something to do if the module was actually started
  if (state < MODULE_STATE_STARTED)
    return;

  LOG_INFO(LOG_TAG, "%s Shutting down module \"%s\"", __func__, module->name);
  if (!call_lifecycle_function(module->shut_down)) {
    LOG_ERROR(LOG_TAG, "%s Failed to shutdown module \"%s\". Continuing anyway.",
              __func__, module->name);
  }
  LOG_INFO(LOG_TAG, "%s Shutdown of module \"%s\" completed",
           __func__, module->name);

  set_module_state(module, MODULE_STATE_INITIALIZED);
}

void module_clean_up(const module_t *module) {
  assert(metadata != NULL);
  assert(module != NULL);
  module_state_t state = get_module_state(module);
  assert(state <= MODULE_STATE_INITIALIZED);

  // Only something to do if the module was actually initialized
  if (state < MODULE_STATE_INITIALIZED)
    return;

  LOG_INFO(LOG_TAG, "%s Cleaning up module \"%s\"", __func__, module->name);
  if (!call_lifecycle_function(module->clean_up)) {
    LOG_ERROR(LOG_TAG, "%s Failed to cleanup module \"%s\". Continuing anyway.",
              __func__, module->name);
  }
  LOG_INFO(LOG_TAG, "%s Cleanup of module \"%s\" completed",
           __func__, module->name);

  set_module_state(module, MODULE_STATE_NONE);
}

static bool call_lifecycle_function(module_lifecycle_fn function) {
  // A NULL lifecycle function means it isn't needed, so assume success
  if (!function)
    return true;

  future_t *future = function();

  // A NULL future means synchronous success
  if (!future)
    return true;

  // Otherwise fall back to the future
  return future_await(future);
}

static module_state_t get_module_state(const module_t *module) {
  pthread_mutex_lock(&metadata_lock);
  module_state_t *state_ptr = hash_map_get(metadata, module);
  pthread_mutex_unlock(&metadata_lock);

  return state_ptr ? *state_ptr : MODULE_STATE_NONE;
}

static void set_module_state(const module_t *module, module_state_t state) {
  pthread_mutex_lock(&metadata_lock);

  module_state_t *state_ptr = hash_map_get(metadata, module);
  if (!state_ptr) {
    state_ptr = osi_malloc(sizeof(module_state_t));
    hash_map_set(metadata, module, state_ptr);
  }

  pthread_mutex_unlock(&metadata_lock);

  *state_ptr = state;
}

// TODO(zachoverflow): remove when everything modulized
// Temporary callback-wrapper-related code

typedef struct {
  const module_t *module;
  thread_t *lifecycle_thread;
  thread_t *callback_thread; // we don't own this thread
  thread_fn callback;
  bool success;
} callbacked_wrapper_t;

static void run_wrapped_start_up(void *context);
static void post_result_to_callback(void *context);

void module_start_up_callbacked_wrapper(
    const module_t *module,
    thread_t *callback_thread,
    thread_fn callback) {
  callbacked_wrapper_t *wrapper = osi_calloc(sizeof(callbacked_wrapper_t));

  wrapper->module = module;
  wrapper->lifecycle_thread = thread_new("module_wrapper");
  wrapper->callback_thread = callback_thread;
  wrapper->callback = callback;

  // Run the actual module start up
  thread_post(wrapper->lifecycle_thread, run_wrapped_start_up, wrapper);
}

static void run_wrapped_start_up(void *context) {
  assert(context);

  callbacked_wrapper_t *wrapper = context;
  wrapper->success = module_start_up(wrapper->module);

  // Post the result back to the callback
  thread_post(wrapper->callback_thread, post_result_to_callback, wrapper);
}

static void post_result_to_callback(void *context) {
  assert(context);

  callbacked_wrapper_t *wrapper = context;

  // Save the values we need for callback
  void *result = wrapper->success ? FUTURE_SUCCESS : FUTURE_FAIL;
  thread_fn callback = wrapper->callback;

  // Clean up the resources we used
  thread_free(wrapper->lifecycle_thread);
  osi_free(wrapper);

  callback(result);
}
