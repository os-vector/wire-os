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

#define LOG_TAG "bt_stack_manager"

#include "stack_manager.h"

#include <hardware/bluetooth.h>

#include "btcore/include/module.h"
#include "btcore/include/osi_module.h"
#include "btif_api.h"
#include "btif_common.h"
#include "device/include/controller.h"
#include "osi/include/log.h"
#include "osi/include/osi.h"
#include "osi/include/semaphore.h"
#include "osi/include/thread.h"

// Temp includes
#include "btif_config.h"
#include "btif_profile_queue.h"
#include "bt_utils.h"
#if (defined(BTC_INCLUDED) && BTC_INCLUDED == TRUE)
#include "btc_common.h"
#endif /*BTC*/

static thread_t *management_thread;
extern void bt_utils_cleanup( void);

// If initialized, any of the bluetooth API functions can be called.
// (e.g. turning logging on and off, enabling/disabling the stack, etc)
static bool stack_is_initialized;
// If running, the stack is fully up and able to bluetooth.
static bool stack_is_running;

static void event_init_stack(void *context);
static void event_start_up_stack(void *context);
static void event_shut_down_stack(void *context);
static void event_clean_up_stack(void *context);

static void event_signal_stack_up(void *context);
static void event_signal_stack_down(void *context);

// Unvetted includes/imports, etc which should be removed or vetted in the future
static future_t *hack_future;
void bte_main_enable();
void btif_thread_post(thread_fn func, void *context);
// End unvetted section

// Interface functions

static void init_stack(void) {
  // This is a synchronous process. Post it to the thread though, so
  // state modification only happens there. Using the thread to perform
  // all stack operations ensures that the operations are done serially
  // and do not overlap.
  semaphore_t *semaphore = semaphore_new(0);
  thread_post(management_thread, event_init_stack, semaphore);
  semaphore_wait(semaphore);
  semaphore_free(semaphore);
}

static void start_up_stack_async(void) {
  thread_post(management_thread, event_start_up_stack, NULL);
}

static void shut_down_stack_async(void) {
  thread_post(management_thread, event_shut_down_stack, NULL);
}

static void clean_up_stack(void) {
  // This is a synchronous process. Post it to the thread though, so
  // state modification only happens there.
  semaphore_t *semaphore = semaphore_new(0);
  thread_post(management_thread, event_clean_up_stack, semaphore);
  semaphore_wait(semaphore);
  semaphore_free(semaphore);
}

static bool get_stack_is_running(void) {
  return stack_is_running;
}

// Internal functions

// Synchronous function to initialize the stack
static void event_init_stack(void *context) {
  semaphore_t *semaphore = (semaphore_t *)context;

  LOG_INFO(LOG_TAG, "%s is initializing the stack", __func__);

 if (stack_is_initialized) {
   LOG_INFO(LOG_TAG, "%s found the stack already in initialized state",
            __func__);
 } else {
    module_management_start();

    module_init(get_module(OSI_MODULE));
    module_init(get_module(BT_UTILS_MODULE));
    module_init(get_module(BTIF_CONFIG_MODULE));
    btif_init_bluetooth();

#if (defined(BTC_INCLUDED) && BTC_INCLUDED == TRUE)
    btc_init();
#endif /*BTC*/

    // stack init is synchronous, so no waiting necessary here
    stack_is_initialized = true;
  }

  LOG_INFO(LOG_TAG, "%s finished", __func__);

  if (semaphore)
    semaphore_post(semaphore);
}

static void ensure_stack_is_initialized(void) {
  if (!stack_is_initialized) {
    LOG_WARN(LOG_TAG, "%s found the stack was uninitialized. Initializing now.", __func__);
    // No semaphore needed since we are calling it directly
    event_init_stack(NULL);
  }
}

// Synchronous function to start up the stack
static void event_start_up_stack(UNUSED_ATTR void *context) {
  if (stack_is_running) {
    LOG_INFO(LOG_TAG, "%s stack already brought up", __func__);
    return;
  }

  ensure_stack_is_initialized();

  init_vnd_Logger();
  LOG_INFO(LOG_TAG, "%s is bringing up the stack", __func__);
  future_t *local_hack_future = future_new();
  hack_future = local_hack_future;

  // Include this for now to put btif config into a shutdown-able state
  module_start_up(get_module(BTIF_CONFIG_MODULE));
  bte_main_enable();

  if (future_await(local_hack_future) != FUTURE_SUCCESS) {
    LOG_ERROR(LOG_TAG, "%s failed to start up the stack", __func__);
    stack_is_running = true; // So stack shutdown actually happens
    event_shut_down_stack(NULL);
    return;
  }

  stack_is_running = true;
  LOG_INFO(LOG_TAG, "%s finished", __func__);

#if (defined(BTC_INCLUDED) && BTC_INCLUDED == TRUE)
  btc_hci_init();
#endif /*BTC*/

  btif_thread_post(event_signal_stack_up, NULL);
}

