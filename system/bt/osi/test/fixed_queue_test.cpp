#include <gtest/gtest.h>

#include <climits>

#include "AllocationTestHarness.h"

extern "C" {
#include "osi/include/allocator.h"
#include "osi/include/fixed_queue.h"
#include "osi/include/future.h"
#include "osi/include/osi.h"
#include "osi/include/thread.h"
}

static const size_t TEST_QUEUE_SIZE = 10;
static const char *DUMMY_DATA_STRING = "Dummy data string";
static const char *DUMMY_DATA_STRING1 = "Dummy data string1";
static const char *DUMMY_DATA_STRING2 = "Dummy data string2";
static const char *DUMMY_DATA_STRING3 = "Dummy data string3";
static future_t *received_message_future = NULL;

// Test whether a file descriptor |fd| is readable.
// Return true if the file descriptor is readable, otherwise false.
static bool is_fd_readable(int fd)
{
  fd_set rfds;
  struct timeval tv;

  FD_ZERO(&rfds);
  tv.tv_sec = 0;
  tv.tv_usec = 0;
  FD_SET(fd, &rfds);
  // Only the enqueue_fd should be readable
  int result = select(FD_SETSIZE, &rfds, NULL, NULL, &tv);
  EXPECT_TRUE(result >= 0);

  return FD_ISSET(fd, &rfds);
}

// Function for performing dequeue operations from the queue when is ready
static void
fixed_queue_ready(fixed_queue_t *queue, UNUSED_ATTR void *context)
{
  void *msg = fixed_queue_try_dequeue(queue);
  EXPECT_TRUE(msg != NULL);
  future_ready(received_message_future, msg);
}

class FixedQueueTest : public AllocationTestHarness {};

TEST_F(FixedQueueTest, test_fixed_queue_new_free) {
  fixed_queue_t *queue;

  // Test a corner case: queue of size 0
  queue = fixed_queue_new(0);
  EXPECT_TRUE(queue != NULL);
  fixed_queue_free(queue, NULL);

  // Test a corner case: queue of size 1
  queue = fixed_queue_new(1);
  EXPECT_TRUE(queue != NULL);
  fixed_queue_free(queue, NULL);

  // Test a corner case: queue of maximum size
  queue = fixed_queue_new((size_t)-1);
  EXPECT_TRUE(queue != NULL);
  fixed_queue_free(queue, NULL);

  // Test a queue of some size
  queue = fixed_queue_new(TEST_QUEUE_SIZE);
  EXPECT_TRUE(queue != NULL);
  fixed_queue_free(queue, NULL);

  // Test free-ing a NULL queue
  fixed_queue_free(NULL, NULL);
  fixed_queue_free(NULL, osi_free);
}

TEST_F(FixedQueueTest, test_fixed_queue_is_empty) {
  fixed_queue_t *queue;

  // Test a NULL queue
  EXPECT_TRUE(fixed_queue_is_empty(NULL));

  // Test an empty queue
  queue = fixed_queue_new(TEST_QUEUE_SIZE);
  ASSERT_TRUE(queue != NULL);
  EXPECT_TRUE(fixed_queue_is_empty(queue));

  // Test a non-empty queue
  fixed_queue_try_enqueue(queue, (void *)DUMMY_DATA_STRING);
  EXPECT_FALSE(fixed_queue_is_empty(queue));

  // Test an empty dequeued queue
  ASSERT_EQ(DUMMY_DATA_STRING, fixed_queue_try_dequeue(queue));
  EXPECT_TRUE(fixed_queue_is_empty(queue));

  fixed_queue_free(queue, NULL);
}

TEST_F(FixedQueueTest, test_fixed_queue_length) {
  fixed_queue_t *queue;

  // Test a NULL queue
  EXPECT_EQ((size_t)0, fixed_queue_length(NULL));

  // Test an empty queue
  queue = fixed_queue_new(TEST_QUEUE_SIZE);
  ASSERT_TRUE(queue != NULL);
  EXPECT_EQ((size_t)0, fixed_queue_length(queue));

  // Test a non-empty queue
  fixed_queue_try_enqueue(queue, (void *)DUMMY_DATA_STRING);
  EXPECT_EQ((size_t)1, fixed_queue_length(queue));

  // Test an empty dequeued queue
  ASSERT_EQ(DUMMY_DATA_STRING, fixed_queue_try_dequeue(queue));
  EXPECT_EQ((size_t)0, fixed_queue_length(queue));

  fixed_queue_free(queue, NULL);
}

