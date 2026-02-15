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

#define LOG_TAG "bt_btif_sock_sco"

#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <hardware/bluetooth.h>
#include <hardware/bt_sock.h>

#include "btif_common.h"
#include "osi/include/allocator.h"
#include "osi/include/list.h"
#include "osi/include/log.h"
#include "osi/include/osi.h"
#include "osi/include/socket.h"
#include "osi/include/thread.h"

// This module provides a socket abstraction for SCO connections to a higher
// layer. It returns file descriptors representing two types of sockets:
// listening (server) and connected (client) sockets. No SCO data is
// transferred across these sockets; instead, they are used to manage SCO
// connection lifecycles while the data routing takes place over the I2S bus.
//
// This code bridges the gap between the BTM layer, which implements SCO
// connections, and the Android HAL. It adapts the BTM representation of SCO
// connections (integer handles) to a file descriptor representation usable by
// Android's LocalSocket implementation.
//
// Sample flow for an incoming connection:
//   btsock_sco_listen()       - listen for incoming connections
//   connection_request_cb()   - incoming connection request from remote host
//   connect_completed_cb()    - connection successfully established
//   socket_read_ready_cb()    - local host closed SCO socket
//   disconnect_completed_cb() - connection terminated

typedef struct {
  uint16_t sco_handle;
  socket_t *socket;
  bool connect_completed;
} sco_socket_t;

// TODO: verify packet types that are being sent OTA.
static tBTM_ESCO_PARAMS sco_parameters = {
    BTM_64KBITS_RATE,                   /* TX Bandwidth (64 kbits/sec)              */
    BTM_64KBITS_RATE,                   /* RX Bandwidth (64 kbits/sec)              */
    0x000a,                             /* 10 ms (HS/HF can use EV3, 2-EV3, 3-EV3)  */
    0x0060,                             /* Inp Linear, Air CVSD, 2s Comp, 16bit     */
    (BTM_SCO_LINK_ALL_PKT_MASK       |
     BTM_SCO_PKT_TYPES_MASK_NO_2_EV5 |
     BTM_SCO_PKT_TYPES_MASK_NO_3_EV5),
     BTM_ESCO_RETRANS_POWER       /* Retransmission effort                      */
};

static sco_socket_t *sco_socket_establish_locked(bool is_listening, const bt_bdaddr_t *bd_addr, int *sock_fd);
static sco_socket_t *sco_socket_new(void);
static void sco_socket_free_locked(sco_socket_t *socket);
static sco_socket_t *sco_socket_find_locked(uint16_t sco_handle);
static void connection_request_cb(tBTM_ESCO_EVT event, tBTM_ESCO_EVT_DATA *data);
static void connect_completed_cb(uint16_t sco_handle);
static void disconnect_completed_cb(uint16_t sco_handle);
static void socket_read_ready_cb(socket_t *socket, void *context);

// |lock| protects all of the static variables below and
// calls into the BTM layer.
static pthread_mutex_t lock;
static list_t *sco_sockets;              // Owns a collection of sco_socket_t objects.
static sco_socket_t *listen_sco_socket;  // Not owned, do not free.
static thread_t *thread;                 // Not owned, do not free.

bt_status_t btsock_sco_init(thread_t *thread_) {
  assert(thread_ != NULL);

  sco_sockets = list_new((list_free_cb)sco_socket_free_locked);
  if (!sco_sockets)
    return BT_STATUS_FAIL;

  pthread_mutex_init(&lock, NULL);

  thread = thread_;
  BTM_SetEScoMode(BTM_LINK_TYPE_ESCO, &sco_parameters);

  return BT_STATUS_SUCCESS;
}

bt_status_t btsock_sco_cleanup(void) {
  list_free(sco_sockets);
  sco_sockets = NULL;
  pthread_mutex_destroy(&lock);
  return BT_STATUS_SUCCESS;
}

bt_status_t btsock_sco_listen(int *sock_fd, UNUSED_ATTR int flags) {
  assert(sock_fd != NULL);

  pthread_mutex_lock(&lock);

  sco_socket_t *sco_socket = sco_socket_establish_locked(true, NULL, sock_fd);
  if (sco_socket) {
    BTM_RegForEScoEvts(sco_socket->sco_handle, connection_request_cb);
    listen_sco_socket = sco_socket;
  }

  pthread_mutex_unlock(&lock);

  return sco_socket ? BT_STATUS_SUCCESS : BT_STATUS_FAIL;
}

