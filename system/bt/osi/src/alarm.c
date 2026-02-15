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

#include "include/bt_target.h"

#define LOG_TAG "bt_osi_alarm"

#include "osi/include/alarm.h"

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <malloc.h>
#include <pthread.h>
#include <signal.h>
#include <string.h>
#include <time.h>

#include <hardware/bluetooth.h>

#include "osi/include/allocator.h"
#include "osi/include/fixed_queue.h"
#include "osi/include/list.h"
#include "osi/include/log.h"
#include "osi/include/osi.h"
#include "osi/include/semaphore.h"
#include "osi/include/thread.h"
#include "osi/include/wakelock.h"

// Make callbacks run at high thread priority. Some callbacks are used for audio
// related timer tasks as well as re-transmissions etc. Since we at this point
// cannot differentiate what callback we are dealing with, assume high priority
// for now.
// TODO(eisenbach): Determine correct thread priority (from parent?/per alarm?)
static const int CALLBACK_THREAD_PRIORITY_HIGH = -19;

typedef struct {
  size_t count;
  period_ms_t total_ms;
  period_ms_t max_ms;
} stat_t;

// Alarm-related information and statistics
typedef struct {
  const char *name;
  size_t scheduled_count;
  size_t canceled_count;
  size_t rescheduled_count;
  size_t total_updates;
  period_ms_t last_update_ms;
  stat_t callback_execution;
  stat_t overdue_scheduling;
  stat_t premature_scheduling;
} alarm_stats_t;

struct alarm_t {
  // The lock is held while the callback for this alarm is being executed.
  // It allows us to release the coarse-grained monitor lock while a
  // potentially long-running callback is executing. |alarm_cancel| uses this
  // lock to provide a guarantee to its caller that the callback will not be
  // in progress when it returns.
  pthread_mutex_t callback_lock;
  period_ms_t creation_time;
  period_ms_t period;
  period_ms_t deadline;
  period_ms_t prev_deadline;    // Previous deadline - used for accounting of
                                // periodic timers
  bool is_periodic;
  fixed_queue_t *queue;         // The processing queue to add this alarm to
  alarm_callback_t callback;
  void *data;
  alarm_stats_t stats;
};


// If the next wakeup time is less than this threshold, we should acquire
// a wakelock instead of setting a wake alarm so we're not bouncing in
// and out of suspend frequently. This value is externally visible to allow
// unit tests to run faster. It should not be modified by production code.
int64_t TIMER_INTERVAL_FOR_WAKELOCK_IN_MS = 3000;
static const clockid_t CLOCK_ID = CLOCK_BOOTTIME;
#if defined(KERNEL_MISSING_CLOCK_BOOTTIME_ALARM) && (KERNEL_MISSING_CLOCK_BOOTTIME_ALARM == TRUE)
static const clockid_t CLOCK_ID_ALARM = CLOCK_BOOTTIME;
#else
static const clockid_t CLOCK_ID_ALARM = CLOCK_BOOTTIME_ALARM;
#endif
// This mutex ensures that the |alarm_set|, |alarm_cancel|, and alarm callback
// functions execute serially and not concurrently. As a result, this mutex
// also protects the |alarms| list.
static pthread_mutex_t monitor;
static list_t *alarms;
static timer_t timer;
static timer_t wakeup_timer;
static bool timer_set;

// All alarm callbacks are dispatched from |dispatcher_thread|
static thread_t *dispatcher_thread;
static bool dispatcher_thread_active;
static semaphore_t *alarm_expired;

// Default alarm callback thread and queue
static thread_t *default_callback_thread;
static fixed_queue_t *default_callback_queue;

static alarm_t *alarm_new_internal(const char *name, bool is_periodic);
static bool lazy_initialize(void);
static period_ms_t now(void);
static void alarm_set_internal(alarm_t *alarm, period_ms_t period,
                               alarm_callback_t cb, void *data,
                               fixed_queue_t *queue);
