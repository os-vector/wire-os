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

#include <gtest/gtest.h>

#include "AlarmTestHarness.h"

extern "C" {
#include "osi/include/alarm.h"
#include "osi/include/fixed_queue.h"
#include "osi/include/osi.h"
#include "osi/include/semaphore.h"
#include "osi/include/thread.h"
}

static semaphore_t *semaphore;
static int cb_counter;
static int cb_misordered_counter;

static const uint64_t EPSILON_MS = 5;

static void msleep(uint64_t ms) {
  usleep(ms * 1000);
}

class AlarmTest : public AlarmTestHarness {
  protected:
    virtual void SetUp() {
      AlarmTestHarness::SetUp();
      cb_counter = 0;
      cb_misordered_counter = 0;

      semaphore = semaphore_new(0);
    }

    virtual void TearDown() {
      semaphore_free(semaphore);
      AlarmTestHarness::TearDown();
    }
};

static void cb(UNUSED_ATTR void *data) {
  ++cb_counter;
  semaphore_post(semaphore);
}

static void ordered_cb(void *data) {
  int i = PTR_TO_INT(data);
  if (i != cb_counter)
    cb_misordered_counter++;
  ++cb_counter;
  semaphore_post(semaphore);
}

TEST_F(AlarmTest, test_new_free_simple) {
  alarm_t *alarm = alarm_new("alarm_test.test_new_free_simple");
  ASSERT_TRUE(alarm != NULL);
  alarm_free(alarm);
}

TEST_F(AlarmTest, test_free_null) {
  alarm_free(NULL);
}

TEST_F(AlarmTest, test_simple_cancel) {
  alarm_t *alarm = alarm_new("alarm_test.test_simple_cancel");
  alarm_cancel(alarm);
  alarm_free(alarm);
}

TEST_F(AlarmTest, test_cancel) {
  alarm_t *alarm = alarm_new("alarm_test.test_cancel");
  alarm_set(alarm, 10, cb, NULL);
  alarm_cancel(alarm);

  msleep(10 + EPSILON_MS);

  EXPECT_EQ(cb_counter, 0);
  EXPECT_FALSE(WakeLockHeld());
  alarm_free(alarm);
}

TEST_F(AlarmTest, test_cancel_idempotent) {
  alarm_t *alarm = alarm_new("alarm_test.test_cancel_idempotent");
  alarm_set(alarm, 10, cb, NULL);
  alarm_cancel(alarm);
  alarm_cancel(alarm);
  alarm_cancel(alarm);
  alarm_free(alarm);
}

TEST_F(AlarmTest, test_set_short) {
  alarm_t *alarm = alarm_new("alarm_test.test_set_short");

  alarm_set(alarm, 10, cb, NULL);

  EXPECT_EQ(cb_counter, 0);
  EXPECT_TRUE(WakeLockHeld());

  semaphore_wait(semaphore);

  EXPECT_EQ(cb_counter, 1);
  EXPECT_FALSE(WakeLockHeld());

  alarm_free(alarm);
}

TEST_F(AlarmTest, test_set_short_periodic) {
  alarm_t *alarm = alarm_new_periodic("alarm_test.test_set_short_periodic");

  alarm_set(alarm, 10, cb, NULL);

  EXPECT_EQ(cb_counter, 0);
  EXPECT_TRUE(WakeLockHeld());

  for (int i = 1; i <= 10; i++) {
    semaphore_wait(semaphore);

    EXPECT_GE(cb_counter, i);
    EXPECT_TRUE(WakeLockHeld());
  }
  alarm_cancel(alarm);
  EXPECT_FALSE(WakeLockHeld());

  alarm_free(alarm);
}

TEST_F(AlarmTest, test_set_zero_periodic) {
  alarm_t *alarm = alarm_new_periodic("alarm_test.test_set_zero_periodic");

  alarm_set(alarm, 0, cb, NULL);

  EXPECT_TRUE(WakeLockHeld());

  for (int i = 1; i <= 10; i++) {
    semaphore_wait(semaphore);

    EXPECT_GE(cb_counter, i);
    EXPECT_TRUE(WakeLockHeld());
  }
  alarm_cancel(alarm);
  EXPECT_FALSE(WakeLockHeld());

  alarm_free(alarm);
}

TEST_F(AlarmTest, test_set_long) {
  alarm_t *alarm = alarm_new("alarm_test.test_set_long");
  alarm_set(alarm, TIMER_INTERVAL_FOR_WAKELOCK_IN_MS + EPSILON_MS, cb, NULL);

  EXPECT_EQ(cb_counter, 0);
  EXPECT_FALSE(WakeLockHeld());

  semaphore_wait(semaphore);

  EXPECT_EQ(cb_counter, 1);
  EXPECT_FALSE(WakeLockHeld());

  alarm_free(alarm);
}

