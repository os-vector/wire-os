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

#define LOG_TAG "bt_osi_wakelock"

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <hardware/bluetooth.h>
#include <inttypes.h>
#include <limits.h>
#include <pthread.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "osi/include/alarm.h"
#include "osi/include/allocator.h"
#include "osi/include/log.h"
#include "osi/include/metrics.h"
#include "osi/include/osi.h"
#include "osi/include/thread.h"
#include "osi/include/wakelock.h"

static bt_os_callouts_t *wakelock_os_callouts = NULL;
static bool is_native = true;

static const clockid_t CLOCK_ID = CLOCK_BOOTTIME;
static const char *WAKE_LOCK_ID = "bluetooth_timer";
static char *DEFAULT_WAKE_LOCK_PATH = "/sys/power/wake_lock";
static char *DEFAULT_WAKE_UNLOCK_PATH = "/sys/power/wake_unlock";
static char *wake_lock_path = NULL;
static char *wake_unlock_path = NULL;
static ssize_t locked_id_len = -1;
static pthread_once_t initialized = PTHREAD_ONCE_INIT;
static int wake_lock_fd = INVALID_FD;
static int wake_unlock_fd = INVALID_FD;

// Wakelock statistics for the "bluetooth_timer"
typedef struct {
  bool is_acquired;
  size_t acquired_count;
  size_t released_count;
  size_t acquired_errors;
  size_t released_errors;
  period_ms_t min_acquired_interval_ms;
  period_ms_t max_acquired_interval_ms;
  period_ms_t last_acquired_interval_ms;
  period_ms_t total_acquired_interval_ms;
  period_ms_t last_acquired_timestamp_ms;
  period_ms_t last_released_timestamp_ms;
  period_ms_t last_reset_timestamp_ms;
  int last_acquired_error;
  int last_released_error;
} wakelock_stats_t;

static wakelock_stats_t wakelock_stats;

// This mutex ensures that the functions that update and dump the statistics
// are executed serially.
static pthread_mutex_t monitor;

static bt_status_t wakelock_acquire_callout(void);
static bt_status_t wakelock_acquire_native(void);
static bt_status_t wakelock_release_callout(void);
static bt_status_t wakelock_release_native(void);
static void wakelock_initialize(void);
static void wakelock_initialize_native(void);
static void reset_wakelock_stats(void);
static void update_wakelock_acquired_stats(bt_status_t acquired_status);
static void update_wakelock_released_stats(bt_status_t released_status);

void wakelock_set_os_callouts(bt_os_callouts_t *callouts)
{
  wakelock_os_callouts = callouts;
  is_native = (wakelock_os_callouts == NULL);
  LOG_INFO(LOG_TAG, "%s set to %s",
           __func__, (is_native)? "native" : "non-native");
}

bool wakelock_acquire(void) {
  pthread_once(&initialized, wakelock_initialize);

  bt_status_t status = BT_STATUS_FAIL;

  if (is_native)
    status = wakelock_acquire_native();
  else
    status = wakelock_acquire_callout();

  update_wakelock_acquired_stats(status);

  if (status != BT_STATUS_SUCCESS)
    LOG_ERROR(LOG_TAG, "%s unable to acquire wake lock: %d", __func__, status);

  return (status == BT_STATUS_SUCCESS);
}

static bt_status_t wakelock_acquire_callout(void) {
  return wakelock_os_callouts->acquire_wake_lock(WAKE_LOCK_ID);
}

static bt_status_t wakelock_acquire_native(void) {
  if (wake_lock_fd == INVALID_FD) {
    LOG_ERROR(LOG_TAG, "%s lock not acquired, invalid fd", __func__);
    return BT_STATUS_PARM_INVALID;
  }

  if (wake_unlock_fd == INVALID_FD) {
    LOG_ERROR(LOG_TAG, "%s not acquiring lock: can't release lock", __func__);
    return BT_STATUS_PARM_INVALID;
  }

  long lock_name_len = strlen(WAKE_LOCK_ID);
  locked_id_len = write(wake_lock_fd, WAKE_LOCK_ID, lock_name_len);
  if (locked_id_len == -1) {
    LOG_ERROR(LOG_TAG, "%s wake lock not acquired: %s",
              __func__, strerror(errno));
    return BT_STATUS_FAIL;
  } else if (locked_id_len < lock_name_len) {
    // TODO (jamuraa): this is weird. maybe we should release and retry.
    LOG_WARN(LOG_TAG, "%s wake lock truncated to %zd chars",
             __func__, locked_id_len);
  }
  return BT_STATUS_SUCCESS;
}

bool wakelock_release(void) {
  pthread_once(&initialized, wakelock_initialize);

  bt_status_t status = BT_STATUS_FAIL;

  if (is_native)
    status = wakelock_release_native();
  else
    status = wakelock_release_callout();

  update_wakelock_released_stats(status);

  return (status == BT_STATUS_SUCCESS);
}