static void alarm_cancel_internal(alarm_t *alarm);
static void remove_pending_alarm(alarm_t *alarm);
static void schedule_next_instance(alarm_t *alarm);
static void reschedule_root_alarm(void);
static void alarm_queue_ready(fixed_queue_t *queue, void *context);
static void timer_callback(void *data);
static void callback_dispatch(void *context);
static bool timer_create_internal(const clockid_t clock_id, timer_t *timer);
static void update_scheduling_stats(alarm_stats_t *stats,
                                    period_ms_t now_ms,
                                    period_ms_t deadline_ms,
                                    period_ms_t execution_delta_ms);

static void update_stat(stat_t *stat, period_ms_t delta)
{
  if (stat->max_ms < delta)
    stat->max_ms = delta;
  stat->total_ms += delta;
  stat->count++;
}

alarm_t *alarm_new(const char *name) {
  return alarm_new_internal(name, false);
}

alarm_t *alarm_new_periodic(const char *name) {
  return alarm_new_internal(name, true);
}

static alarm_t *alarm_new_internal(const char *name, bool is_periodic) {
  // Make sure we have a list we can insert alarms into.
  if (!alarms && !lazy_initialize()) {
    assert(false); // if initialization failed, we should not continue
    return NULL;
  }

  pthread_mutexattr_t attr;
  pthread_mutexattr_init(&attr);

  alarm_t *ret = osi_calloc(sizeof(alarm_t));

  // Make this a recursive mutex to make it safe to call |alarm_cancel| from
  // within the callback function of the alarm.
  int error = pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
  if (error) {
    LOG_ERROR(LOG_TAG, "%s unable to create a recursive mutex: %s",
              __func__, strerror(error));
    goto error;
  }

  error = pthread_mutex_init(&ret->callback_lock, &attr);
  if (error) {
    LOG_ERROR(LOG_TAG, "%s unable to initialize mutex: %s",
              __func__, strerror(error));
    goto error;
  }

  ret->is_periodic = is_periodic;

  alarm_stats_t *stats = &ret->stats;
  stats->name = osi_strdup(name);
  // NOTE: The stats were reset by osi_calloc() above

  pthread_mutexattr_destroy(&attr);
  return ret;

error:
  pthread_mutexattr_destroy(&attr);
  osi_free(ret);
  return NULL;
}

void alarm_free(alarm_t *alarm) {
  if (!alarm)
    return;

  alarm_cancel(alarm);
  pthread_mutex_destroy(&alarm->callback_lock);
  osi_free((void *)alarm->stats.name);
  osi_free(alarm);
}

period_ms_t alarm_get_remaining_ms(const alarm_t *alarm) {
  assert(alarm != NULL);
  period_ms_t remaining_ms = 0;
  period_ms_t just_now = now();

  pthread_mutex_lock(&monitor);
  if (alarm->deadline > just_now)
    remaining_ms = alarm->deadline - just_now;
  pthread_mutex_unlock(&monitor);

  return remaining_ms;
}

void alarm_set(alarm_t *alarm, period_ms_t interval_ms,
               alarm_callback_t cb, void *data) {
  alarm_set_on_queue(alarm, interval_ms, cb, data, default_callback_queue);
}

void alarm_set_on_queue(alarm_t *alarm, period_ms_t interval_ms,
                        alarm_callback_t cb, void *data,
                        fixed_queue_t *queue) {
  assert(queue != NULL);
  alarm_set_internal(alarm, interval_ms, cb, data, queue);
}

// Runs in exclusion with alarm_cancel and timer_callback.
static void alarm_set_internal(alarm_t *alarm, period_ms_t period,
                               alarm_callback_t cb, void *data,
                               fixed_queue_t *queue) {
  assert(alarms != NULL);
  assert(alarm != NULL);
  assert(cb != NULL);

  pthread_mutex_lock(&monitor);

  alarm->creation_time = now();
  alarm->period = period;
  alarm->queue = queue;
  alarm->callback = cb;
  alarm->data = data;

  schedule_next_instance(alarm);
  alarm->stats.scheduled_count++;

  pthread_mutex_unlock(&monitor);
}

