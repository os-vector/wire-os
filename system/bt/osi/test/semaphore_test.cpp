#include <gtest/gtest.h>

#include "AllocationTestHarness.h"

extern "C" {
#include <unistd.h>
#include <sys/select.h>

#include "osi/include/osi.h"
#include "osi/include/reactor.h"
#include "osi/include/semaphore.h"
#include "osi/include/thread.h"
}

struct SemaphoreTestSequenceHelper {
  semaphore_t *semaphore;
  int counter;
};

namespace {
  void sleep_then_increment_counter(void *context) {
    SemaphoreTestSequenceHelper *helper =
            reinterpret_cast<SemaphoreTestSequenceHelper*>(context);
    assert(helper);
    assert(helper->semaphore);
    sleep(1);
    ++helper->counter;
    semaphore_post(helper->semaphore);
  }
} // namespace

class SemaphoreTest : public AllocationTestHarness {};

TEST_F(SemaphoreTest, test_new_simple) {
  semaphore_t *semaphore = semaphore_new(0);
  ASSERT_TRUE(semaphore != NULL);
  semaphore_free(semaphore);
}

TEST_F(SemaphoreTest, test_new_with_value) {
  semaphore_t *semaphore = semaphore_new(3);
  ASSERT_TRUE(semaphore != NULL);

  EXPECT_TRUE(semaphore_try_wait(semaphore));
  EXPECT_TRUE(semaphore_try_wait(semaphore));
  EXPECT_TRUE(semaphore_try_wait(semaphore));
  EXPECT_FALSE(semaphore_try_wait(semaphore));

  semaphore_free(semaphore);
}

TEST_F(SemaphoreTest, test_try_wait) {
  semaphore_t *semaphore = semaphore_new(0);
  ASSERT_TRUE(semaphore != NULL);

  EXPECT_FALSE(semaphore_try_wait(semaphore));
  semaphore_post(semaphore);
  EXPECT_TRUE(semaphore_try_wait(semaphore));
  EXPECT_FALSE(semaphore_try_wait(semaphore));

  semaphore_free(semaphore);
}

TEST_F(SemaphoreTest, test_wait_after_post) {
  semaphore_t *semaphore = semaphore_new(0);
  ASSERT_TRUE(semaphore != NULL);
  semaphore_post(semaphore);
  semaphore_wait(semaphore);
  semaphore_free(semaphore);
}

TEST_F(SemaphoreTest, test_ensure_wait) {
  semaphore_t *semaphore = semaphore_new(0);
  ASSERT_TRUE(semaphore != NULL);
  thread_t *thread = thread_new("semaphore_test_thread");
  ASSERT_TRUE(thread != NULL);

  EXPECT_FALSE(semaphore_try_wait(semaphore));
  SemaphoreTestSequenceHelper sequence_helper = {semaphore, 0};
  thread_post(thread, sleep_then_increment_counter, &sequence_helper);
  semaphore_wait(semaphore);
  EXPECT_EQ(sequence_helper.counter, 1) <<
      "semaphore_wait() did not wait for counter to increment";

  semaphore_free(semaphore);
  thread_free(thread);
}