TEST_F(AlarmTest, test_set_short_short) {
  alarm_t *alarm[2] = {
    alarm_new("alarm_test.test_set_short_short_0"),
    alarm_new("alarm_test.test_set_short_short_1")
  };

  alarm_set(alarm[0], 10, cb, NULL);
  alarm_set(alarm[1], 20, cb, NULL);

  EXPECT_EQ(cb_counter, 0);
  EXPECT_TRUE(WakeLockHeld());

  semaphore_wait(semaphore);

  EXPECT_EQ(cb_counter, 1);
  EXPECT_TRUE(WakeLockHeld());

  semaphore_wait(semaphore);

  EXPECT_EQ(cb_counter, 2);
  EXPECT_FALSE(WakeLockHeld());

  alarm_free(alarm[0]);
  alarm_free(alarm[1]);
}

TEST_F(AlarmTest, test_set_short_long) {
  alarm_t *alarm[2] = {
    alarm_new("alarm_test.test_set_short_long_0"),
    alarm_new("alarm_test.test_set_short_long_1")
  };

  alarm_set(alarm[0], 10, cb, NULL);
  alarm_set(alarm[1], 10 + TIMER_INTERVAL_FOR_WAKELOCK_IN_MS + EPSILON_MS, cb, NULL);

  EXPECT_EQ(cb_counter, 0);
  EXPECT_TRUE(WakeLockHeld());

  semaphore_wait(semaphore);

  EXPECT_EQ(cb_counter, 1);
  EXPECT_FALSE(WakeLockHeld());

  semaphore_wait(semaphore);

  EXPECT_EQ(cb_counter, 2);
  EXPECT_FALSE(WakeLockHeld());

  alarm_free(alarm[0]);
  alarm_free(alarm[1]);
}

TEST_F(AlarmTest, test_set_long_long) {
  alarm_t *alarm[2] = {
    alarm_new("alarm_test.test_set_long_long_0"),
    alarm_new("alarm_test.test_set_long_long_1")
  };

  alarm_set(alarm[0], TIMER_INTERVAL_FOR_WAKELOCK_IN_MS + EPSILON_MS, cb, NULL);
  alarm_set(alarm[1], 2 * (TIMER_INTERVAL_FOR_WAKELOCK_IN_MS + EPSILON_MS), cb, NULL);

  EXPECT_EQ(cb_counter, 0);
  EXPECT_FALSE(WakeLockHeld());

  semaphore_wait(semaphore);

  EXPECT_EQ(cb_counter, 1);
  EXPECT_FALSE(WakeLockHeld());

  semaphore_wait(semaphore);

  EXPECT_EQ(cb_counter, 2);
  EXPECT_FALSE(WakeLockHeld());

  alarm_free(alarm[0]);
  alarm_free(alarm[1]);
}

TEST_F(AlarmTest, test_is_scheduled) {
  alarm_t *alarm = alarm_new("alarm_test.test_is_scheduled");

  EXPECT_FALSE(alarm_is_scheduled((alarm_t *)NULL));
  EXPECT_FALSE(alarm_is_scheduled(alarm));
  alarm_set(alarm, TIMER_INTERVAL_FOR_WAKELOCK_IN_MS + EPSILON_MS, cb, NULL);
  EXPECT_TRUE(alarm_is_scheduled(alarm));

  EXPECT_EQ(cb_counter, 0);
  EXPECT_FALSE(WakeLockHeld());

  semaphore_wait(semaphore);

  EXPECT_FALSE(alarm_is_scheduled(alarm));
  EXPECT_EQ(cb_counter, 1);
  EXPECT_FALSE(WakeLockHeld());

  alarm_free(alarm);
}

// Test whether the callbacks are invoked in the expected order
TEST_F(AlarmTest, test_callback_ordering) {
  alarm_t *alarms[100];

  for (int i = 0; i < 100; i++) {
    const std::string alarm_name = "alarm_test.test_callback_ordering[" +
      std::to_string(i) + "]";
    alarms[i] = alarm_new(alarm_name.c_str());
  }

  for (int i = 0; i < 100; i++) {
    alarm_set(alarms[i], 100, ordered_cb, INT_TO_PTR(i));
  }

  for (int i = 1; i <= 100; i++) {
    semaphore_wait(semaphore);
    EXPECT_GE(cb_counter, i);
  }
  EXPECT_EQ(cb_counter, 100);
  EXPECT_EQ(cb_misordered_counter, 0);

  for (int i = 0; i < 100; i++)
    alarm_free(alarms[i]);

  EXPECT_FALSE(WakeLockHeld());
}

// Test whether the callbacks are involed in the expected order on a
// separate queue.
TEST_F(AlarmTest, test_callback_ordering_on_queue) {
  alarm_t *alarms[100];
  fixed_queue_t *queue = fixed_queue_new(SIZE_MAX);
  thread_t *thread = thread_new("timers.test_callback_ordering_on_queue.thread");

  alarm_register_processing_queue(queue, thread);

  for (int i = 0; i < 100; i++) {
    const std::string alarm_name =
      "alarm_test.test_callback_ordering_on_queue[" +
      std::to_string(i) + "]";
    alarms[i] = alarm_new(alarm_name.c_str());
  }

  for (int i = 0; i < 100; i++) {
    alarm_set_on_queue(alarms[i], 100, ordered_cb, INT_TO_PTR(i), queue);
  }

  for (int i = 1; i <= 100; i++) {
    semaphore_wait(semaphore);
    EXPECT_GE(cb_counter, i);
  }
  EXPECT_EQ(cb_counter, 100);
  EXPECT_EQ(cb_misordered_counter, 0);

  for (int i = 0; i < 100; i++)
    alarm_free(alarms[i]);

  EXPECT_FALSE(WakeLockHeld());

  alarm_unregister_processing_queue(queue);
  fixed_queue_free(queue, NULL);
  thread_free(thread);
}