void alarm_cancel(alarm_t *alarm) {
  assert(alarms != NULL);
  if (!alarm)
    return;

  pthread_mutex_lock(&monitor);
  alarm_cancel_internal(alarm);
  pthread_mutex_unlock(&monitor);

  // If the callback for |alarm| is in progress, wait here until it completes.
  pthread_mutex_lock(&alarm->callback_lock);
  pthread_mutex_unlock(&alarm->callback_lock);
}

// Internal implementation of canceling an alarm.
// The caller must hold the |monitor| lock.
static void alarm_cancel_internal(alarm_t *alarm) {
  bool needs_reschedule = (!list_is_empty(alarms) && list_front(alarms) == alarm);

  remove_pending_alarm(alarm);

  alarm->deadline = 0;
  alarm->prev_deadline = 0;
  alarm->callback = NULL;
  alarm->data = NULL;
  alarm->stats.canceled_count++;
  alarm->queue = NULL;

  if (needs_reschedule)
    reschedule_root_alarm();
}

bool alarm_is_scheduled(const alarm_t *alarm) {
  if ((alarms == NULL) || (alarm == NULL))
    return false;
  return (alarm->callback != NULL);
}

void alarm_cleanup(void) {
  // If lazy_initialize never ran there is nothing else to do
  if (!alarms)
    return;

  dispatcher_thread_active = false;
  semaphore_post(alarm_expired);
  thread_free(dispatcher_thread);
  dispatcher_thread = NULL;

  pthread_mutex_lock(&monitor);

  fixed_queue_free(default_callback_queue, NULL);
  default_callback_queue = NULL;
  thread_free(default_callback_thread);
  default_callback_thread = NULL;

  timer_delete(wakeup_timer);
  timer_delete(timer);
  semaphore_free(alarm_expired);
  alarm_expired = NULL;

  list_free(alarms);
  alarms = NULL;

  pthread_mutex_unlock(&monitor);
  pthread_mutex_destroy(&monitor);
}

static bool lazy_initialize(void) {
  assert(alarms == NULL);

  // timer_t doesn't have an invalid value so we must track whether
  // the |timer| variable is valid ourselves.
  bool timer_initialized = false;
  bool wakeup_timer_initialized = false;

  pthread_mutex_init(&monitor, NULL);

  alarms = list_new(NULL);
  if (!alarms) {
    LOG_ERROR(LOG_TAG, "%s unable to allocate alarm list.", __func__);
    goto error;
  }

  if (!timer_create_internal(CLOCK_ID, &timer))
    goto error;
  timer_initialized = true;

  if (!timer_create_internal(CLOCK_ID_ALARM, &wakeup_timer))
    goto error;
  wakeup_timer_initialized = true;

  alarm_expired = semaphore_new(0);
  if (!alarm_expired) {
    LOG_ERROR(LOG_TAG, "%s unable to create alarm expired semaphore", __func__);
    goto error;
  }

  default_callback_thread = thread_new_sized("alarm_default_callbacks",
                                             SIZE_MAX);
  if (default_callback_thread == NULL) {
    LOG_ERROR(LOG_TAG, "%s unable to create default alarm callbacks thread.",
              __func__);
    goto error;
  }
  thread_set_priority(default_callback_thread, CALLBACK_THREAD_PRIORITY_HIGH);
  default_callback_queue = fixed_queue_new(SIZE_MAX);
  if (default_callback_queue == NULL) {
    LOG_ERROR(LOG_TAG, "%s unable to create default alarm callbacks queue.",
              __func__);
    goto error;
  }
  alarm_register_processing_queue(default_callback_queue,
                                  default_callback_thread);

  dispatcher_thread_active = true;
  dispatcher_thread = thread_new("alarm_dispatcher");
  if (!dispatcher_thread) {
    LOG_ERROR(LOG_TAG, "%s unable to create alarm callback thread.", __func__);
    goto error;
  }

  thread_set_priority(dispatcher_thread, CALLBACK_THREAD_PRIORITY_HIGH);
  thread_post(dispatcher_thread, callback_dispatch, NULL);
  return true;

error:
  fixed_queue_free(default_callback_queue, NULL);
  default_callback_queue = NULL;
  thread_free(default_callback_thread);
  default_callback_thread = NULL;

  thread_free(dispatcher_thread);
  dispatcher_thread = NULL;

  dispatcher_thread_active = false;

  semaphore_free(alarm_expired);
  alarm_expired = NULL;

  if (wakeup_timer_initialized)
    timer_delete(wakeup_timer);

  if (timer_initialized)
    timer_delete(timer);

  list_free(alarms);
  alarms = NULL;

  pthread_mutex_destroy(&monitor);

  return false;
}