// Synchronous function to shut down the stack
static void event_shut_down_stack(UNUSED_ATTR void *context) {
  if (!stack_is_running) {
    LOG_INFO(LOG_TAG, "%s stack is already brought down", __func__);
    return;
  }

  LOG_INFO(LOG_TAG, "%s is bringing down the stack", __func__);
  future_t *local_hack_future = future_new();
  hack_future = local_hack_future;
  stack_is_running = false;

#if (defined(BTC_INCLUDED) && BTC_INCLUDED == TRUE)
  btc_hci_deinit();
#endif /*BTC*/

  btif_disable_bluetooth();
  module_shut_down(get_module(BTIF_CONFIG_MODULE));

  future_await(local_hack_future);
  module_shut_down(get_module(CONTROLLER_MODULE)); // Doesn't do any work, just puts it in a restartable state

  LOG_INFO(LOG_TAG, "%s finished", __func__);
  hack_future = future_new();
  btif_thread_post(event_signal_stack_down, NULL);
  future_await(hack_future);
}

static void ensure_stack_is_not_running(void) {
  if (stack_is_running) {
    LOG_WARN(LOG_TAG, "%s found the stack was still running. Bringing it down now.", __func__);
    event_shut_down_stack(NULL);
  }
}

// Synchronous function to clean up the stack
static void event_clean_up_stack(void *context) {
  if (!stack_is_initialized) {
    LOG_INFO(LOG_TAG, "%s found the stack already in a clean state", __func__);
    goto cleanup;
  }

  ensure_stack_is_not_running();

  LOG_INFO(LOG_TAG, "%s is cleaning up the stack", __func__);
  stack_is_initialized = false;

  btif_cleanup_bluetooth();
  module_clean_up(get_module(BTIF_CONFIG_MODULE));
  module_clean_up(get_module(BT_UTILS_MODULE));
  module_clean_up(get_module(OSI_MODULE));

#if (defined(BTC_INCLUDED) && BTC_INCLUDED == TRUE)
    btc_deinit();
#endif /*BTC*/

  module_management_stop();
  LOG_INFO(LOG_TAG, "%s finished", __func__);

cleanup:
  clean_vnd_logger();
  semaphore_t *semaphore = (semaphore_t *)context;
  if (semaphore)
    semaphore_post(semaphore);
}

static void event_signal_stack_up(UNUSED_ATTR void *context) {
  // Notify BTIF connect queue that we've brought up the stack. It's
  // now time to dispatch all the pending profile connect requests.
  btif_queue_connect_next();
  HAL_CBACK(bt_hal_cbacks, adapter_state_changed_cb, BT_STATE_ON);
#if (defined(BTC_INCLUDED) && BTC_INCLUDED == TRUE)
  btc_post_msg(BLUETOOTH_ON);
#endif /*BTC*/
}

static void event_signal_stack_down(UNUSED_ATTR void *context) {
  HAL_CBACK(bt_hal_cbacks, adapter_state_changed_cb, BT_STATE_OFF);
  future_ready(stack_manager_get_hack_future(), FUTURE_SUCCESS);
#if (defined(BTC_INCLUDED) && BTC_INCLUDED == TRUE)
  btc_post_msg(BLUETOOTH_OFF);
#endif /*BTC*/
}

static void ensure_manager_initialized(void) {
  if (management_thread)
    return;

  management_thread = thread_new("stack_manager");
  if (!management_thread) {
    LOG_ERROR(LOG_TAG, "%s unable to create stack management thread", __func__);
    return;
  }
}

static const stack_manager_t interface = {
  init_stack,
  start_up_stack_async,
  shut_down_stack_async,
  clean_up_stack,

  get_stack_is_running
};

const stack_manager_t *stack_manager_get_interface() {
  ensure_manager_initialized();
  return &interface;
}

future_t *stack_manager_get_hack_future() {
  return hack_future;
}
