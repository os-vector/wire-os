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

#define LOG_TAG "bt_osi_reactor"

#include "osi/include/reactor.h"

#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <unistd.h>

#include "osi/include/allocator.h"
#include "osi/include/list.h"
#include "osi/include/log.h"

#if !defined(EFD_SEMAPHORE)
#  define EFD_SEMAPHORE (1 << 0)
#endif

struct reactor_t {
  int epoll_fd;
  int event_fd;
  pthread_mutex_t list_lock;  // protects invalidation_list.
  list_t *invalidation_list;  // reactor objects that have been unregistered.
  pthread_t run_thread;       // the pthread on which reactor_run is executing.
  bool is_running;            // indicates whether |run_thread| is valid.
  bool object_removed;
};

struct reactor_object_t {
  int fd;                              // the file descriptor to monitor for events.
  void *context;                       // a context that's passed back to the *_ready functions.
  reactor_t *reactor;                  // the reactor instance this object is registered with.
  pthread_mutex_t lock;                // protects the lifetime of this object and all variables.

  void (*read_ready)(void *context);   // function to call when the file descriptor becomes readable.
  void (*write_ready)(void *context);  // function to call when the file descriptor becomes writeable.
};

static reactor_status_t run_reactor(reactor_t *reactor, int iterations);

static const size_t MAX_EVENTS = 64;
static const eventfd_t EVENT_REACTOR_STOP = 1;

reactor_t *reactor_new(void) {
  reactor_t *ret = (reactor_t *)osi_calloc(sizeof(reactor_t));

  ret->epoll_fd = INVALID_FD;
  ret->event_fd = INVALID_FD;

  ret->epoll_fd = epoll_create(MAX_EVENTS);
  if (ret->epoll_fd == INVALID_FD) {
    LOG_ERROR(LOG_TAG, "%s unable to create epoll instance: %s", __func__, strerror(errno));
    goto error;
  }

  ret->event_fd = eventfd(0, 0);
  if (ret->event_fd == INVALID_FD) {
    LOG_ERROR(LOG_TAG, "%s unable to create eventfd: %s", __func__, strerror(errno));
    goto error;
  }

  pthread_mutex_init(&ret->list_lock, NULL);
  ret->invalidation_list = list_new(NULL);
  if (!ret->invalidation_list) {
    LOG_ERROR(LOG_TAG, "%s unable to allocate object invalidation list.", __func__);
    goto error;
  }

  struct epoll_event event;
  memset(&event, 0, sizeof(event));
  event.events = EPOLLIN;
  event.data.ptr = NULL;
  if (epoll_ctl(ret->epoll_fd, EPOLL_CTL_ADD, ret->event_fd, &event) == -1) {
    LOG_ERROR(LOG_TAG, "%s unable to register eventfd with epoll set: %s", __func__, strerror(errno));
    goto error;
  }

  return ret;

error:;
  reactor_free(ret);
  return NULL;
}

void reactor_free(reactor_t *reactor) {
  if (!reactor)
    return;

  list_free(reactor->invalidation_list);
  close(reactor->event_fd);
  close(reactor->epoll_fd);
  osi_free(reactor);
}

reactor_status_t reactor_start(reactor_t *reactor) {
  assert(reactor != NULL);
  return run_reactor(reactor, 0);
}

reactor_status_t reactor_run_once(reactor_t *reactor) {
  assert(reactor != NULL);
  return run_reactor(reactor, 1);
}

void reactor_stop(reactor_t *reactor) {
  assert(reactor != NULL);

  eventfd_write(reactor->event_fd, EVENT_REACTOR_STOP);
}

reactor_object_t *reactor_register(reactor_t *reactor,
    int fd, void *context,
    void (*read_ready)(void *context),
    void (*write_ready)(void *context)) {
  assert(reactor != NULL);
  assert(fd != INVALID_FD);

  reactor_object_t *object =
      (reactor_object_t *)osi_calloc(sizeof(reactor_object_t));

  object->reactor = reactor;
  object->fd = fd;
  object->context = context;
  object->read_ready = read_ready;
  object->write_ready = write_ready;
  pthread_mutex_init(&object->lock, NULL);

  struct epoll_event event;
  memset(&event, 0, sizeof(event));
  if (read_ready)
    event.events |= (EPOLLIN | EPOLLRDHUP);
  if (write_ready)
    event.events |= EPOLLOUT;
  event.data.ptr = object;

  if (epoll_ctl(reactor->epoll_fd, EPOLL_CTL_ADD, fd, &event) == -1) {
    LOG_ERROR(LOG_TAG, "%s unable to register fd %d to epoll set: %s", __func__, fd, strerror(errno));
    pthread_mutex_destroy(&object->lock);
    osi_free(object);
    return NULL;
  }

  return object;
}