static period_ms_t now(void) {
  assert(alarms != NULL);

  struct timespec ts;
  if (clock_gettime(CLOCK_ID, &ts) == -1) {
    LOG_ERROR(LOG_TAG, "%s unable to get current time: %s",
              __func__, strerror(errno));
    return 0;
  }

  return (ts.tv_sec * 1000LL) + (ts.tv_nsec / 1000000LL);
}

// Remove alarm from internal alarm list and the processing queue
// The caller must hold the |monitor| lock.
static void remove_pending_alarm(alarm_t *alarm) {
  list_remove(alarms, alarm);
  while (fixed_queue_try_remove_from_queue(alarm->queue, alarm) != NULL) {
    // Remove all repeated alarm instances from the queue.
    // NOTE: We are defensive here - we shouldn't have repeated alarm instances
  }
}

// Must be called with monitor held
static void schedule_next_instance(alarm_t *alarm) {
  // If the alarm is currently set and it's at the start of the list,
  // we'll need to re-schedule since we've adjusted the earliest deadline.
  bool needs_reschedule = (!list_is_empty(alarms) && list_front(alarms) == alarm);
  if (alarm->callback)
    remove_pending_alarm(alarm);

  // Calculate the next deadline for this alarm
  period_ms_t just_now = now();
  period_ms_t ms_into_period = 0;
  if ((alarm->is_periodic) && (alarm->period != 0))
    ms_into_period = ((just_now - alarm->creation_time) % alarm->period);
  alarm->deadline = just_now + (alarm->period - ms_into_period);

  // Add it into the timer list sorted by deadline (earliest deadline first).
  if (list_is_empty(alarms) ||
      ((alarm_t *)list_front(alarms))->deadline > alarm->deadline) {
    list_prepend(alarms, alarm);
  } else {
    for (list_node_t *node = list_begin(alarms); node != list_end(alarms); node = list_next(node)) {
      list_node_t *next = list_next(node);
      if (next == list_end(alarms) || ((alarm_t *)list_node(next))->deadline > alarm->deadline) {
        list_insert_after(alarms, node, alarm);
        break;
      }
    }
  }

  // If the new alarm has the earliest deadline, we need to re-evaluate our schedule.
  if (needs_reschedule ||
      (!list_is_empty(alarms) && list_front(alarms) == alarm)) {
    reschedule_root_alarm();
  }
}

