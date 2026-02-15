/******************************************************************************
 *
 *  Copyright (C) 2013 Google, Inc.
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

#define LOG_TAG "bt_snoop_net"

#include <assert.h>
#include <cutils/sockets.h>
#include <sys/un.h>
#include <sys/poll.h>
#include <errno.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdbool.h>
#include <string.h>
#include <sys/prctl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#ifndef ANDROID
#include <sys/time.h>
#include <stdlib.h>
#endif

#include "osi/include/log.h"
#include "osi/include/osi.h"
#include "osi/include/compat.h"

static void safe_close_(int *fd);
static void *listen_fn_(void *context);

static const char *LISTEN_THREAD_NAME_ = "btsnoop_net_listen";
static const int LOCALHOST_ = 0x7F000001;
static const int LISTEN_PORT_ = 8872;

static pthread_t listen_thread_;
static bool listen_thread_valid_ = false;
static pthread_mutex_t client_socket_lock_ = PTHREAD_MUTEX_INITIALIZER;
static int listen_socket_ = -1;
int client_socket_btsnoop = -1;

/*
    local socket for writing from different process
    to limit blocking of HCI threads.
*/
#define LOCAL_SOCKET_NAME "bthcitraffic"
static int listen_socket_local_ = -1;

static int local_socket_create(void) {
#ifndef ANDROID
  struct sockaddr_un addr;
#endif

  listen_socket_local_ = socket(AF_LOCAL, SOCK_STREAM, 0);
  if(listen_socket_local_ < 0) {
    return -1;
  }

#ifdef ANDROID
  if(socket_local_server_bind(listen_socket_local_, LOCAL_SOCKET_NAME,
      ANDROID_SOCKET_NAMESPACE_ABSTRACT) < 0) {
#else
  memset(&addr, 0, sizeof(addr));
  addr.sun_family = AF_LOCAL;
  strlcpy(addr.sun_path, LOCAL_SOCKET_NAME, sizeof(addr.sun_path));
  unlink(LOCAL_SOCKET_NAME);
  if (bind(listen_socket_local_, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
#endif
    LOG_ERROR("Failed to create Local Socket (%s)", strerror(errno));
    return -1;
  }

  if (listen(listen_socket_local_, 1) < 0) {
    LOG_ERROR(LOG_TAG, "Local socket listen failed (%s)", strerror(errno));
    close(listen_socket_local_);
    return -1;
  }
  return listen_socket_local_;
}

void btsnoop_net_open() {

  listen_thread_valid_ = (pthread_create(&listen_thread_, NULL, listen_fn_, NULL) == 0);
  if (!listen_thread_valid_) {
    LOG_ERROR(LOG_TAG, "%s pthread_create failed: %s", __func__, strerror(errno));
  } else {
    LOG_DEBUG(LOG_TAG, "initialized");
  }
}

void btsnoop_net_close() {

  if (listen_thread_valid_) {
#if (defined(BT_NET_DEBUG) && (BT_NET_DEBUG == TRUE))
    // Disable using network sockets for security reasons
    shutdown(listen_socket_, SHUT_RDWR);
#endif
    shutdown(listen_socket_local_, SHUT_RDWR);
    pthread_join(listen_thread_, NULL);
    safe_close_(&client_socket_btsnoop);
    listen_thread_valid_ = false;
  }
}

void btsnoop_net_write(const void *data, size_t length) {
  ssize_t ret;

  pthread_mutex_lock(&client_socket_lock_);
  if (client_socket_btsnoop != -1) {
    do {
      if ((ret = send(client_socket_btsnoop, data, length, 0)) == -1 && errno == ECONNRESET) {
        safe_close_(&client_socket_btsnoop);
        LOG_INFO(LOG_TAG, "%s conn closed", __func__);
      }
      if ((size_t) ret < length) {
        LOG_ERROR(LOG_TAG, "%s: send : not able to write complete packet", __func__);
      }
      length -= ret;
    } while ((length > 0) && (ret != -1));
  }

  pthread_mutex_unlock(&client_socket_lock_);
}

static void *listen_fn_(UNUSED_ATTR void *context) {
  fd_set sock_fds;
  int fd_max = -1, retval;

  prctl(PR_SET_NAME, (unsigned long)LISTEN_THREAD_NAME_, 0, 0, 0);

  FD_ZERO(&sock_fds);

  // Disable using network sockets for security reasons
  listen_socket_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (listen_socket_ == -1) {
    LOG_ERROR(LOG_TAG, "%s socket creation failed: %s", __func__, strerror(errno));
    goto cleanup;
  }
  FD_SET(listen_socket_, &sock_fds);
  fd_max = listen_socket_;

  int enable = 1;
  if (setsockopt(listen_socket_, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable)) == -1) {
    LOG_ERROR(LOG_TAG, "%s unable to set SO_REUSEADDR: %s", __func__, strerror(errno));
    goto cleanup;
  }

  struct sockaddr_in addr;
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(LOCALHOST_);
  addr.sin_port = htons(LISTEN_PORT_);
  if (bind(listen_socket_, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
    LOG_ERROR(LOG_TAG, "%s unable to bind listen socket: %s", __func__, strerror(errno));
    goto cleanup;
  }

  if (listen(listen_socket_, 10) == -1) {
    LOG_ERROR(LOG_TAG, "%s unable to listen: %s", __func__, strerror(errno));
    goto cleanup;
  }

  if (local_socket_create() != -1) {
    if (listen_socket_local_ > fd_max) {
      fd_max = listen_socket_local_;
    }
    FD_SET(listen_socket_local_, &sock_fds);
  }

  if (fd_max == -1) {
    LOG_ERROR(LOG_TAG, "%s No sockets to wait for conn..", __func__);
    return NULL;
  }

  for (;;) {
    int client_socket = -1;

    LOG_DEBUG(LOG_TAG, "waiting for client connection");

    if ((retval = select(fd_max + 1, &sock_fds, NULL, NULL, NULL)) == -1) {
      LOG_ERROR(LOG_TAG, "%s select failed %s", __func__, strerror(errno));
      goto cleanup;
    }

    if ((listen_socket_ != -1) && FD_ISSET(listen_socket_, &sock_fds)) {
      struct sockaddr_in cli_addr;
      socklen_t length = sizeof(cli_addr);
      client_socket = accept(listen_socket_, (struct sockaddr *) &cli_addr, &length);
      if (client_socket == -1) {
        if (errno == EINVAL || errno == EBADF) {
          LOG_WARN(LOG_TAG, "%s error accepting TCP socket: %s", __func__, strerror(errno));
          break;
        }
        LOG_WARN(LOG_TAG, "%s error accepting TCP socket: %s", __func__, strerror(errno));
        continue;
      }
    } else if ((listen_socket_local_ != -1) && FD_ISSET(listen_socket_local_, &sock_fds)){
      struct sockaddr_un cliaddr;
      socklen_t length = sizeof(cliaddr);

      client_socket = accept(listen_socket_local_, (struct sockaddr *)&cliaddr, (socklen_t *)&length);
      if (client_socket == -1) {
        if (errno == EINVAL || errno == EBADF) {
          LOG_WARN(LOG_TAG, "%s error accepting LOCAL socket: %s", __func__, strerror(errno));
          break;
        }
        LOG_WARN(LOG_TAG, "%s error accepting LOCAL socket: %s", __func__, strerror(errno));
        continue;
      }
    }

    /* When a new client connects, we have to send the btsnoop file header. This allows
       a decoder to treat the session as a new, valid btsnoop file. */
    pthread_mutex_lock(&client_socket_lock_);
    safe_close_(&client_socket_btsnoop);
    client_socket_btsnoop = client_socket;
    send(client_socket_btsnoop, "btsnoop\0\0\0\0\1\0\0\x3\xea", 16, 0);
    pthread_mutex_unlock(&client_socket_lock_);

    FD_ZERO(&sock_fds);
    if(listen_socket_ != -1) {
      FD_SET(listen_socket_, &sock_fds);
    }
    if(listen_socket_local_ != -1) {
        FD_SET(listen_socket_local_, &sock_fds);
    }
  }

cleanup:
  safe_close_(&listen_socket_);
  safe_close_(&listen_socket_local_);
  return NULL;
}

static void safe_close_(int *fd) {
  assert(fd != NULL);
  if (*fd != -1) {
    close(*fd);
    *fd = -1;
  }
}
