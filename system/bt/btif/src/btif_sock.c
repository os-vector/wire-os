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

#define LOG_TAG "bt_btif_sock"

#include <assert.h>

#include <hardware/bluetooth.h>
#include <hardware/bt_sock.h>

#include "bta_api.h"
#include "btif_common.h"
#include "btif_sock_l2cap.h"
#include "btif_sock_rfc.h"
#include "btif_sock_sco.h"
#include "btif_sock_sdp.h"
#include "btif_sock_thread.h"
#include "btif_uid.h"
#include "btif_util.h"
#include "osi/include/thread.h"

static bt_status_t btsock_listen(btsock_type_t type, const char *service_name, const uint8_t *uuid, int channel, int *sock_fd, int flags, int app_uid);
static bt_status_t btsock_connect(const bt_bdaddr_t *bd_addr, btsock_type_t type, const uint8_t *uuid, int channel, int *sock_fd, int flags, int app_uid);

static bt_status_t btsock_get_sockopt(btsock_type_t type, int channel, btsock_option_type_t option_name,
                                            void *option_value, int *option_len);
static bt_status_t btsock_set_sockopt(btsock_type_t type, int channel, btsock_option_type_t option_name,
                                            void *option_value, int option_len);
static void btsock_signaled(int fd, int type, int flags, uint32_t user_id);

static int thread_handle = -1;
static thread_t *thread;

btsock_interface_t *btif_sock_get_interface(void) {
  static btsock_interface_t interface = {
    sizeof(interface),
    btsock_listen,
    btsock_connect,
    btsock_get_sockopt,
    btsock_set_sockopt
  };

  return &interface;
}

bt_status_t btif_sock_init(uid_set_t* uid_set) {
  assert(thread_handle == -1);
  assert(thread == NULL);

  btsock_thread_init();
  thread_handle = btsock_thread_create(btsock_signaled, NULL);
  if (thread_handle == -1) {
    LOG_ERROR(LOG_TAG, "%s unable to create btsock_thread.", __func__);
    goto error;
  }

  bt_status_t status = btsock_rfc_init(thread_handle, uid_set);
  if (status != BT_STATUS_SUCCESS) {
    LOG_ERROR(LOG_TAG, "%s error initializing RFCOMM sockets: %d", __func__, status);
    goto error;
  }

  status = btsock_l2cap_init(thread_handle, uid_set);
  if (status != BT_STATUS_SUCCESS) {
    LOG_ERROR(LOG_TAG, "%s error initializing L2CAP sockets: %d", __func__, status);
    goto error;
  }

  thread = thread_new("btif_sock");
  if (!thread) {
    LOG_ERROR(LOG_TAG, "%s error creating new thread.", __func__);
    btsock_rfc_cleanup();
    goto error;
  }

#if (defined BTM_SCO_INCLUDED && BTM_SCO_INCLUDED == TRUE)
  status = btsock_sco_init(thread);
  if (status != BT_STATUS_SUCCESS) {
    LOG_ERROR(LOG_TAG, "%s error initializing SCO sockets: %d", __func__, status);
    btsock_rfc_cleanup();
    goto error;
  }
#endif

  return BT_STATUS_SUCCESS;

error:;
  thread_free(thread);
  thread = NULL;
  if (thread_handle != -1)
    btsock_thread_exit(thread_handle);
  thread_handle = -1;
  uid_set = NULL;
  return BT_STATUS_FAIL;
}

void btif_sock_cleanup(void) {
  if (thread_handle == -1)
    return;

  thread_stop(thread);
  thread_join(thread);
  btsock_thread_exit(thread_handle);
  btsock_rfc_cleanup();
#if (defined BTM_SCO_INCLUDED && BTM_SCO_INCLUDED == TRUE)
  btsock_sco_cleanup();
#endif
  btsock_l2cap_cleanup();
  thread_free(thread);
  thread_handle = -1;
  thread = NULL;
}

static bt_status_t btsock_listen(btsock_type_t type, const char *service_name, const uint8_t *service_uuid, int channel, int *sock_fd, int flags, int app_uid) {
  if((flags & BTSOCK_FLAG_NO_SDP) == 0) {
      assert(service_uuid != NULL || channel > 0);
      assert(sock_fd != NULL);
  }

  *sock_fd = INVALID_FD;
  bt_status_t status = BT_STATUS_FAIL;

  switch (type) {
    case BTSOCK_RFCOMM:
      status = btsock_rfc_listen(service_name, service_uuid, channel, sock_fd, flags, app_uid);
      break;
    case BTSOCK_L2CAP:
      status = btsock_l2cap_listen(service_name, channel, sock_fd, flags, app_uid);
      break;
#if (defined BTM_SCO_INCLUDED && BTM_SCO_INCLUDED == TRUE)
    case BTSOCK_SCO:
      status = btsock_sco_listen(sock_fd, flags);
      break;
#endif
    default:
      LOG_ERROR(LOG_TAG, "%s unknown/unsupported socket type: %d", __func__, type);
      status = BT_STATUS_UNSUPPORTED;
      break;
  }
  return status;
}

