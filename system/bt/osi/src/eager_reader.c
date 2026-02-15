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

#define LOG_TAG "bt_osi_eager_reader"

#include "osi/include/eager_reader.h"

#include <assert.h>
#include <errno.h>
#include <string.h>
#include <sys/eventfd.h>
#include <unistd.h>
#ifndef ANDROID
#include <sys/time.h>
#endif

#include "osi/include/fixed_queue.h"
#include "osi/include/log.h"
#include "osi/include/osi.h"
#include "osi/include/reactor.h"

#if !defined(EFD_SEMAPHORE)
#  define EFD_SEMAPHORE (1 << 0)
#endif

typedef struct {
  size_t length;
  size_t offset;
  uint8_t data[];
} data_buffer_t;

struct eager_reader_t {
  int bytes_available_fd; // semaphore mode eventfd which counts the number of available bytes
  int inbound_fd;

  const allocator_t *allocator;
  size_t buffer_size;
  fixed_queue_t *buffers;
  data_buffer_t *current_buffer;

  thread_t *inbound_read_thread;
  reactor_object_t *inbound_read_object;

  reactor_object_t *outbound_registration;
  eager_reader_cb outbound_read_ready;
  void *outbound_context;
};

static bool has_byte(const eager_reader_t *reader);
static void inbound_data_waiting(void *context);
static void internal_outbound_read_ready(void *context);

eager_reader_t *eager_reader_new(
    int fd_to_read,
    const allocator_t *allocator,
    size_t buffer_size,
    size_t max_buffer_count,
    const char *thread_name) {

  assert(fd_to_read != INVALID_FD);
  assert(allocator != NULL);
  assert(buffer_size > 0);
  assert(max_buffer_count > 0);
  assert(thread_name != NULL && *thread_name != '\0');

  eager_reader_t *ret = osi_calloc(sizeof(eager_reader_t));

  ret->allocator = allocator;
  ret->inbound_fd = fd_to_read;

  ret->bytes_available_fd = eventfd(0, 0);
  if (ret->bytes_available_fd == INVALID_FD) {
    LOG_ERROR(LOG_TAG, "%s unable to create output reading semaphore.", __func__);
    goto error;
  }

  ret->buffer_size = buffer_size;

  ret->buffers = fixed_queue_new(max_buffer_count);
  if (!ret->buffers) {
    LOG_ERROR(LOG_TAG, "%s unable to create buffers queue.", __func__);
    goto error;
  }

  ret->inbound_read_thread = thread_new(thread_name);
  if (!ret->inbound_read_thread) {
    LOG_ERROR(LOG_TAG, "%s unable to make reading thread.", __func__);
    goto error;
  }

  ret->inbound_read_object = reactor_register(
    thread_get_reactor(ret->inbound_read_thread),
    fd_to_read,
    ret,
    inbound_data_waiting,
    NULL
  );

  return ret;

error:;
  eager_reader_free(ret);
  return NULL;
}

void eager_reader_free(eager_reader_t *reader) {
  if (!reader)
    return;

  eager_reader_unregister(reader);

  // Only unregister from the input if we actually did register
  if (reader->inbound_read_object)
    reactor_unregister(reader->inbound_read_object);

  if (reader->bytes_available_fd != INVALID_FD)
    close(reader->bytes_available_fd);

  // Free the current buffer, because it's not in the queue
  // and won't be freed below
  if (reader->current_buffer)
    reader->allocator->free(reader->current_buffer);

  fixed_queue_free(reader->buffers, reader->allocator->free);
  thread_free(reader->inbound_read_thread);
  osi_free(reader);
}

void eager_reader_register(eager_reader_t *reader, reactor_t *reactor, eager_reader_cb read_cb, void *context) {
  assert(reader != NULL);
  assert(reactor != NULL);
  assert(read_cb != NULL);

  // Make sure the reader isn't currently registered.
  eager_reader_unregister(reader);

  reader->outbound_read_ready = read_cb;
  reader->outbound_context = context;
  reader->outbound_registration = reactor_register(reactor, reader->bytes_available_fd, reader, internal_outbound_read_ready, NULL);
}