// NOTE: must be called with monitor lock.
static void reschedule_root_alarm(void) {
  assert(alarms != NULL);

  const bool timer_was_set = timer_set;

  // If used in a zeroed state, disarms the timer.
  struct itimerspec timer_time;
  memset(&timer_time, 0, sizeof(timer_time));

  if (list_is_empty(alarms))
    goto done;

#ifdef ANDROID
  const alarm_t *next = list_front(alarms);
  const int64_t next_expiration = next->deadline - now();
  if (next_expiration < TIMER_INTERVAL_FOR_WAKELOCK_IN_MS) {
    if (!timer_set) {
      if (!wakelock_acquire()) {
        LOG_ERROR(LOG_TAG, "%s unable to acquire wake lock", __func__);
        goto done;
      }
    }

    timer_time.it_value.tv_sec = (next->deadline / 1000);
    timer_time.it_value.tv_nsec = (next->deadline % 1000) * 1000000LL;

    // It is entirely unsafe to call timer_settime(2) with a zeroed timerspec
    // for timers with *_ALARM clock IDs. Although the man page states that the
    // timer would be canceled, the current behavior (as of Linux kernel 3.17)
    // is that the callback is issued immediately. The only way to cancel an
    // *_ALARM timer is to delete the timer. But unfortunately, deleting and
    // re-creating a timer is rather expensive; every timer_create(2) spawns a
    // new thread. So we simply set the timer to fire at the largest possible
    // time.
    //
    // If we've reached this code path, we're going to grab a wake lock and
    // wait for the next timer to fire. In that case, there's no reason to
    // have a pending wakeup timer so we simply cancel it.
    struct itimerspec end_of_time;
    memset(&end_of_time, 0, sizeof(end_of_time));
    end_of_time.it_value.tv_sec = (time_t)(1LL << (sizeof(time_t) * 8 - 2));
    timer_settime(wakeup_timer, TIMER_ABSTIME, &end_of_time, NULL);
  } else {
    // WARNING: do not attempt to use relative timers with *_ALARM clock IDs
    // in kernels before 3.17 unless you have the following patch:
    // https://lkml.org/lkml/2014/7/7/576
    struct itimerspec wakeup_time;
    memset(&wakeup_time, 0, sizeof(wakeup_time));


    wakeup_time.it_value.tv_sec = (next->deadline / 1000);
    wakeup_time.it_value.tv_nsec = (next->deadline % 1000) * 1000000LL;
    if (timer_settime(wakeup_timer, TIMER_ABSTIME, &wakeup_time, NULL) == -1)
      LOG_ERROR(LOG_TAG, "%s unable to set wakeup timer: %s",
                __func__, strerror(errno));
  }

done:
  timer_set = timer_time.it_value.tv_sec != 0 || timer_time.it_value.tv_nsec != 0;
  if (timer_was_set && !timer_set) {
    wakelock_release();
  }
#else
  alarm_t *next = list_front(alarms);
  timer_time.it_value.tv_sec = (next->deadline / 1000);
  timer_time.it_value.tv_nsec = (next->deadline % 1000) * 1000000LL;

done:
  timer_set = timer_time.it_value.tv_sec != 0 || timer_time.it_value.tv_nsec != 0;
#endif

  if (timer_settime(timer, TIMER_ABSTIME, &timer_time, NULL) == -1)
    LOG_ERROR(LOG_TAG, "%s unable to set timer: %s", __func__, strerror(errno));

  // If next expiration was in the past (e.g. short timer that got context
  // switched) then the timer might have diarmed itself. Detect this case and
  // work around it by manually signalling the |alarm_expired| semaphore.
  //
  // It is possible that the timer was actually super short (a few
  // milliseconds) and the timer expired normally before we called
  // |timer_gettime|. Worst case, |alarm_expired| is signaled twice for that
  // alarm. Nothing bad should happen in that case though since the callback
  // dispatch function checks to make sure the timer at the head of the list
  // actually expired.
  if (timer_set) {
    struct itimerspec time_to_expire;
    timer_gettime(timer, &time_to_expire);
    if (time_to_expire.it_value.tv_sec == 0 &&
        time_to_expire.it_value.tv_nsec == 0) {
      LOG_DEBUG(LOG_TAG, "%s alarm expiration too close for posix timers, switching to guns", __func__);
      semaphore_post(alarm_expired);
    }
  }
}

void alarm_register_processing_queue(fixed_queue_t *queue, thread_t *thread) {
  assert(queue != NULL);
  assert(thread != NULL);

  fixed_queue_register_dequeue(queue, thread_get_reactor(thread),
                               alarm_queue_ready, NULL);
}

