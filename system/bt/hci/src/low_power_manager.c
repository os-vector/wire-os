/******************************************************************************
 *  Copyright (c) 2016, The Linux Foundation. All rights reserved.
 *
 *  Not a contribution.
 ******************************************************************************/
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

#define LOG_TAG "bt_low_power_manager"

#include "low_power_manager.h"

#include <assert.h>
#include <stdint.h>

#include "osi/include/alarm.h"
#include "osi/include/log.h"
#include "osi/include/osi.h"
#include "osi/include/thread.h"
#include "vendor.h"

typedef enum {
  LPM_DISABLED = 0,
  LPM_ENABLED,
  LPM_ENABLING,
  LPM_DISABLING
} low_power_mode_state_t;

typedef enum {
  LPM_WAKE_DEASSERTED = 0,
  LPM_WAKE_W4_TX_DONE,
  LPM_WAKE_W4_TIMEOUT,
  LPM_WAKE_ASSERTED,
} wake_state_t;

// Our interface and modules we import
static const low_power_manager_t interface;
static const vendor_t *vendor;

static void vendor_enable_disable_callback(bool success);

static void event_disable(void *context);
static void event_enable(void *context);
static void event_wake_assert(void *context);
static void event_allow_device_sleep(void *context);
static void event_idle_timeout(void *context);

static void reset_state();
void start_idle_timer(bool check_LPM);
void stop_idle_timer();

static thread_fn event_functions[] = {
  event_disable,
  event_enable,
  event_wake_assert,
  event_allow_device_sleep
};

static thread_t *thread;
static low_power_mode_state_t state;
static wake_state_t wake_state;
static uint32_t idle_timeout_ms;
static alarm_t *idle_alarm;
static bool transmit_is_done;
static void wake_deassert();

// Interface functions

static void init(thread_t *post_thread) {
  assert(post_thread != NULL);
  thread = post_thread;

  vendor->set_callback(VENDOR_SET_LPM_MODE, vendor_enable_disable_callback);

  idle_alarm = alarm_new("hci.idle");
  if (!idle_alarm) {
    LOG_ERROR(LOG_TAG, "%s could not create idle alarm.", __func__);
  }

  reset_state();
}

static void cleanup() {
  reset_state();
  alarm_free(idle_alarm);
  idle_alarm = NULL;
}

static void post_command(low_power_command_t command) {
  if (command > LPM_WAKE_DEASSERT) {
    LOG_ERROR(LOG_TAG, "%s unknown low power command %d", __func__, command);
    return;
  }

  thread_post(thread, event_functions[command], NULL);
}

static void wake_assert() {
  if (state != LPM_DISABLED) {
    stop_idle_timer();

    uint8_t new_state = BT_VND_LPM_WAKE_ASSERT;
    vendor->send_command(VENDOR_SET_LPM_WAKE_STATE, &new_state);
    wake_state = LPM_WAKE_ASSERTED;
  }

  // TODO(zachoverflow): investigate this interaction. If someone above
  // HCI asserts wake, we'll wait until we transmit before deasserting.
  // That doesn't seem quite right.
  transmit_is_done = false;
}

static void transmit_done() {
  transmit_is_done = true;
  if (wake_state == LPM_WAKE_W4_TX_DONE) {
    wake_state = LPM_WAKE_W4_TIMEOUT;
    start_idle_timer(true);
  }
}

// Internal functions

static void enable(bool enable) {
  if (state == LPM_DISABLING) {
    if (enable) {
      LOG_ERROR(LOG_TAG, "%s still processing prior disable request, cannot enable.", __func__);
    } else {
      LOG_WARN(LOG_TAG, "%s still processing prior disable request, ignoring new request to disable.", __func__);
    }
  } else if (state == LPM_ENABLING) {
    if (enable) {
      LOG_ERROR(LOG_TAG, "%s still processing prior enable request, ignoring new request to enable.", __func__);
    } else {
      LOG_WARN(LOG_TAG, "%s still processing prior enable request, cannot disable.", __func__);
    }
  } else if (state == LPM_ENABLED && enable) {
    LOG_INFO(LOG_TAG, "%s already enabled.", __func__);
  } else if (state == LPM_DISABLED && !enable) {
    LOG_INFO(LOG_TAG, "%s already disabled.", __func__);
  } else {
    uint8_t command = enable ? BT_VND_LPM_ENABLE : BT_VND_LPM_DISABLE;
    state = enable ? LPM_ENABLING : LPM_DISABLING;
    if (state == LPM_ENABLING)
        vendor->send_command(VENDOR_GET_LPM_IDLE_TIMEOUT, &idle_timeout_ms);
    vendor->send_async_command(VENDOR_SET_LPM_MODE, &command);
  }
}

static void allow_device_sleep() {
  if (state == LPM_ENABLED && wake_state == LPM_WAKE_ASSERTED) {
    if (transmit_is_done) {
      wake_state = LPM_WAKE_W4_TIMEOUT;
      start_idle_timer(true);
    } else {
      wake_state = LPM_WAKE_W4_TX_DONE;
    }
  }
}

static void wake_deassert() {
  if (state == LPM_ENABLED && transmit_is_done) {
    uint8_t new_state = BT_VND_LPM_WAKE_DEASSERT;
    vendor->send_command(VENDOR_SET_LPM_WAKE_STATE, &new_state);
    wake_state = LPM_WAKE_DEASSERTED;
  }
}

static void reset_state() {
  state = LPM_DISABLED;
  wake_state = LPM_WAKE_DEASSERTED;
  transmit_is_done = true;
  stop_idle_timer();
}

static void idle_timer_expired(UNUSED_ATTR void *context) {
  if (state == LPM_ENABLED && wake_state == LPM_WAKE_W4_TIMEOUT)
    thread_post(thread, event_idle_timeout, NULL);
}

void start_idle_timer(bool check_LPM) {
  if (state == LPM_ENABLED || !check_LPM) {
    if (idle_timeout_ms == 0) {
       wake_deassert();
    }
    else {
       alarm_set(idle_alarm, idle_timeout_ms, idle_timer_expired, NULL);
    }

    LOG_VERBOSE(LOG_TAG,"%s check_LPM = %d", __func__, check_LPM);
  }
}

void stop_idle_timer() {
  alarm_cancel(idle_alarm);
  LOG_VERBOSE(LOG_TAG, "%s", __func__);
}

static void event_disable(UNUSED_ATTR void *context) {
  enable(false);
}

static void event_enable(UNUSED_ATTR void *context) {
  enable(true);
}

static void event_wake_assert(UNUSED_ATTR void *context) {
  wake_assert();
}

static void event_allow_device_sleep(UNUSED_ATTR void *context) {
  allow_device_sleep();
}

static void event_idle_timeout(UNUSED_ATTR void *context) {
  wake_deassert();
}

static void vendor_enable_disable_callback(bool success) {
  if (success)
    state = (state == LPM_ENABLING) ? LPM_ENABLED : LPM_DISABLED;
  else
    state = (state == LPM_ENABLING) ? LPM_DISABLED : LPM_ENABLED;

  if (state == LPM_DISABLED) {
    reset_state();
  }
}

static const low_power_manager_t interface = {
  init,
  cleanup,
  post_command,
  wake_assert,
  transmit_done,
  start_idle_timer,
  stop_idle_timer
};

const low_power_manager_t *low_power_manager_get_interface() {
  vendor = vendor_get_interface();
  return &interface;
}

const low_power_manager_t *low_power_manager_get_test_interface(const vendor_t *vendor_interface) {
  vendor = vendor_interface;
  return &interface;
}