static bt_status_t btsock_connect(const bt_bdaddr_t *bd_addr, btsock_type_t type, const uint8_t *uuid, int channel, int *sock_fd, int flags, int app_uid) {
  assert(uuid != NULL || channel > 0);
  assert(bd_addr != NULL);
  assert(sock_fd != NULL);

  *sock_fd = INVALID_FD;
  bt_status_t status = BT_STATUS_FAIL;

  switch (type) {
    case BTSOCK_RFCOMM:
      status = btsock_rfc_connect(bd_addr, uuid, channel, sock_fd, flags, app_uid);
      break;

    case BTSOCK_L2CAP:
      status = btsock_l2cap_connect(bd_addr, channel, sock_fd, flags, app_uid);
      break;

#if (defined BTM_SCO_INCLUDED && BTM_SCO_INCLUDED == TRUE)
    case BTSOCK_SCO:
      status = btsock_sco_connect(bd_addr, sock_fd, flags);
      break;
#endif

    default:
      LOG_ERROR(LOG_TAG, "%s unknown/unsupported socket type: %d", __func__, type);
      status = BT_STATUS_UNSUPPORTED;
      break;
  }
  return status;
}

static bt_status_t btsock_get_sockopt(btsock_type_t type, int channel, btsock_option_type_t option_name,
                                            void *option_value, int *option_len)
{
    if((channel <= 0) || (option_value == NULL) || (option_len == NULL))
    {
        BTIF_TRACE_ERROR("invalid parameters, channel:%d, option_value:%p, option_len:%p", channel,
                                                                        option_value, option_len);
        return BT_STATUS_PARM_INVALID;
    }

    bt_status_t status = BT_STATUS_FAIL;
    switch(type)
    {
        case BTSOCK_RFCOMM:
            status = btsock_rfc_get_sockopt(channel, option_name, option_value, option_len);
            break;
        case BTSOCK_L2CAP:
            BTIF_TRACE_ERROR("bt l2cap socket type not supported, type:%d", type);
            status = BT_STATUS_UNSUPPORTED;
            break;
#if (defined BTM_SCO_INCLUDED && BTM_SCO_INCLUDED == TRUE)
        case BTSOCK_SCO:
            BTIF_TRACE_ERROR("bt sco socket not supported, type:%d", type);
            status = BT_STATUS_UNSUPPORTED;
            break;
#endif
        default:
            BTIF_TRACE_ERROR("unknown bt socket type:%d", type);
            status = BT_STATUS_UNSUPPORTED;
            break;
    }
    return status;
}

static bt_status_t btsock_set_sockopt(btsock_type_t type, int channel, btsock_option_type_t option_name,
                                            void *option_value, int option_len)
{
    if((channel <= 0) || (option_value == NULL))
    {
        BTIF_TRACE_ERROR("invalid parameters, channel:%d, option_value:%p", channel, option_value);
        return BT_STATUS_PARM_INVALID;
    }

    bt_status_t status = BT_STATUS_FAIL;
    switch(type)
    {
        case BTSOCK_RFCOMM:
            status = btsock_rfc_set_sockopt(channel, option_name, option_value, option_len);
            break;
        case BTSOCK_L2CAP:
            BTIF_TRACE_ERROR("bt l2cap socket type not supported, type:%d", type);
            status = BT_STATUS_UNSUPPORTED;
            break;
#if (defined BTM_SCO_INCLUDED && BTM_SCO_INCLUDED == TRUE)
        case BTSOCK_SCO:
            BTIF_TRACE_ERROR("bt sco socket not supported, type:%d", type);
            status = BT_STATUS_UNSUPPORTED;
            break;
#endif
        default:
            BTIF_TRACE_ERROR("unknown bt socket type:%d", type);
            status = BT_STATUS_UNSUPPORTED;
            break;
    }
    return status;
}

static void btsock_signaled(int fd, int type, int flags, uint32_t user_id) {
  switch (type) {
    case BTSOCK_RFCOMM:
      btsock_rfc_signaled(fd, flags, user_id);
      break;
    case BTSOCK_L2CAP:
      btsock_l2cap_signaled(fd, flags, user_id);
      break;
    default:
      assert(false && "Invalid socket type");
      break;
  }
}
