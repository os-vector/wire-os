#include <gtest/gtest.h>

#include <climits>

#include "AllocationTestHarness.h"

extern "C" {
#include "osi/include/data_dispatcher.h"
#include "osi/include/fixed_queue.h"
#include "osi/include/osi.h"
}

#define DUMMY_TYPE_0 34
#define DUMMY_TYPE_1 42
#define TYPE_EDGE_CASE_ZERO 0
#define TYPE_EDGE_CASE_MAX INT_MAX

#define DUMMY_QUEUE_SIZE 10

class DataDispatcherTest : public AllocationTestHarness {};

static char dummy_data_0[42] = "please test your code";
static char dummy_data_1[42] = "testing is good for your sanity";

TEST_F(DataDispatcherTest, test_new_free_simple) {
  data_dispatcher_t *dispatcher = data_dispatcher_new("test_dispatcher");
  ASSERT_TRUE(dispatcher != NULL);
  data_dispatcher_free(dispatcher);
}

TEST_F(DataDispatcherTest, test_dispatch_single_to_nowhere) {
  data_dispatcher_t *dispatcher = data_dispatcher_new("test_dispatcher");
  EXPECT_FALSE(data_dispatcher_dispatch(dispatcher, DUMMY_TYPE_0, dummy_data_0));
  data_dispatcher_free(dispatcher);
}

TEST_F(DataDispatcherTest, test_dispatch_single_to_single) {
  data_dispatcher_t *dispatcher = data_dispatcher_new("test_dispatcher");

  // Register a queue
  fixed_queue_t *dummy_queue = fixed_queue_new(DUMMY_QUEUE_SIZE);
  data_dispatcher_register(dispatcher, DUMMY_TYPE_0, dummy_queue);
  EXPECT_TRUE(fixed_queue_is_empty(dummy_queue));

  // Send data to the queue
  EXPECT_TRUE(data_dispatcher_dispatch(dispatcher, DUMMY_TYPE_0, dummy_data_0));

  // Did we get it?
  EXPECT_FALSE(fixed_queue_is_empty(dummy_queue));
  EXPECT_STREQ(dummy_data_0, (char *)fixed_queue_try_dequeue(dummy_queue));
  EXPECT_TRUE(fixed_queue_is_empty(dummy_queue));

  fixed_queue_free(dummy_queue, NULL);
  data_dispatcher_free(dispatcher);
}

TEST_F(DataDispatcherTest, test_dispatch_single_to_multiple) {
  data_dispatcher_t *dispatcher = data_dispatcher_new("test_dispatcher");

  // Register two queues
  fixed_queue_t *dummy_queue0 = fixed_queue_new(DUMMY_QUEUE_SIZE);
  fixed_queue_t *dummy_queue1 = fixed_queue_new(DUMMY_QUEUE_SIZE);
  data_dispatcher_register(dispatcher, DUMMY_TYPE_0, dummy_queue0);
  data_dispatcher_register(dispatcher, DUMMY_TYPE_1, dummy_queue1);
  EXPECT_TRUE(fixed_queue_is_empty(dummy_queue0));
  EXPECT_TRUE(fixed_queue_is_empty(dummy_queue1));

  // Send data to one of them
  EXPECT_TRUE(data_dispatcher_dispatch(dispatcher, DUMMY_TYPE_0, dummy_data_0));

  // Did we get it?
  EXPECT_FALSE(fixed_queue_is_empty(dummy_queue0));
  EXPECT_TRUE(fixed_queue_is_empty(dummy_queue1));
  EXPECT_STREQ(dummy_data_0, (char *)fixed_queue_try_dequeue(dummy_queue0));
  EXPECT_TRUE(fixed_queue_is_empty(dummy_queue0));

  fixed_queue_free(dummy_queue0, NULL);
  fixed_queue_free(dummy_queue1, NULL);
  data_dispatcher_free(dispatcher);
}

TEST_F(DataDispatcherTest, test_dispatch_single_to_default) {
  data_dispatcher_t *dispatcher = data_dispatcher_new("test_dispatcher");

  // Register two queues, a default and a typed one
  fixed_queue_t *dummy_queue = fixed_queue_new(DUMMY_QUEUE_SIZE);
  fixed_queue_t *default_queue = fixed_queue_new(DUMMY_QUEUE_SIZE);
  data_dispatcher_register(dispatcher, DUMMY_TYPE_0, dummy_queue);
  data_dispatcher_register_default(dispatcher, default_queue);
  EXPECT_TRUE(fixed_queue_is_empty(dummy_queue));
  EXPECT_TRUE(fixed_queue_is_empty(default_queue));

  // Send data to nowhere
  EXPECT_TRUE(data_dispatcher_dispatch(dispatcher, DUMMY_TYPE_1, dummy_data_1));

  // Did we get it?
  EXPECT_TRUE(fixed_queue_is_empty(dummy_queue));
  EXPECT_FALSE(fixed_queue_is_empty(default_queue));
  EXPECT_STREQ(dummy_data_1, (char *)fixed_queue_try_dequeue(default_queue));
  EXPECT_TRUE(fixed_queue_is_empty(default_queue));

  fixed_queue_free(dummy_queue, NULL);
  fixed_queue_free(default_queue, NULL);
  data_dispatcher_free(dispatcher);
}

