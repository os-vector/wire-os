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

#define LOG_TAG "bt_osi_semaphore"

#include "osi/include/semaphore.h"

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <malloc.h>
#include <string.h>
#include <sys/eventfd.h>
#include <unistd.h>

#include "osi/include/allocator.h"
#include "osi/include/log.h"
#include "osi/include/osi.h"

#if !defined(EFD_SEMAPHORE)
#  define EFD_SEMAPHORE (1 << 0)
#endif

struct semaphore_t {
  int fd;
};

semaphore_t *semaphore_new(unsigned int value) {
  semaphore_t *ret = osi_malloc(sizeof(semaphore_t));
  ret->fd = eventfd(value, EFD_SEMAPHORE);
  if (ret->fd == INVALID_FD) {
    LOG_ERROR(LOG_TAG, "%s unable to allocate semaphore: %s", __func__, strerror(errno));
    osi_free(ret);
    ret = NULL;
  }
  return ret;
}

void semaphore_free(semaphore_t *semaphore) {
  if (!semaphore)
    return;

  if (semaphore->fd != INVALID_FD)
    close(semaphore->fd);
  osi_free(semaphore);
}

void semaphore_wait(semaphore_t *semaphore) {
  assert(semaphore != NULL);
  assert(semaphore->fd != INVALID_FD);

  eventfd_t value;
  if (eventfd_read(semaphore->fd, &value) == -1)
    LOG_ERROR(LOG_TAG, "%s unable to wait on semaphore: %s", __func__, strerror(errno));
}

bool semaphore_try_wait(semaphore_t *semaphore) {
  assert(semaphore != NULL);
  assert(semaphore->fd != INVALID_FD);

  int flags = fcntl(semaphore->fd, F_GETFL);
  if (flags == -1) {
    LOG_ERROR(LOG_TAG, "%s unable to get flags for semaphore fd: %s", __func__, strerror(errno));
    return false;
  }
  if (fcntl(semaphore->fd, F_SETFL, flags | O_NONBLOCK) == -1) {
    LOG_ERROR(LOG_TAG, "%s unable to set O_NONBLOCK for semaphore fd: %s", __func__, strerror(errno));
    return false;
  }

  bool rc = true;
  eventfd_t value;
  if (eventfd_read(semaphore->fd, &value) == -1)
    rc = false;

  if (fcntl(semaphore->fd, F_SETFL, flags) == -1)
    LOG_ERROR(LOG_TAG, "%s unable to restore flags for semaphore fd: %s", __func__, strerror(errno));
  return rc;
}

void semaphore_post(semaphore_t *semaphore) {
  assert(semaphore != NULL);
  assert(semaphore->fd != INVALID_FD);

  if (eventfd_write(semaphore->fd, 1ULL) == -1)
    LOG_ERROR(LOG_TAG, "%s unable to post to semaphore: %s", __func__, strerror(errno));
}

int semaphore_get_fd(const semaphore_t *semaphore) {
  assert(semaphore != NULL);
  assert(semaphore->fd != INVALID_FD);
  return semaphore->fd;
}