// Test whether unregistering a processing queue cancels all timers using
// that queue.
TEST_F(AlarmTest, test_unregister_processing_queue) {
  alarm_t *alarms[100];
  fixed_queue_t *queue = fixed_queue_new(SIZE_MAX);
  thread_t *thread =
    thread_new("timers.test_unregister_processing_queue.thread");

  alarm_register_processing_queue(queue, thread);

  for (int i = 0; i < 100; i++) {
    const std::string alarm_name =
      "alarm_test.test_unregister_processing_queue[" +
      std::to_string(i) + "]";
    alarms[i] = alarm_new(alarm_name.c_str());
  }

  // Schedule half of the timers to expire soon, and the rest far in the future
  for (int i = 0; i < 50; i++) {
    alarm_set_on_queue(alarms[i], 100, ordered_cb, INT_TO_PTR(i), queue);
  }
  for (int i = 50; i < 100; i++) {
    alarm_set_on_queue(alarms[i], 1000 * 1000, ordered_cb, INT_TO_PTR(i), queue);
  }

  // Wait until half of the timers have expired
  for (int i = 1; i <= 50; i++) {
    semaphore_wait(semaphore);
    EXPECT_GE(cb_counter, i);
  }
  EXPECT_EQ(cb_counter, 50);
  EXPECT_EQ(cb_misordered_counter, 0);

  // Test that only the expired timers are not scheduled
  for (int i = 0; i < 50; i++) {
    EXPECT_FALSE(alarm_is_scheduled(alarms[i]));
  }
  for (int i = 50; i < 100; i++) {
    EXPECT_TRUE(alarm_is_scheduled(alarms[i]));
  }

  alarm_unregister_processing_queue(queue);

  // Test that none of the timers are scheduled
  for (int i = 0; i < 100; i++) {
    EXPECT_FALSE(alarm_is_scheduled(alarms[i]));
  }

  for (int i = 0; i < 100; i++) {
    alarm_free(alarms[i]);
  }

  EXPECT_FALSE(WakeLockHeld());

  fixed_queue_free(queue, NULL);
  thread_free(thread);
}

// Test whether unregistering a processing queue cancels all periodic timers
// using that queue.
TEST_F(AlarmTest, test_periodic_unregister_processing_queue) {
  alarm_t *alarms[5];
  fixed_queue_t *queue = fixed_queue_new(SIZE_MAX);
  thread_t *thread =
    thread_new("timers.test_periodic_unregister_processing_queue.thread");

  alarm_register_processing_queue(queue, thread);

  for (int i = 0; i < 5; i++) {
    const std::string alarm_name =
      "alarm_test.test_periodic_unregister_processing_queue[" +
      std::to_string(i) + "]";
    alarms[i] = alarm_new_periodic(alarm_name.c_str());
  }

  // Schedule each of the timers with different period
  for (int i = 0; i < 5; i++) {
    alarm_set_on_queue(alarms[i], 20 + i, cb, INT_TO_PTR(i), queue);
  }
  EXPECT_TRUE(WakeLockHeld());

  for (int i = 1; i <= 20; i++) {
    semaphore_wait(semaphore);

    EXPECT_GE(cb_counter, i);
    EXPECT_TRUE(WakeLockHeld());
  }

  // Test that all timers are still scheduled
  for (int i = 0; i < 5; i++) {
    EXPECT_TRUE(alarm_is_scheduled(alarms[i]));
  }

  alarm_unregister_processing_queue(queue);

  int saved_cb_counter = cb_counter;

  // Test that none of the timers are scheduled
  for (int i = 0; i < 5; i++) {
    EXPECT_FALSE(alarm_is_scheduled(alarms[i]));
  }

  // Sleep for 500ms and test again that the cb_counter hasn't been modified
  usleep(500 * 1000);
  EXPECT_TRUE(cb_counter == saved_cb_counter);

  for (int i = 0; i < 5; i++) {
    alarm_free(alarms[i]);
  }

  EXPECT_FALSE(WakeLockHeld());

  fixed_queue_free(queue, NULL);
  thread_free(thread);
}

// Try to catch any race conditions between the timer callback and |alarm_free|.
TEST_F(AlarmTest, test_callback_free_race) {
  for (int i = 0; i < 1000; ++i) {
    const std::string alarm_name = "alarm_test.test_callback_free_race[" +
      std::to_string(i) + "]";
    alarm_t *alarm = alarm_new(alarm_name.c_str());
    alarm_set(alarm, 0, cb, NULL);
    alarm_free(alarm);
  }
  alarm_cleanup();
}