bool reactor_change_registration(reactor_object_t *object,
    void (*read_ready)(void *context),
    void (*write_ready)(void *context)) {
  assert(object != NULL);

  struct epoll_event event;
  memset(&event, 0, sizeof(event));
  if (read_ready)
    event.events |= (EPOLLIN | EPOLLRDHUP);
  if (write_ready)
    event.events |= EPOLLOUT;
  event.data.ptr = object;

  if (epoll_ctl(object->reactor->epoll_fd, EPOLL_CTL_MOD, object->fd, &event) == -1) {
    LOG_ERROR(LOG_TAG, "%s unable to modify interest set for fd %d: %s", __func__, object->fd, strerror(errno));
    return false;
  }

  pthread_mutex_lock(&object->lock);
  object->read_ready = read_ready;
  object->write_ready = write_ready;
  pthread_mutex_unlock(&object->lock);

  return true;
}

void reactor_unregister(reactor_object_t *obj) {
  assert(obj != NULL);

  reactor_t *reactor = obj->reactor;

  if (epoll_ctl(reactor->epoll_fd, EPOLL_CTL_DEL, obj->fd, NULL) == -1)
    LOG_ERROR(LOG_TAG, "%s unable to unregister fd %d from epoll set: %s", __func__, obj->fd, strerror(errno));

  if (reactor->is_running && pthread_equal(pthread_self(), reactor->run_thread)) {
    reactor->object_removed = true;
    return;
  }

  pthread_mutex_lock(&reactor->list_lock);
  list_append(reactor->invalidation_list, obj);
  pthread_mutex_unlock(&reactor->list_lock);

  // Taking the object lock here makes sure a callback for |obj| isn't
  // currently executing. The reactor thread must then either be before
  // the callbacks or after. If after, we know that the object won't be
  // referenced because it has been taken out of the epoll set. If before,
  // it won't be referenced because the reactor thread will check the
  // invalidation_list and find it in there. So by taking this lock, we
  // are waiting until the reactor thread drops all references to |obj|.
  // One the wait completes, we can unlock and destroy |obj| safely.
  pthread_mutex_lock(&obj->lock);
  pthread_mutex_unlock(&obj->lock);
  pthread_mutex_destroy(&obj->lock);
  osi_free(obj);
}

// Runs the reactor loop for a maximum of |iterations|.
// 0 |iterations| means loop forever.
// |reactor| may not be NULL.
static reactor_status_t run_reactor(reactor_t *reactor, int iterations) {
  assert(reactor != NULL);

  reactor->run_thread = pthread_self();
  reactor->is_running = true;

  struct epoll_event events[MAX_EVENTS];
  for (int i = 0; iterations == 0 || i < iterations; ++i) {
    pthread_mutex_lock(&reactor->list_lock);
    list_clear(reactor->invalidation_list);
    pthread_mutex_unlock(&reactor->list_lock);

    int ret;
    OSI_NO_INTR(ret = epoll_wait(reactor->epoll_fd, events, MAX_EVENTS, -1));
    if (ret == -1) {
      LOG_ERROR(LOG_TAG, "%s error in epoll_wait: %s", __func__, strerror(errno));
      reactor->is_running = false;
      return REACTOR_STATUS_ERROR;
    }

    for (int j = 0; j < ret; ++j) {
      // The event file descriptor is the only one that registers with
      // a NULL data pointer. We use the NULL to identify it and break
      // out of the reactor loop.
      if (events[j].data.ptr == NULL) {
        eventfd_t value;
        eventfd_read(reactor->event_fd, &value);
        reactor->is_running = false;
        return REACTOR_STATUS_STOP;
      }

      reactor_object_t *object = (reactor_object_t *)events[j].data.ptr;

      pthread_mutex_lock(&reactor->list_lock);
      if (list_contains(reactor->invalidation_list, object)) {
        pthread_mutex_unlock(&reactor->list_lock);
        continue;
      }

      // Downgrade the list lock to an object lock.
      pthread_mutex_lock(&object->lock);
      pthread_mutex_unlock(&reactor->list_lock);

      reactor->object_removed = false;
      if (events[j].events & (EPOLLIN | EPOLLHUP | EPOLLRDHUP | EPOLLERR) && object->read_ready)
        object->read_ready(object->context);
      if (!reactor->object_removed && events[j].events & EPOLLOUT && object->write_ready)
        object->write_ready(object->context);
      pthread_mutex_unlock(&object->lock);

      if (reactor->object_removed) {
        pthread_mutex_destroy(&object->lock);
        osi_free(object);
      }
    }
  }

  reactor->is_running = false;
  return REACTOR_STATUS_DONE;
}