static bt_status_t wakelock_release_callout(void) {
  return wakelock_os_callouts->release_wake_lock(WAKE_LOCK_ID);
}

static bt_status_t wakelock_release_native(void) {
  if (wake_unlock_fd == INVALID_FD) {
    LOG_ERROR(LOG_TAG, "%s lock not released, invalid fd", __func__);
    return BT_STATUS_PARM_INVALID;
  }

  ssize_t wrote_name_len = write(wake_unlock_fd, WAKE_LOCK_ID, locked_id_len);
  if (wrote_name_len == -1) {
    LOG_ERROR(LOG_TAG, "%s can't release wake lock: %s",
              __func__, strerror(errno));
  } else if (wrote_name_len < locked_id_len) {
    LOG_ERROR(LOG_TAG, "%s lock release only wrote %zd, assuming released",
              __func__, wrote_name_len);
  }
  return BT_STATUS_SUCCESS;
}

static void wakelock_initialize(void) {
  pthread_mutex_init(&monitor, NULL);
  reset_wakelock_stats();

  if (is_native)
    wakelock_initialize_native();
}

static void wakelock_initialize_native(void) {
  LOG_DEBUG(LOG_TAG, "%s opening wake locks", __func__);

  if (!wake_lock_path)
    wake_lock_path = DEFAULT_WAKE_LOCK_PATH;

  wake_lock_fd = open(wake_lock_path, O_RDWR | O_CLOEXEC);
  if (wake_lock_fd == INVALID_FD) {
    LOG_ERROR(LOG_TAG, "%s can't open wake lock %s: %s",
              __func__, wake_lock_path, strerror(errno));
  }

  if (!wake_unlock_path)
    wake_unlock_path = DEFAULT_WAKE_UNLOCK_PATH;

  wake_unlock_fd = open(wake_unlock_path, O_RDWR | O_CLOEXEC);
  if (wake_unlock_fd == INVALID_FD) {
    LOG_ERROR(LOG_TAG, "%s can't open wake unlock %s: %s",
              __func__, wake_unlock_path, strerror(errno));
  }
}

void wakelock_cleanup(void) {
  if (wake_lock_path && wake_lock_path != DEFAULT_WAKE_LOCK_PATH)
    osi_free_and_reset((void **)&wake_lock_path);

  if (wake_unlock_path && wake_unlock_path != DEFAULT_WAKE_UNLOCK_PATH)
    osi_free_and_reset((void **)&wake_unlock_path);

  initialized = PTHREAD_ONCE_INIT;
  pthread_mutex_destroy(&monitor);
}

void wakelock_set_paths(const char *lock_path, const char *unlock_path) {
  if (lock_path) {
    if (wake_lock_path && wake_lock_path != DEFAULT_WAKE_LOCK_PATH)
      osi_free(wake_lock_path);
    wake_lock_path = osi_strndup(lock_path, PATH_MAX);
  }

  if (unlock_path) {
    if (wake_unlock_path && wake_unlock_path != DEFAULT_WAKE_UNLOCK_PATH)
      osi_free(wake_unlock_path);
    wake_unlock_path = osi_strndup(unlock_path, PATH_MAX);
  }
}

static period_ms_t now(void) {
  struct timespec ts;
  if (clock_gettime(CLOCK_ID, &ts) == -1) {
    LOG_ERROR(LOG_TAG, "%s unable to get current time: %s",
              __func__, strerror(errno));
    return 0;
  }

  return (ts.tv_sec * 1000LL) + (ts.tv_nsec / 1000000LL);
}

// Reset the Bluetooth wakelock statistics.
// This function is thread-safe.
static void reset_wakelock_stats(void) {
  pthread_mutex_lock(&monitor);

  wakelock_stats.is_acquired = false;
  wakelock_stats.acquired_count = 0;
  wakelock_stats.released_count = 0;
  wakelock_stats.acquired_errors = 0;
  wakelock_stats.released_errors = 0;
  wakelock_stats.min_acquired_interval_ms = 0;
  wakelock_stats.max_acquired_interval_ms = 0;
  wakelock_stats.last_acquired_interval_ms = 0;
  wakelock_stats.total_acquired_interval_ms = 0;
  wakelock_stats.last_acquired_timestamp_ms = 0;
  wakelock_stats.last_released_timestamp_ms = 0;
  wakelock_stats.last_reset_timestamp_ms = now();

  pthread_mutex_unlock(&monitor);
}