TEST_F(DataDispatcherTest, test_dispatch_multiple_to_single) {
  data_dispatcher_t *dispatcher = data_dispatcher_new("test_dispatcher");

  // Register a queue
  fixed_queue_t *dummy_queue = fixed_queue_new(DUMMY_QUEUE_SIZE);
  data_dispatcher_register(dispatcher, DUMMY_TYPE_0, dummy_queue);
  EXPECT_TRUE(fixed_queue_is_empty(dummy_queue));

  // Send data to the queue
  EXPECT_TRUE(data_dispatcher_dispatch(dispatcher, DUMMY_TYPE_0, dummy_data_0));
  EXPECT_TRUE(data_dispatcher_dispatch(dispatcher, DUMMY_TYPE_0, dummy_data_1));

  // Did we get it?
  EXPECT_FALSE(fixed_queue_is_empty(dummy_queue));
  EXPECT_STREQ(dummy_data_0, (char *)fixed_queue_try_dequeue(dummy_queue));
  EXPECT_FALSE(fixed_queue_is_empty(dummy_queue));
  EXPECT_STREQ(dummy_data_1, (char *)fixed_queue_try_dequeue(dummy_queue));
  EXPECT_TRUE(fixed_queue_is_empty(dummy_queue));

  fixed_queue_free(dummy_queue, NULL);
  data_dispatcher_free(dispatcher);
}

TEST_F(DataDispatcherTest, test_dispatch_multiple_to_multiple) {
  data_dispatcher_t *dispatcher = data_dispatcher_new("test_dispatcher");

  // Register two queues
  fixed_queue_t *dummy_queue0 = fixed_queue_new(DUMMY_QUEUE_SIZE);
  fixed_queue_t *dummy_queue1 = fixed_queue_new(DUMMY_QUEUE_SIZE);
  data_dispatcher_register(dispatcher, DUMMY_TYPE_0, dummy_queue0);
  data_dispatcher_register(dispatcher, DUMMY_TYPE_1, dummy_queue1);
  EXPECT_TRUE(fixed_queue_is_empty(dummy_queue0));
  EXPECT_TRUE(fixed_queue_is_empty(dummy_queue1));

  // Send data to both of them
  EXPECT_TRUE(data_dispatcher_dispatch(dispatcher, DUMMY_TYPE_0, dummy_data_0));
  EXPECT_TRUE(data_dispatcher_dispatch(dispatcher, DUMMY_TYPE_1, dummy_data_1));

  // Did we get it?
  EXPECT_FALSE(fixed_queue_is_empty(dummy_queue0));
  EXPECT_FALSE(fixed_queue_is_empty(dummy_queue1));
  EXPECT_STREQ(dummy_data_0, (char *)fixed_queue_try_dequeue(dummy_queue0));
  EXPECT_STREQ(dummy_data_1, (char *)fixed_queue_try_dequeue(dummy_queue1));
  EXPECT_TRUE(fixed_queue_is_empty(dummy_queue0));
  EXPECT_TRUE(fixed_queue_is_empty(dummy_queue1));

  fixed_queue_free(dummy_queue0, NULL);
  fixed_queue_free(dummy_queue1, NULL);
  data_dispatcher_free(dispatcher);
}

TEST_F(DataDispatcherTest, test_dispatch_single_to_single_reregistered) {
  data_dispatcher_t *dispatcher = data_dispatcher_new("test_dispatcher");

  // Register a queue, then reregister
  fixed_queue_t *dummy_queue = fixed_queue_new(DUMMY_QUEUE_SIZE);
  fixed_queue_t *dummy_queue_reregistered = fixed_queue_new(DUMMY_QUEUE_SIZE);
  data_dispatcher_register(dispatcher, DUMMY_TYPE_0, dummy_queue);
  data_dispatcher_register(dispatcher, DUMMY_TYPE_0, dummy_queue_reregistered);
  EXPECT_TRUE(fixed_queue_is_empty(dummy_queue));
  EXPECT_TRUE(fixed_queue_is_empty(dummy_queue_reregistered));

  // Send data to the queue
  EXPECT_TRUE(data_dispatcher_dispatch(dispatcher, DUMMY_TYPE_0, dummy_data_0));

  // Did we get it?
  EXPECT_TRUE(fixed_queue_is_empty(dummy_queue));
  EXPECT_FALSE(fixed_queue_is_empty(dummy_queue_reregistered));
  EXPECT_STREQ(dummy_data_0, (char *)fixed_queue_try_dequeue(dummy_queue_reregistered));
  EXPECT_TRUE(fixed_queue_is_empty(dummy_queue_reregistered));

  fixed_queue_free(dummy_queue, NULL);
  fixed_queue_free(dummy_queue_reregistered, NULL);
  data_dispatcher_free(dispatcher);
}