TEST_F(FixedQueueTest, test_fixed_queue_capacity) {
  fixed_queue_t *queue;

  // Test a corner case: queue of size 0
  queue = fixed_queue_new(0);
  ASSERT_TRUE(queue != NULL);
  EXPECT_EQ((size_t)0, fixed_queue_capacity(queue));
  fixed_queue_free(queue, NULL);

  // Test a corner case: queue of size 1
  queue = fixed_queue_new(1);
  ASSERT_TRUE(queue != NULL);
  EXPECT_EQ((size_t)1, fixed_queue_capacity(queue));
  fixed_queue_free(queue, NULL);

  // Test a corner case: queue of maximum size
  queue = fixed_queue_new((size_t)-1);
  ASSERT_TRUE(queue != NULL);
  EXPECT_EQ((size_t)-1, fixed_queue_capacity(queue));
  fixed_queue_free(queue, NULL);

  // Test a queue of some size
  queue = fixed_queue_new(TEST_QUEUE_SIZE);
  ASSERT_TRUE(queue != NULL);
  EXPECT_EQ(TEST_QUEUE_SIZE, fixed_queue_capacity(queue));
  fixed_queue_free(queue, NULL);
}

TEST_F(FixedQueueTest, test_fixed_queue_enqueue_dequeue) {
  fixed_queue_t *queue = fixed_queue_new(TEST_QUEUE_SIZE);
  ASSERT_TRUE(queue != NULL);

  // Test blocking enqueue and blocking dequeue
  fixed_queue_enqueue(queue, (void *)DUMMY_DATA_STRING);
  EXPECT_EQ((size_t)1, fixed_queue_length(queue));
  EXPECT_EQ(DUMMY_DATA_STRING, fixed_queue_dequeue(queue));
  EXPECT_EQ((size_t)0, fixed_queue_length(queue));

  // Test non-blocking enqueue and non-blocking dequeue
  EXPECT_TRUE(fixed_queue_try_enqueue(queue, (void *)DUMMY_DATA_STRING));
  EXPECT_EQ((size_t)1, fixed_queue_length(queue));
  EXPECT_EQ(DUMMY_DATA_STRING, fixed_queue_try_dequeue(queue));
  EXPECT_EQ((size_t)0, fixed_queue_length(queue));

  // Test non-blocking enqueue beyond queue capacity
  for (size_t i = 0; i < TEST_QUEUE_SIZE; i++) {
    EXPECT_TRUE(fixed_queue_try_enqueue(queue, (void *)DUMMY_DATA_STRING));
  }
  // The next enqueue operation is beyond the queue capacity, so it should fail
  EXPECT_FALSE(fixed_queue_try_enqueue(queue, (void *)DUMMY_DATA_STRING));

  // Test non-blocking dequeue from a queue that is full to max capacity
  for (size_t i = 0; i < TEST_QUEUE_SIZE; i++) {
    EXPECT_EQ(DUMMY_DATA_STRING, fixed_queue_try_dequeue(queue));
  }

  // Test non-blocking dequeue from an empty queue
  EXPECT_EQ(NULL, fixed_queue_try_dequeue(queue));

  // Test non-blocking dequeue from a NULL queue
  EXPECT_EQ(NULL, fixed_queue_try_dequeue(NULL));

  fixed_queue_free(queue, NULL);
}

TEST_F(FixedQueueTest, test_fixed_queue_try_peek_first_last) {
  fixed_queue_t *queue = fixed_queue_new(TEST_QUEUE_SIZE);
  ASSERT_TRUE(queue != NULL);

  // Test peek first/last from a NULL queue
  EXPECT_EQ(NULL, fixed_queue_try_peek_first(NULL));
  EXPECT_EQ(NULL, fixed_queue_try_peek_last(NULL));

  // Test peek first/last from an empty queue
  EXPECT_EQ(NULL, fixed_queue_try_peek_first(queue));
  EXPECT_EQ(NULL, fixed_queue_try_peek_last(queue));

  // Test peek first/last from a queue with one element
  fixed_queue_enqueue(queue, (void *)DUMMY_DATA_STRING1);
  EXPECT_EQ(DUMMY_DATA_STRING1, fixed_queue_try_peek_first(queue));
  EXPECT_EQ(DUMMY_DATA_STRING1, fixed_queue_try_peek_last(queue));

  // Test peek first/last from a queue with two elements
  fixed_queue_enqueue(queue, (void *)DUMMY_DATA_STRING2);
  EXPECT_EQ(DUMMY_DATA_STRING1, fixed_queue_try_peek_first(queue));
  EXPECT_EQ(DUMMY_DATA_STRING2, fixed_queue_try_peek_last(queue));

  // Test peek first/last from a queue with three elements
  fixed_queue_enqueue(queue, (void *)DUMMY_DATA_STRING3);
  EXPECT_EQ(DUMMY_DATA_STRING1, fixed_queue_try_peek_first(queue));
  EXPECT_EQ(DUMMY_DATA_STRING3, fixed_queue_try_peek_last(queue));

  fixed_queue_free(queue, NULL);
}