void alarm_unregister_processing_queue(fixed_queue_t *queue) {
  assert(alarms != NULL);
  assert(queue != NULL);

  fixed_queue_unregister_dequeue(queue);

  // Cancel all alarms that are using this queue
  pthread_mutex_lock(&monitor);
  for (list_node_t *node = list_begin(alarms); node != list_end(alarms); ) {
    alarm_t *alarm = (alarm_t *)list_node(node);
    node = list_next(node);
    // TODO: Each module is responsible for tearing down its alarms; currently,
    // this is not the case. In the future, this check should be replaced by
    // an assert.
    if (alarm->queue == queue)
      alarm_cancel_internal(alarm);
  }
  pthread_mutex_unlock(&monitor);
}

static void alarm_queue_ready(fixed_queue_t *queue,
                              UNUSED_ATTR void *context) {
  assert(queue != NULL);

  pthread_mutex_lock(&monitor);
  alarm_t *alarm = (alarm_t *)fixed_queue_try_dequeue(queue);
  if (alarm == NULL) {
    pthread_mutex_unlock(&monitor);
    return;             // The alarm was probably canceled
  }

  //
  // If the alarm is not periodic, we've fully serviced it now, and can reset
  // some of its internal state. This is useful to distinguish between expired
  // alarms and active ones.
  //
  alarm_callback_t callback = alarm->callback;
  void *data = alarm->data;
  period_ms_t deadline = alarm->deadline;
  if (alarm->is_periodic) {
    // The periodic alarm has been rescheduled and alarm->deadline has been
    // updated, hence we need to use the previous deadline.
    deadline = alarm->prev_deadline;
  } else {
    alarm->deadline = 0;
    alarm->callback = NULL;
    alarm->data = NULL;
    alarm->queue = NULL;
  }

  pthread_mutex_lock(&alarm->callback_lock);
  pthread_mutex_unlock(&monitor);

  period_ms_t t0 = now();
  callback(data);
  period_ms_t t1 = now();

  // Update the statistics
  assert(t1 >= t0);
  period_ms_t delta = t1 - t0;
  update_scheduling_stats(&alarm->stats, t0, deadline, delta);

  pthread_mutex_unlock(&alarm->callback_lock);
}

// Callback function for wake alarms and our posix timer
static void timer_callback(UNUSED_ATTR void *ptr) {
    semaphore_post(alarm_expired);
}

// Function running on |dispatcher_thread| that performs the following:
//   (1) Receives a signal using |alarm_exired| that the alarm has expired
//   (2) Dispatches the alarm callback for processing by the corresponding
// thread for that alarm.
static void callback_dispatch(UNUSED_ATTR void *context) {
  while (true) {
    semaphore_wait(alarm_expired);
    if (!dispatcher_thread_active)
      break;

    pthread_mutex_lock(&monitor);
    alarm_t *alarm;

    // Take into account that the alarm may get cancelled before we get to it.
    // We're done here if there are no alarms or the alarm at the front is in
    // the future. Release the monitor lock and exit right away since there's
    // nothing left to do.
    if (list_is_empty(alarms) ||
        (alarm = list_front(alarms))->deadline > now()) {
      reschedule_root_alarm();
      pthread_mutex_unlock(&monitor);
      continue;
    }

    list_remove(alarms, alarm);

    if (alarm->is_periodic) {
      alarm->prev_deadline = alarm->deadline;
      schedule_next_instance(alarm);
      alarm->stats.rescheduled_count++;
    }
    reschedule_root_alarm();

    // Enqueue the alarm for processing
    fixed_queue_enqueue(alarm->queue, alarm);

    pthread_mutex_unlock(&monitor);
  }

  LOG_DEBUG(LOG_TAG, "%s Callback thread exited", __func__);
}