bt_status_t btsock_sco_connect(const bt_bdaddr_t *bd_addr, int *sock_fd, UNUSED_ATTR int flags) {
  assert(bd_addr != NULL);
  assert(sock_fd != NULL);

  pthread_mutex_lock(&lock);
  sco_socket_t *sco_socket = sco_socket_establish_locked(false, bd_addr, sock_fd);
  pthread_mutex_unlock(&lock);

  return (sco_socket != NULL) ? BT_STATUS_SUCCESS : BT_STATUS_FAIL;
}

// Must be called with |lock| held.
static sco_socket_t *sco_socket_establish_locked(bool is_listening, const bt_bdaddr_t *bd_addr, int *sock_fd) {
  int pair[2] = { INVALID_FD, INVALID_FD };
  sco_socket_t *sco_socket = NULL;

  if (socketpair(AF_LOCAL, SOCK_STREAM, 0, pair) == -1) {
    LOG_ERROR(LOG_TAG, "%s unable to allocate socket pair: %s", __func__, strerror(errno));
    goto error;
  }

  sco_socket = sco_socket_new();
  if (!sco_socket) {
    LOG_ERROR(LOG_TAG, "%s unable to allocate new SCO socket.", __func__);
    goto error;
  }

  tBTM_STATUS status = BTM_CreateSco((uint8_t *)bd_addr, !is_listening, sco_parameters.packet_types, &sco_socket->sco_handle, connect_completed_cb, disconnect_completed_cb);
  if (status != BTM_CMD_STARTED) {
    LOG_ERROR(LOG_TAG, "%s unable to create SCO socket: %d", __func__, status);
    goto error;
  }

  socket_t *socket = socket_new_from_fd(pair[1]);
  if (!socket) {
    LOG_ERROR(LOG_TAG, "%s unable to allocate socket from file descriptor %d.", __func__, pair[1]);
    goto error;
  }

  *sock_fd = pair[0];           // Transfer ownership of one end to caller.
  sco_socket->socket = socket;  // Hang on to the other end.
  list_append(sco_sockets, sco_socket);

  socket_register(socket, thread_get_reactor(thread), sco_socket, socket_read_ready_cb, NULL);
  return sco_socket;

error:;
  if (pair[0] != INVALID_FD)
    close(pair[0]);
  if (pair[1] != INVALID_FD)
    close(pair[1]);

  sco_socket_free_locked(sco_socket);
  return NULL;
}

static sco_socket_t *sco_socket_new(void) {
  sco_socket_t *sco_socket = (sco_socket_t *)osi_calloc(sizeof(sco_socket_t));
  sco_socket->sco_handle = BTM_INVALID_SCO_INDEX;
  return sco_socket;
}

// Must be called with |lock| held except during teardown when we know the socket thread
// is no longer alive.
static void sco_socket_free_locked(sco_socket_t *sco_socket) {
  if (!sco_socket)
    return;

  if (sco_socket->sco_handle != BTM_INVALID_SCO_INDEX)
    BTM_RemoveSco(sco_socket->sco_handle);
  socket_free(sco_socket->socket);
  osi_free(sco_socket);
}

// Must be called with |lock| held.
static sco_socket_t *sco_socket_find_locked(uint16_t sco_handle) {
  for (const list_node_t *node = list_begin(sco_sockets); node != list_end(sco_sockets); node = list_next(node)) {
    sco_socket_t *sco_socket = (sco_socket_t *)list_node(node);
    if (sco_socket->sco_handle == sco_handle)
      return sco_socket;
  }
  return NULL;
}