TEST_F(DataDispatcherTest, test_dispatch_single_to_reregistered_null) {
  data_dispatcher_t *dispatcher = data_dispatcher_new("test_dispatcher");

  // Register a queue
  fixed_queue_t *dummy_queue = fixed_queue_new(DUMMY_QUEUE_SIZE);
  data_dispatcher_register(dispatcher, DUMMY_TYPE_0, dummy_queue);
  data_dispatcher_register(dispatcher, DUMMY_TYPE_0, NULL);
  EXPECT_TRUE(fixed_queue_is_empty(dummy_queue));

  EXPECT_FALSE(data_dispatcher_dispatch(dispatcher, DUMMY_TYPE_0, dummy_data_0));
  EXPECT_TRUE(fixed_queue_is_empty(dummy_queue));

  fixed_queue_free(dummy_queue, NULL);
  data_dispatcher_free(dispatcher);
}

TEST_F(DataDispatcherTest, test_dispatch_single_to_default_reregistered_null) {
  data_dispatcher_t *dispatcher = data_dispatcher_new("test_dispatcher");

  // Register a queue
  fixed_queue_t *dummy_queue = fixed_queue_new(DUMMY_QUEUE_SIZE);
  data_dispatcher_register_default(dispatcher, dummy_queue);
  data_dispatcher_register_default(dispatcher, NULL);
  EXPECT_TRUE(fixed_queue_is_empty(dummy_queue));

  EXPECT_FALSE(data_dispatcher_dispatch(dispatcher, DUMMY_TYPE_0, dummy_data_0));
  EXPECT_TRUE(fixed_queue_is_empty(dummy_queue));

  fixed_queue_free(dummy_queue, NULL);
  data_dispatcher_free(dispatcher);
}

TEST_F(DataDispatcherTest, test_dispatch_edge_zero) {
  data_dispatcher_t *dispatcher = data_dispatcher_new("test_dispatcher");

  // Register a queue
  fixed_queue_t *dummy_queue = fixed_queue_new(DUMMY_QUEUE_SIZE);
  data_dispatcher_register(dispatcher, TYPE_EDGE_CASE_ZERO, dummy_queue);
  EXPECT_TRUE(fixed_queue_is_empty(dummy_queue));

  // Send data to the queue
  EXPECT_TRUE(data_dispatcher_dispatch(dispatcher, TYPE_EDGE_CASE_ZERO, dummy_data_0));

  // Did we get it?
  EXPECT_FALSE(fixed_queue_is_empty(dummy_queue));
  EXPECT_STREQ(dummy_data_0, (char *)fixed_queue_try_dequeue(dummy_queue));
  EXPECT_TRUE(fixed_queue_is_empty(dummy_queue));

  fixed_queue_free(dummy_queue, NULL);
  data_dispatcher_free(dispatcher);
}

TEST_F(DataDispatcherTest, test_dispatch_edge_max) {
  data_dispatcher_t *dispatcher = data_dispatcher_new("test_dispatcher");

  // Register a queue
  fixed_queue_t *dummy_queue = fixed_queue_new(DUMMY_QUEUE_SIZE);
  data_dispatcher_register(dispatcher, TYPE_EDGE_CASE_MAX, dummy_queue);
  EXPECT_TRUE(fixed_queue_is_empty(dummy_queue));

  // Send data to the queue
  EXPECT_TRUE(data_dispatcher_dispatch(dispatcher, TYPE_EDGE_CASE_MAX, dummy_data_0));

  // Did we get it?
  EXPECT_FALSE(fixed_queue_is_empty(dummy_queue));
  EXPECT_STREQ(dummy_data_0, (char *)fixed_queue_try_dequeue(dummy_queue));
  EXPECT_TRUE(fixed_queue_is_empty(dummy_queue));

  fixed_queue_free(dummy_queue, NULL);
  data_dispatcher_free(dispatcher);
}