void eager_reader_unregister(eager_reader_t *reader) {
  assert(reader != NULL);

  if (reader->outbound_registration) {
    reactor_unregister(reader->outbound_registration);
    reader->outbound_registration = NULL;
  }
}

// SEE HEADER FOR THREAD SAFETY NOTE
size_t eager_reader_read(eager_reader_t *reader, uint8_t *buffer, size_t max_size) {
  assert(reader != NULL);
  assert(buffer != NULL);

  // Poll to see if we have any bytes available before reading.
  if (!has_byte(reader))
    return 0;

  // Find out how many bytes we have available in our various buffers.
  eventfd_t bytes_available;
  if (eventfd_read(reader->bytes_available_fd, &bytes_available) == -1) {
    LOG_ERROR(LOG_TAG, "%s unable to read semaphore for output data.", __func__);
    return 0;
  }

  if (max_size > bytes_available)
    max_size = bytes_available;

  size_t bytes_consumed = 0;
  while (bytes_consumed < max_size) {
    if (!reader->current_buffer)
      reader->current_buffer = fixed_queue_dequeue(reader->buffers);

    size_t bytes_to_copy = reader->current_buffer->length - reader->current_buffer->offset;
    if (bytes_to_copy > (max_size - bytes_consumed))
      bytes_to_copy = max_size - bytes_consumed;

    memcpy(&buffer[bytes_consumed], &reader->current_buffer->data[reader->current_buffer->offset], bytes_to_copy);
    bytes_consumed += bytes_to_copy;
    reader->current_buffer->offset += bytes_to_copy;

    if (reader->current_buffer->offset >= reader->current_buffer->length) {
      reader->allocator->free(reader->current_buffer);
      reader->current_buffer = NULL;
    }
  }

  bytes_available -= bytes_consumed;
  if (eventfd_write(reader->bytes_available_fd, bytes_available) == -1) {
    LOG_ERROR(LOG_TAG, "%s unable to write back bytes available for output data.", __func__);
  }

  return bytes_consumed;
}

thread_t* eager_reader_get_read_thread(const eager_reader_t *reader) {
  assert(reader != NULL);
  return reader->inbound_read_thread;
}

static bool has_byte(const eager_reader_t *reader) {
  assert(reader != NULL);

  fd_set read_fds;

  for (;;) {
    FD_ZERO(&read_fds);
    FD_SET(reader->bytes_available_fd, &read_fds);

    // Immediate timeout
    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 0;

    int ret = select(reader->bytes_available_fd + 1, &read_fds, NULL, NULL,
                     &timeout);
    if (ret == -1 && errno == EINTR)
      continue;
    break;
  }

  return FD_ISSET(reader->bytes_available_fd, &read_fds);
}

static void inbound_data_waiting(void *context) {
  eager_reader_t *reader = (eager_reader_t *)context;

  data_buffer_t *buffer = (data_buffer_t *)reader->allocator->alloc(reader->buffer_size + sizeof(data_buffer_t));
  if (!buffer) {
    LOG_ERROR(LOG_TAG, "%s couldn't aquire memory for inbound data buffer.", __func__);
    return;
  }

  buffer->length = 0;
  buffer->offset = 0;

  ssize_t bytes_read;
  OSI_NO_INTR(bytes_read = read(reader->inbound_fd, buffer->data,
                                reader->buffer_size));
  if (bytes_read > 0) {
    // Save the data for later
    buffer->length = bytes_read;
    fixed_queue_enqueue(reader->buffers, buffer);

    // Tell consumers data is available by incrementing
    // the semaphore by the number of bytes we just read
    eventfd_write(reader->bytes_available_fd, bytes_read);
  } else {
    if (bytes_read == 0){
      LOG_WARN(LOG_TAG, "%s fd said bytes existed, but none were found.", __func__);
    } else
      LOG_WARN(LOG_TAG, "%s unable to read from file descriptor: %s", __func__, strerror(errno));

    reader->allocator->free(buffer);
  }
}

static void internal_outbound_read_ready(void *context) {
  assert(context != NULL);

  eager_reader_t *reader = (eager_reader_t *)context;
  reader->outbound_read_ready(reader, reader->outbound_context);
}