static void connection_request_cb(tBTM_ESCO_EVT event, tBTM_ESCO_EVT_DATA *data) {
  assert(data != NULL);

  // Don't care about change of link parameters, only connection requests.
  if (event != BTM_ESCO_CONN_REQ_EVT)
    return;

  pthread_mutex_lock(&lock);

  const tBTM_ESCO_CONN_REQ_EVT_DATA *conn_data = &data->conn_evt;
  sco_socket_t *sco_socket = sco_socket_find_locked(conn_data->sco_inx);
  int client_fd = INVALID_FD;

  if (!sco_socket) {
    LOG_ERROR(LOG_TAG, "%s unable to find sco_socket for handle: %hu", __func__, conn_data->sco_inx);
    goto error;
  }

  if (sco_socket != listen_sco_socket) {
    LOG_ERROR(LOG_TAG, "%s received connection request on non-listening socket handle: %hu", __func__, conn_data->sco_inx);
    goto error;
  }

  sco_socket_t *new_sco_socket = sco_socket_establish_locked(true, NULL, &client_fd);
  if (!new_sco_socket) {
    LOG_ERROR(LOG_TAG, "%s unable to allocate new sco_socket.", __func__);
    goto error;
  }

  // Swap socket->sco_handle and new_socket->sco_handle
  uint16_t temp = sco_socket->sco_handle;
  sco_socket->sco_handle = new_sco_socket->sco_handle;
  new_sco_socket->sco_handle = temp;

  sock_connect_signal_t connect_signal;
  connect_signal.size = sizeof(connect_signal);
  memcpy(&connect_signal.bd_addr, conn_data->bd_addr, sizeof(bt_bdaddr_t));
  connect_signal.channel = 0;
  connect_signal.status = 0;

  if (socket_write_and_transfer_fd(sco_socket->socket, &connect_signal, sizeof(connect_signal), client_fd) != sizeof(connect_signal)) {
    LOG_ERROR(LOG_TAG, "%s unable to send new file descriptor to listening socket.", __func__);
    goto error;
  }

  BTM_RegForEScoEvts(listen_sco_socket->sco_handle, connection_request_cb);
  BTM_EScoConnRsp(conn_data->sco_inx, HCI_SUCCESS, NULL);

  pthread_mutex_unlock(&lock);
  return;

error:;
  pthread_mutex_unlock(&lock);

  if (client_fd != INVALID_FD)
    close(client_fd);
  BTM_EScoConnRsp(conn_data->sco_inx, HCI_ERR_HOST_REJECT_RESOURCES, NULL);
}

static void connect_completed_cb(uint16_t sco_handle) {
  pthread_mutex_lock(&lock);

  sco_socket_t *sco_socket = sco_socket_find_locked(sco_handle);
  if (!sco_socket) {
    LOG_ERROR(LOG_TAG, "%s SCO socket not found on connect for handle: %hu", __func__, sco_handle);
    goto out;
  }

  // If sco_socket->socket was closed, we should tear down because there is no app-level
  // interest in the SCO socket.
  if (!sco_socket->socket) {
    BTM_RemoveSco(sco_socket->sco_handle);
    list_remove(sco_sockets, sco_socket);
    goto out;
  }

  sco_socket->connect_completed = true;

out:;
  pthread_mutex_unlock(&lock);
}

static void disconnect_completed_cb(uint16_t sco_handle) {
  pthread_mutex_lock(&lock);

  sco_socket_t *sco_socket = sco_socket_find_locked(sco_handle);
  if (!sco_socket) {
    LOG_ERROR(LOG_TAG, "%s SCO socket not found on disconnect for handle: %hu", __func__, sco_handle);
    goto out;
  }

  list_remove(sco_sockets, sco_socket);

out:;
  pthread_mutex_unlock(&lock);
}

static void socket_read_ready_cb(UNUSED_ATTR socket_t *socket, void *context) {
  pthread_mutex_lock(&lock);

  sco_socket_t *sco_socket = (sco_socket_t *)context;
  socket_free(sco_socket->socket);
  sco_socket->socket = NULL;

  // Defer the underlying disconnect until the connection completes
  // since the BTM code doesn't behave correctly when a disconnect
  // request is issued while a connect is in progress. The fact that
  // sco_socket->socket == NULL indicates to the connect callback
  // routine that the socket is no longer desired and should be torn
  // down.
  if (sco_socket->connect_completed || sco_socket == listen_sco_socket) {
    if (BTM_RemoveSco(sco_socket->sco_handle) == BTM_SUCCESS)
      list_remove(sco_sockets, sco_socket);
    if (sco_socket == listen_sco_socket)
      listen_sco_socket = NULL;
  }

  pthread_mutex_unlock(&lock);
}
