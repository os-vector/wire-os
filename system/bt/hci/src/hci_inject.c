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

#define LOG_TAG "bt_hci_inject"

#include "hci_inject.h"

#include <assert.h>
#include <errno.h>
#include <string.h>

#include "bt_types.h"
#include "buffer_allocator.h"
#include "hci_layer.h"
#include "osi/include/allocator.h"
#include "osi/include/list.h"
#include "osi/include/log.h"
#include "osi/include/osi.h"
#include "osi/include/socket.h"
#include "osi/include/thread.h"

typedef enum {
  HCI_PACKET_COMMAND  = 1,
  HCI_PACKET_ACL_DATA = 2,
  HCI_PACKET_SCO_DATA = 3,
  HCI_PACKET_EVENT    = 4,
} hci_packet_t;

typedef struct {
  socket_t *socket;
  uint8_t buffer[65536 + 3];  // 2 bytes length prefix, 1 byte type prefix.
  size_t buffer_size;
} client_t;

static const port_t LISTEN_PORT = 8873;

static const hci_inject_t interface;
static const hci_t *hci;
static const allocator_t *buffer_allocator;
static socket_t *listen_socket;
static thread_t *thread;
static list_t *clients;

static int hci_packet_to_event(hci_packet_t packet);
static void accept_ready(socket_t *socket, void *context);
static void read_ready(socket_t *socket, void *context);
static void client_free(void *ptr);

bool hci_inject_open(const hci_t *hci_interface) {
#if (!defined(BT_NET_DEBUG) || (BT_NET_DEBUG != TRUE))
  return true;          // Disable using network sockets for security reasons
#endif

  assert(listen_socket == NULL);
  assert(thread == NULL);
  assert(clients == NULL);
  assert(hci_interface != NULL);

  hci = hci_interface;

  thread = thread_new("hci_inject");
  if (!thread)
    goto error;

  clients = list_new(client_free);
  if (!clients)
    goto error;

  listen_socket = socket_new();
  if (!listen_socket)
    goto error;

  if (!socket_listen(listen_socket, LISTEN_PORT))
    goto error;

  socket_register(listen_socket, thread_get_reactor(thread), NULL, accept_ready, NULL);
  return true;

error:;
  interface.close();
  return false;
}

void hci_inject_close(void) {
#if (!defined(BT_NET_DEBUG) || (BT_NET_DEBUG != TRUE))
  return;               // Disable using network sockets for security reasons
#endif

  socket_free(listen_socket);
  list_free(clients);
  thread_free(thread);

  listen_socket = NULL;
  thread = NULL;
  clients = NULL;
}

static int hci_packet_to_event(hci_packet_t packet) {
  switch (packet) {
    case HCI_PACKET_COMMAND:
      return MSG_STACK_TO_HC_HCI_CMD;
    case HCI_PACKET_ACL_DATA:
      return MSG_STACK_TO_HC_HCI_ACL;
    case HCI_PACKET_SCO_DATA:
      return MSG_STACK_TO_HC_HCI_SCO;
    default:
      LOG_ERROR(LOG_TAG, "%s unsupported packet type: %d", __func__, packet);
      return -1;
  }
}

static void accept_ready(socket_t *socket, UNUSED_ATTR void *context) {
  assert(socket != NULL);
  assert(socket == listen_socket);

  socket = socket_accept(socket);
  if (!socket)
    return;

  client_t *client = (client_t *)osi_calloc(sizeof(client_t));

  client->socket = socket;

  if (!list_append(clients, client)) {
    LOG_ERROR(LOG_TAG, "%s unable to add client to list.", __func__);
    client_free(client);
    return;
  }

  socket_register(socket, thread_get_reactor(thread), client, read_ready, NULL);
}

static void read_ready(UNUSED_ATTR socket_t *socket, void *context) {
  assert(socket != NULL);
  assert(context != NULL);

  client_t *client = (client_t *)context;

  ssize_t ret = socket_read(client->socket, client->buffer + client->buffer_size, sizeof(client->buffer) - client->buffer_size);
  if (ret == 0 || (ret == -1 && ret != EWOULDBLOCK && ret != EAGAIN)) {
    list_remove(clients, client);
    return;
  }
  client->buffer_size += ret;

  while (client->buffer_size > 3) {
    uint8_t *buffer = client->buffer;
    hci_packet_t packet_type = (hci_packet_t)buffer[0];
    size_t packet_len = (buffer[2] << 8) | buffer[1];
    size_t frame_len = 3 + packet_len;

    if (client->buffer_size < frame_len)
      break;

    // TODO(sharvil): validate incoming HCI messages.
    // TODO(sharvil): once we have an HCI parser, we can eliminate
    //   the 2-byte size field since it will be contained in the packet.

    BT_HDR *buf = (BT_HDR *)buffer_allocator->alloc(BT_HDR_SIZE + packet_len);
    if (buf) {
      buf->event = hci_packet_to_event(packet_type);
      buf->offset = 0;
      buf->layer_specific = 0;
      buf->len = packet_len;
      memcpy(buf->data, buffer + 3, packet_len);
      hci->transmit_downward(buf->event, buf);
    } else {
      LOG_ERROR(LOG_TAG, "%s dropping injected packet of length %zu", __func__, packet_len);
    }

    size_t remainder = client->buffer_size - frame_len;
    memmove(buffer, buffer + frame_len, remainder);
    client->buffer_size -= frame_len;
  }
}

static void client_free(void *ptr) {
  if (!ptr)
    return;

  client_t *client = (client_t *)ptr;
  socket_free(client->socket);
  osi_free(client);
}

static const hci_inject_t interface = {
  hci_inject_open,
  hci_inject_close
};

const hci_inject_t *hci_inject_get_interface() {
  buffer_allocator = buffer_allocator_get_interface();
  return &interface;
}
