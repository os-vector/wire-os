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

#include <assert.h>
#include <pthread.h>
#include <string.h>

#include "osi/include/allocator.h"
#include "osi/include/fixed_queue.h"
#include "osi/include/list.h"
#include "osi/include/osi.h"
#include "osi/include/semaphore.h"
#include "osi/include/reactor.h"

typedef struct fixed_queue_t {
  list_t *list;
  semaphore_t *enqueue_sem;
  semaphore_t *dequeue_sem;
  pthread_mutex_t lock;
  size_t capacity;

  reactor_object_t *dequeue_object;
  fixed_queue_cb dequeue_ready;
  void *dequeue_context;
} fixed_queue_t;

static void internal_dequeue_ready(void *context);

fixed_queue_t *fixed_queue_new(size_t capacity) {
  fixed_queue_t *ret = osi_calloc(sizeof(fixed_queue_t));

  pthread_mutex_init(&ret->lock, NULL);
  ret->capacity = capacity;

  ret->list = list_new(NULL);
  if (!ret->list)
    goto error;

  ret->enqueue_sem = semaphore_new(capacity);
  if (!ret->enqueue_sem)
    goto error;

  ret->dequeue_sem = semaphore_new(0);
  if (!ret->dequeue_sem)
    goto error;

  return ret;

error:
  fixed_queue_free(ret, NULL);
  return NULL;
}

void fixed_queue_free(fixed_queue_t *queue, fixed_queue_free_cb free_cb) {
  if (!queue)
    return;

  fixed_queue_unregister_dequeue(queue);

  if (free_cb)
    for (const list_node_t *node = list_begin(queue->list); node != list_end(queue->list); node = list_next(node))
      free_cb(list_node(node));

  list_free(queue->list);
  semaphore_free(queue->enqueue_sem);
  semaphore_free(queue->dequeue_sem);
  pthread_mutex_destroy(&queue->lock);
  osi_free(queue);
}

bool fixed_queue_is_empty(fixed_queue_t *queue) {
  if (queue == NULL)
    return true;

  pthread_mutex_lock(&queue->lock);
  bool is_empty = list_is_empty(queue->list);
  pthread_mutex_unlock(&queue->lock);

  return is_empty;
}

size_t fixed_queue_length(fixed_queue_t *queue) {
  if (queue == NULL)
    return 0;

  pthread_mutex_lock(&queue->lock);
  size_t length = list_length(queue->list);
  pthread_mutex_unlock(&queue->lock);

  return length;
}

size_t fixed_queue_capacity(fixed_queue_t *queue) {
  assert(queue != NULL);

  return queue->capacity;
}

void fixed_queue_enqueue(fixed_queue_t *queue, void *data) {
  assert(queue != NULL);
  assert(data != NULL);

  semaphore_wait(queue->enqueue_sem);

  pthread_mutex_lock(&queue->lock);
  list_append(queue->list, data);
  pthread_mutex_unlock(&queue->lock);

  semaphore_post(queue->dequeue_sem);
}

void *fixed_queue_dequeue(fixed_queue_t *queue) {
  assert(queue != NULL);

  semaphore_wait(queue->dequeue_sem);

  pthread_mutex_lock(&queue->lock);
  void *ret = list_front(queue->list);
  list_remove(queue->list, ret);
  pthread_mutex_unlock(&queue->lock);

  semaphore_post(queue->enqueue_sem);

  return ret;
}

bool fixed_queue_try_enqueue(fixed_queue_t *queue, void *data) {
  assert(queue != NULL);
  assert(data != NULL);

  if (!semaphore_try_wait(queue->enqueue_sem))
    return false;

  pthread_mutex_lock(&queue->lock);
  list_append(queue->list, data);
  pthread_mutex_unlock(&queue->lock);

  semaphore_post(queue->dequeue_sem);
  return true;
}

void *fixed_queue_try_dequeue(fixed_queue_t *queue) {
  if (queue == NULL)
    return NULL;

  if (!semaphore_try_wait(queue->dequeue_sem))
    return NULL;

  pthread_mutex_lock(&queue->lock);
  void *ret = list_front(queue->list);
  list_remove(queue->list, ret);
  pthread_mutex_unlock(&queue->lock);

  semaphore_post(queue->enqueue_sem);

  return ret;
}

void *fixed_queue_try_peek_first(fixed_queue_t *queue) {
  if (queue == NULL)
    return NULL;

  pthread_mutex_lock(&queue->lock);
  void *ret = list_is_empty(queue->list) ? NULL : list_front(queue->list);
  pthread_mutex_unlock(&queue->lock);

  return ret;
}

void *fixed_queue_try_peek_last(fixed_queue_t *queue) {
  if (queue == NULL)
    return NULL;

  pthread_mutex_lock(&queue->lock);
  void *ret = list_is_empty(queue->list) ? NULL : list_back(queue->list);
  pthread_mutex_unlock(&queue->lock);

  return ret;
}

void *fixed_queue_try_remove_from_queue(fixed_queue_t *queue, void *data) {
  if (queue == NULL)
    return NULL;

  bool removed = false;
  pthread_mutex_lock(&queue->lock);
  if (list_contains(queue->list, data) &&
      semaphore_try_wait(queue->dequeue_sem)) {
    removed = list_remove(queue->list, data);
    assert(removed);
  }
  pthread_mutex_unlock(&queue->lock);

  if (removed) {
    semaphore_post(queue->enqueue_sem);
    return data;
  }
  return NULL;
}

list_t *fixed_queue_get_list(fixed_queue_t *queue) {
  assert(queue != NULL);

  // NOTE: This function is not thread safe, and there is no point for
  // calling pthread_mutex_lock() / pthread_mutex_unlock()
  return queue->list;
}


int fixed_queue_get_dequeue_fd(const fixed_queue_t *queue) {
  assert(queue != NULL);
  return semaphore_get_fd(queue->dequeue_sem);
}

int fixed_queue_get_enqueue_fd(const fixed_queue_t *queue) {
  assert(queue != NULL);
  return semaphore_get_fd(queue->enqueue_sem);
}

void fixed_queue_register_dequeue(fixed_queue_t *queue, reactor_t *reactor, fixed_queue_cb ready_cb, void *context) {
  assert(queue != NULL);
  assert(reactor != NULL);
  assert(ready_cb != NULL);

  // Make sure we're not already registered
  fixed_queue_unregister_dequeue(queue);

  queue->dequeue_ready = ready_cb;
  queue->dequeue_context = context;
  queue->dequeue_object = reactor_register(
    reactor,
    fixed_queue_get_dequeue_fd(queue),
    queue,
    internal_dequeue_ready,
    NULL
  );
}

void fixed_queue_unregister_dequeue(fixed_queue_t *queue) {
  assert(queue != NULL);

  if (queue->dequeue_object) {
    reactor_unregister(queue->dequeue_object);
    queue->dequeue_object = NULL;
  }
}

static void internal_dequeue_ready(void *context) {
  assert(context != NULL);

  fixed_queue_t *queue = context;
  queue->dequeue_ready(queue, queue->dequeue_context);
}