TEST_F(FixedQueueTest, test_fixed_queue_try_remove_from_queue) {
  fixed_queue_t *queue = fixed_queue_new(TEST_QUEUE_SIZE);
  ASSERT_TRUE(queue != NULL);

  // Test removing from a NULL queue
  EXPECT_EQ(NULL, fixed_queue_try_remove_from_queue(NULL,
                                        (void *)DUMMY_DATA_STRING));

  // Test removing from an empty queue
  EXPECT_EQ(NULL, fixed_queue_try_remove_from_queue(queue,
                                        (void *)DUMMY_DATA_STRING));

  // Test removing a queued string from a queue
  fixed_queue_enqueue(queue, (void *)DUMMY_DATA_STRING1);
  fixed_queue_enqueue(queue, (void *)DUMMY_DATA_STRING2);
  fixed_queue_enqueue(queue, (void *)DUMMY_DATA_STRING3);
  EXPECT_EQ((size_t)3, fixed_queue_length(queue));
  EXPECT_EQ(DUMMY_DATA_STRING2, fixed_queue_try_remove_from_queue(queue,
                                        (void *)DUMMY_DATA_STRING2));
  EXPECT_EQ((size_t)2, fixed_queue_length(queue));
  // Removing again should fail
  EXPECT_EQ(NULL, fixed_queue_try_remove_from_queue(queue,
                                        (void *)DUMMY_DATA_STRING2));

  // Test removing a non-queued string from a queue
  EXPECT_EQ(NULL, fixed_queue_try_remove_from_queue(queue,
                                        (void *)DUMMY_DATA_STRING));

  fixed_queue_free(queue, NULL);
}

TEST_F(FixedQueueTest, test_fixed_queue_get_enqueue_dequeue_fd) {
  fixed_queue_t *queue = fixed_queue_new(TEST_QUEUE_SIZE);
  ASSERT_TRUE(queue != NULL);

  // Test validity of enqueue and dequeue file descriptors
  int enqueue_fd = fixed_queue_get_enqueue_fd(queue);
  int dequeue_fd = fixed_queue_get_dequeue_fd(queue);
  EXPECT_TRUE(enqueue_fd >= 0);
  EXPECT_TRUE(dequeue_fd >= 0);
  EXPECT_TRUE(enqueue_fd < FD_SETSIZE);
  EXPECT_TRUE(dequeue_fd < FD_SETSIZE);

  // Test the file descriptors of an empty queue
  // Only the enqueue_fd should be readable
  EXPECT_TRUE(is_fd_readable(enqueue_fd));
  EXPECT_FALSE(is_fd_readable(dequeue_fd));

  // Test the file descriptors of a non-empty queue
  // Both the enqueue_fd and dequeue_fd should be readable
  fixed_queue_enqueue(queue, (void *)DUMMY_DATA_STRING);
  EXPECT_TRUE(is_fd_readable(enqueue_fd));
  EXPECT_TRUE(is_fd_readable(dequeue_fd));
  fixed_queue_dequeue(queue);

  // Test the file descriptors of a full queue
  // Only the dequeue_fd should be readable
  for (size_t i = 0; i < TEST_QUEUE_SIZE; i++) {
    EXPECT_TRUE(fixed_queue_try_enqueue(queue, (void *)DUMMY_DATA_STRING));
  }
  EXPECT_FALSE(is_fd_readable(enqueue_fd));
  EXPECT_TRUE(is_fd_readable(dequeue_fd));

  fixed_queue_free(queue, NULL);
}

TEST_F(FixedQueueTest, test_fixed_queue_register_dequeue) {
  fixed_queue_t *queue = fixed_queue_new(TEST_QUEUE_SIZE);
  ASSERT_TRUE(queue != NULL);

  received_message_future = future_new();
  ASSERT_TRUE(received_message_future != NULL);

  thread_t *worker_thread = thread_new("test_fixed_queue_worker_thread");
  ASSERT_TRUE(worker_thread != NULL);

  fixed_queue_register_dequeue(queue,
                               thread_get_reactor(worker_thread),
                               fixed_queue_ready,
                               NULL);

  // Add a message to the queue, and expect to receive it
  fixed_queue_enqueue(queue, (void *)DUMMY_DATA_STRING);
  const char *msg = (const char *)future_await(received_message_future);
  EXPECT_EQ(DUMMY_DATA_STRING, msg);

  fixed_queue_unregister_dequeue(queue);
  thread_free(worker_thread);
  fixed_queue_free(queue, NULL);
}
