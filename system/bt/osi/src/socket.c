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

#define LOG_TAG "bt_osi_socket"

#include "osi/include/socket.h"

#include <asm/ioctls.h>
#include <assert.h>
#include <errno.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>

#include "osi/include/allocator.h"
#include "osi/include/log.h"
#include "osi/include/osi.h"
#include "osi/include/reactor.h"

// The IPv4 loopback address: 127.0.0.1
static const in_addr_t LOCALHOST_ = 0x7f000001;

struct socket_t {
  int fd;
  reactor_object_t *reactor_object;
  socket_cb read_ready;
  socket_cb write_ready;
  void *context;                     // Not owned, do not free.
};

static void internal_read_ready(void *context);
static void internal_write_ready(void *context);

socket_t *socket_new(void) {
  socket_t *ret = (socket_t *)osi_calloc(sizeof(socket_t));

  if (ret == NULL) {
    LOG_ERROR(LOG_TAG, "%s unable to allocate : %s", __func__, strerror(errno));
    return NULL;
  }

  ret->fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (ret->fd == INVALID_FD) {
    LOG_ERROR(LOG_TAG, "%s unable to create socket: %s", __func__, strerror(errno));
    goto error;
  }

  int enable = 1;
  if (setsockopt(ret->fd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable)) == -1) {
    LOG_ERROR(LOG_TAG, "%s unable to set SO_REUSEADDR: %s", __func__, strerror(errno));
    goto error;
  }

  return ret;

error:;
  if (ret)
    close(ret->fd);
  osi_free(ret);
  return NULL;
}

socket_t *socket_new_from_fd(int fd) {
  assert(fd != INVALID_FD);

  socket_t *ret = (socket_t *)osi_calloc(sizeof(socket_t));

  ret->fd = fd;
  return ret;
}

void socket_free(socket_t *socket) {
  if (!socket)
    return;

  socket_unregister(socket);
  close(socket->fd);
  osi_free(socket);
}

bool socket_listen(const socket_t *socket, port_t port) {
  assert(socket != NULL);

  struct sockaddr_in addr;
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(LOCALHOST_);
  addr.sin_port = htons(port);
  if (bind(socket->fd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
    LOG_ERROR(LOG_TAG, "%s unable to bind socket to port %u: %s", __func__, port, strerror(errno));
    return false;
  }

  if (listen(socket->fd, 10) == -1) {
    LOG_ERROR(LOG_TAG, "%s unable to listen on port %u: %s", __func__, port, strerror(errno));
    return false;
  }

  return true;
}

socket_t *socket_accept(const socket_t *socket) {
  assert(socket != NULL);

  int fd;
  OSI_NO_INTR(fd = accept(socket->fd, NULL, NULL));
  if (fd == INVALID_FD) {
    LOG_ERROR(LOG_TAG, "%s unable to accept socket: %s", __func__, strerror(errno));
    return NULL;
  }

  socket_t *ret = (socket_t *)osi_calloc(sizeof(socket_t));

  ret->fd = fd;
  return ret;
}

ssize_t socket_read(const socket_t *socket, void *buf, size_t count) {
  assert(socket != NULL);
  assert(buf != NULL);

  ssize_t ret;
  OSI_NO_INTR(ret = recv(socket->fd, buf, count, MSG_DONTWAIT));

  return ret;
}

ssize_t socket_write(const socket_t *socket, const void *buf, size_t count) {
  assert(socket != NULL);
  assert(buf != NULL);

  ssize_t ret;
  OSI_NO_INTR(ret = send(socket->fd, buf, count, MSG_DONTWAIT));

  return ret;
}

ssize_t socket_write_and_transfer_fd(const socket_t *socket, const void *buf, size_t count, int fd) {
  assert(socket != NULL);
  assert(buf != NULL);

  if (fd == INVALID_FD)
    return socket_write(socket, buf, count);

  struct msghdr msg;
  struct iovec iov;
  char control_buf[CMSG_SPACE(sizeof(int))];

  iov.iov_base = (void *)buf;
  iov.iov_len = count;

  msg.msg_iov = &iov;
  msg.msg_iovlen = 1;
  msg.msg_control = control_buf;
  msg.msg_controllen = sizeof(control_buf);
  msg.msg_name = NULL;
  msg.msg_namelen = 0;

  struct cmsghdr *header = CMSG_FIRSTHDR(&msg);
  header->cmsg_level = SOL_SOCKET;
  header->cmsg_type = SCM_RIGHTS;
  header->cmsg_len = CMSG_LEN(sizeof(int));
  *(int *)CMSG_DATA(header) = fd;

  ssize_t ret;
  OSI_NO_INTR(ret = sendmsg(socket->fd, &msg, MSG_DONTWAIT));

  close(fd);
  return ret;
}

ssize_t socket_bytes_available(const socket_t *socket) {
  assert(socket != NULL);

  int size = 0;
  if (ioctl(socket->fd, FIONREAD, &size) == -1)
    return -1;
  return size;
}

void socket_register(socket_t *socket, reactor_t *reactor, void *context, socket_cb read_cb, socket_cb write_cb) {
  assert(socket != NULL);

  // Make sure the socket isn't currently registered.
  socket_unregister(socket);

  socket->read_ready = read_cb;
  socket->write_ready = write_cb;
  socket->context = context;

  void (*read_fn)(void *) = (read_cb != NULL) ? internal_read_ready : NULL;
  void (*write_fn)(void *) = (write_cb != NULL) ? internal_write_ready : NULL;

  socket->reactor_object = reactor_register(reactor, socket->fd, socket, read_fn, write_fn);
}

void socket_unregister(socket_t *socket) {
  assert(socket != NULL);

  if (socket->reactor_object)
    reactor_unregister(socket->reactor_object);
  socket->reactor_object = NULL;
}

static void internal_read_ready(void *context) {
  assert(context != NULL);

  socket_t *socket = (void *)context;
  socket->read_ready(socket, socket->context);
}

static void internal_write_ready(void *context) {
  assert(context != NULL);

  socket_t *socket = (void *)context;
  socket->write_ready(socket, socket->context);
}