static bool timer_create_internal(const clockid_t clock_id, timer_t *timer) {
  assert(timer != NULL);

  struct sigevent sigevent;
  memset(&sigevent, 0, sizeof(sigevent));
  sigevent.sigev_notify = SIGEV_THREAD;
  sigevent.sigev_notify_function = (void (*)(union sigval))timer_callback;
  if (timer_create(clock_id, &sigevent, timer) == -1) {
    LOG_ERROR(LOG_TAG, "%s unable to create timer with clock %d: %s",
              __func__, clock_id, strerror(errno));
    if (clock_id == CLOCK_BOOTTIME_ALARM) {
      LOG_ERROR(LOG_TAG, "The kernel might not have support for timer_create(CLOCK_BOOTTIME_ALARM): https://lwn.net/Articles/429925/");
      LOG_ERROR(LOG_TAG, "See following patches: https://git.kernel.org/cgit/linux/kernel/git/torvalds/linux.git/log/?qt=grep&q=CLOCK_BOOTTIME_ALARM");
    }
    return false;
  }

  return true;
}

static void update_scheduling_stats(alarm_stats_t *stats,
                                    period_ms_t now_ms,
                                    period_ms_t deadline_ms,
                                    period_ms_t execution_delta_ms)
{
  stats->total_updates++;
  stats->last_update_ms = now_ms;

  update_stat(&stats->callback_execution, execution_delta_ms);

  if (deadline_ms < now_ms) {
    // Overdue scheduling
    period_ms_t delta_ms = now_ms - deadline_ms;
    update_stat(&stats->overdue_scheduling, delta_ms);
  } else if (deadline_ms > now_ms) {
    // Premature scheduling
    period_ms_t delta_ms = deadline_ms - now_ms;
    update_stat(&stats->premature_scheduling, delta_ms);
  }
}

static void dump_stat(int fd, stat_t *stat, const char *description)
{
    period_ms_t average_time_ms = 0;
    if (stat->count != 0)
      average_time_ms = stat->total_ms / stat->count;

    dprintf(fd, "%-51s: %llu / %llu / %llu\n",
            description,
            (unsigned long long)stat->total_ms,
            (unsigned long long)stat->max_ms,
            (unsigned long long)average_time_ms);
}

void alarm_debug_dump(int fd)
{
  dprintf(fd, "\nBluetooth Alarms Statistics:\n");

  pthread_mutex_lock(&monitor);

  if (alarms == NULL) {
    pthread_mutex_unlock(&monitor);
    dprintf(fd, "  None\n");
    return;
  }

  period_ms_t just_now = now();

  dprintf(fd, "  Total Alarms: %zu\n\n", list_length(alarms));

  // Dump info for each alarm
  for (list_node_t *node = list_begin(alarms); node != list_end(alarms);
       node = list_next(node)) {
    alarm_t *alarm = (alarm_t *)list_node(node);
    alarm_stats_t *stats = &alarm->stats;

    dprintf(fd, "  Alarm : %s (%s)\n", stats->name,
            (alarm->is_periodic) ? "PERIODIC" : "SINGLE");

    dprintf(fd, "%-51s: %zu / %zu / %zu / %zu\n",
            "    Action counts (sched/resched/exec/cancel)",
            stats->scheduled_count, stats->rescheduled_count,
            stats->callback_execution.count, stats->canceled_count);

    dprintf(fd, "%-51s: %zu / %zu\n",
            "    Deviation counts (overdue/premature)",
            stats->overdue_scheduling.count,
            stats->premature_scheduling.count);

    dprintf(fd, "%-51s: %llu / %llu / %lld\n",
            "    Time in ms (since creation/interval/remaining)",
            (unsigned long long)(just_now - alarm->creation_time),
            (unsigned long long) alarm->period,
            (long long)(alarm->deadline - just_now));

    dump_stat(fd, &stats->callback_execution,
              "    Callback execution time in ms (total/max/avg)");

    dump_stat(fd, &stats->overdue_scheduling,
              "    Overdue scheduling time in ms (total/max/avg)");

    dump_stat(fd, &stats->premature_scheduling,
              "    Premature scheduling time in ms (total/max/avg)");

    dprintf(fd, "\n");
  }
  pthread_mutex_unlock(&monitor);
}
