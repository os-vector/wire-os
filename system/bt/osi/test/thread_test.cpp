#include <gtest/gtest.h>

#include "AllocationTestHarness.h"

extern "C" {
#include <sys/select.h>

#include "osi/include/reactor.h"
#include "osi/include/thread.h"
#include "osi/include/osi.h"
}

class ThreadTest : public AllocationTestHarness {};

TEST_F(ThreadTest, test_new_simple) {
  thread_t *thread = thread_new("test_thread");
  ASSERT_TRUE(thread != NULL);
  thread_free(thread);
}

TEST_F(ThreadTest, test_free_simple) {
  thread_t *thread = thread_new("test_thread");
  thread_free(thread);
}

TEST_F(ThreadTest, test_name) {
  thread_t *thread = thread_new("test_name");
  ASSERT_STREQ(thread_name(thread), "test_name");
  thread_free(thread);
}

TEST_F(ThreadTest, test_long_name) {
  thread_t *thread = thread_new("0123456789abcdef");
  ASSERT_STREQ("0123456789abcdef", thread_name(thread));
  thread_free(thread);
}

TEST_F(ThreadTest, test_very_long_name) {
  thread_t *thread = thread_new("0123456789abcdefg");
  ASSERT_STREQ("0123456789abcdef", thread_name(thread));
  thread_free(thread);
}

static void thread_is_self_fn(void *context) {
  thread_t *thread = (thread_t *)context;
  EXPECT_TRUE(thread_is_self(thread));
}

TEST_F(ThreadTest, test_thread_is_self) {
  thread_t *thread = thread_new("test_thread");
  thread_post(thread, thread_is_self_fn, thread);
  thread_free(thread);
}

TEST_F(ThreadTest, test_thread_is_not_self) {
  thread_t *thread = thread_new("test_thread");
  EXPECT_FALSE(thread_is_self(thread));
  thread_free(thread);
}