//
// Update the Bluetooth acquire wakelock statistics.
//
// This function should be called every time when the wakelock is acquired.
// |acquired_status| is the status code that was return when the wakelock was
// acquired.
// This function is thread-safe.
//
static void update_wakelock_acquired_stats(bt_status_t acquired_status) {
  const period_ms_t now_ms = now();

  pthread_mutex_lock(&monitor);

  if (acquired_status != BT_STATUS_SUCCESS) {
    wakelock_stats.acquired_errors++;
    wakelock_stats.last_acquired_error = acquired_status;
  }

  if (wakelock_stats.is_acquired) {
    pthread_mutex_unlock(&monitor);
    return;
  }

  wakelock_stats.is_acquired = true;
  wakelock_stats.acquired_count++;
  wakelock_stats.last_acquired_timestamp_ms = now_ms;

  pthread_mutex_unlock(&monitor);

#ifdef ANDROID
  metrics_wake_event(WAKE_EVENT_ACQUIRED, NULL, WAKE_LOCK_ID, now_ms);
#endif
}

//
// Update the Bluetooth release wakelock statistics.
//
// This function should be called every time when the wakelock is released.
// |released_status| is the status code that was return when the wakelock was
// released.
// This function is thread-safe.
//
static void update_wakelock_released_stats(bt_status_t released_status) {
  const period_ms_t now_ms = now();

  pthread_mutex_lock(&monitor);

  if (released_status != BT_STATUS_SUCCESS) {
    wakelock_stats.released_errors++;
    wakelock_stats.last_released_error = released_status;
  }

  if (!wakelock_stats.is_acquired) {
    pthread_mutex_unlock(&monitor);
    return;
  }

  wakelock_stats.is_acquired = false;
  wakelock_stats.released_count++;
  wakelock_stats.last_released_timestamp_ms = now_ms;

  // Compute the acquired interval and update the statistics
  period_ms_t delta_ms = now_ms - wakelock_stats.last_acquired_timestamp_ms;
  if (delta_ms < wakelock_stats.min_acquired_interval_ms ||
      wakelock_stats.released_count == 1) {
    wakelock_stats.min_acquired_interval_ms = delta_ms;
  }
  if (delta_ms > wakelock_stats.max_acquired_interval_ms) {
    wakelock_stats.max_acquired_interval_ms = delta_ms;
  }
  wakelock_stats.last_acquired_interval_ms = delta_ms;
  wakelock_stats.total_acquired_interval_ms += delta_ms;

  pthread_mutex_unlock(&monitor);

#ifdef ANDROID
  metrics_wake_event(WAKE_EVENT_RELEASED, NULL, WAKE_LOCK_ID, now_ms);
#endif
}

void wakelock_debug_dump(int fd) {
  const period_ms_t now_ms = now();

  // Need to keep track for lock errors - e.g., the "monitor" mutex
  // might not be initialized
  const int lock_error = pthread_mutex_lock(&monitor);

  // Compute the last acquired interval if the wakelock is still acquired
  period_ms_t delta_ms = 0;
  period_ms_t last_interval = wakelock_stats.last_acquired_interval_ms;
  period_ms_t min_interval = wakelock_stats.min_acquired_interval_ms;
  period_ms_t max_interval = wakelock_stats.max_acquired_interval_ms;
  period_ms_t ave_interval = 0;

  if (wakelock_stats.is_acquired) {
    delta_ms = now_ms - wakelock_stats.last_acquired_timestamp_ms;
    if (delta_ms > max_interval)
      max_interval = delta_ms;
    if (delta_ms < min_interval)
      min_interval = delta_ms;
    last_interval = delta_ms;
  }
  period_ms_t total_interval =
    wakelock_stats.total_acquired_interval_ms + delta_ms;

  if (wakelock_stats.acquired_count > 0)
    ave_interval = total_interval / wakelock_stats.acquired_count;

  dprintf(fd, "\nBluetooth Wakelock Statistics:\n");
  dprintf(fd, "  Is acquired                    : %s\n",
          wakelock_stats.is_acquired? "true" : "false");
  dprintf(fd, "  Acquired/released count        : %zu / %zu\n",
          wakelock_stats.acquired_count, wakelock_stats.released_count);
  dprintf(fd, "  Acquired/released error count  : %zu / %zu\n",
          wakelock_stats.acquired_errors, wakelock_stats.released_errors);
  dprintf(fd, "  Last acquire/release error code: %d / %d\n",
          wakelock_stats.last_acquired_error, wakelock_stats.last_released_error);
  dprintf(fd, "  Last acquired time (ms)        : %llu\n",
          (unsigned long long)last_interval);
  dprintf(fd, "  Acquired time min/max/avg (ms) : %llu / %llu / %llu\n",
          (unsigned long long)min_interval, (unsigned long long)max_interval,
          (unsigned long long)ave_interval);
  dprintf(fd, "  Total acquired time (ms)       : %llu\n",
          (unsigned long long)total_interval);
  dprintf(fd, "  Total run time (ms)            : %llu\n",
          (unsigned long long)(now_ms - wakelock_stats.last_reset_timestamp_ms));

  if (lock_error == 0)
    pthread_mutex_unlock(&monitor);
}
