#include <gtest/gtest.h>
#include <pthread.h>
#include <sys/eventfd.h>
#include <sys/time.h>
#include <unistd.h>

#include "AllocationTestHarness.h"

extern "C" {
#include "osi/include/reactor.h"
}

class ReactorTest : public AllocationTestHarness {};

static pthread_t thread;
static volatile bool thread_running;

static void *reactor_thread(void *ptr) {
  reactor_t *reactor = (reactor_t *)ptr;

  thread_running = true;
  reactor_start(reactor);
  thread_running = false;

  return NULL;
}

static void spawn_reactor_thread(reactor_t *reactor) {
  int ret = pthread_create(&thread, NULL, reactor_thread, reactor);
  EXPECT_EQ(ret, 0);
}

static void join_reactor_thread() {
  pthread_join(thread, NULL);
}

TEST_F(ReactorTest, reactor_new) {
  reactor_t *reactor = reactor_new();
  EXPECT_TRUE(reactor != NULL);
  reactor_free(reactor);
}

TEST_F(ReactorTest, reactor_free_null) {
  reactor_free(NULL);
}

TEST_F(ReactorTest, reactor_stop_start) {
  reactor_t *reactor = reactor_new();
  reactor_stop(reactor);
  reactor_start(reactor);
  reactor_free(reactor);
}

TEST_F(ReactorTest, reactor_repeated_stop_start) {
  reactor_t *reactor = reactor_new();
  for (int i = 0; i < 10; ++i) {
    reactor_stop(reactor);
    reactor_start(reactor);
  }
  reactor_free(reactor);
}

TEST_F(ReactorTest, reactor_start_wait_stop) {
  reactor_t *reactor = reactor_new();

  spawn_reactor_thread(reactor);
  usleep(50 * 1000);
  EXPECT_TRUE(thread_running);

  reactor_stop(reactor);
  join_reactor_thread();
  EXPECT_FALSE(thread_running);

  reactor_free(reactor);
}

typedef struct {
  reactor_t *reactor;
  reactor_object_t *object;
} unregister_arg_t;

static void unregister_cb(void *context) {
  unregister_arg_t *arg = (unregister_arg_t *)context;
  reactor_unregister(arg->object);
  reactor_stop(arg->reactor);
}

TEST_F(ReactorTest, reactor_unregister_from_callback) {
  reactor_t *reactor = reactor_new();

  int fd = eventfd(0, 0);
  unregister_arg_t arg;
  arg.reactor = reactor;
  arg.object = reactor_register(reactor, fd, &arg, unregister_cb, NULL);
  spawn_reactor_thread(reactor);
  eventfd_write(fd, 1);

  join_reactor_thread();

  close(fd);
  reactor_free(reactor);
}

TEST_F(ReactorTest, reactor_unregister_from_separate_thread) {
  reactor_t *reactor = reactor_new();

  int fd = eventfd(0, 0);

  reactor_object_t *object = reactor_register(reactor, fd, NULL, NULL, NULL);
  spawn_reactor_thread(reactor);
  usleep(50 * 1000);
  reactor_unregister(object);

  reactor_stop(reactor);
  join_reactor_thread();

  close(fd);
  reactor_free(reactor);
}
