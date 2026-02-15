/******************************************************************************
 *
 *  Copyright (C) 2009-2012 Broadcom Corporation
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

#define LOG_TAG "bt_btif_sock_rfcomm"

#include <assert.h>
#include <errno.h>
#include <features.h>
#include <pthread.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <hardware/bluetooth.h>
#include <hardware/bt_sock.h>
#include <stdlib.h>

#include "bt_target.h"
#include "bta_api.h"
#include "bta_jv_api.h"
#include "bta_jv_co.h"
#include "btif_common.h"
#include "btif_sock_sdp.h"
#include "btif_sock_thread.h"
#include "btif_sock_util.h"
#include "btif_uid.h"
#include "btif_util.h"
#include "btm_api.h"
#include "btm_int.h"
#include "btu.h"
#include "bt_common.h"
#include "hcimsgs.h"
#include "osi/include/compat.h"
#include "osi/include/list.h"
#include "osi/include/log.h"
#include "osi/include/osi.h"
#include "port_api.h"
#include "sdp_api.h"

/* The JV interface can have only one user, hence we need to call a few
 * L2CAP functions from this file. */
#include "btif_sock_l2cap.h"

#define MODEM_SIGNAL_DTRDSR        0x01
#define MODEM_SIGNAL_RTSCTS        0x02
#define MODEM_SIGNAL_RI            0x04
#define MODEM_SIGNAL_DCD           0x08
#define MAX_RFC_CHANNEL 30  // Maximum number of RFCOMM channels (1-30 inclusive).
#define MAX_RFC_SESSION 7   // Maximum number of devices we can have an RFCOMM connection with.

typedef struct {
  int outgoing_congest : 1;
  int pending_sdp_request : 1;
  int doing_sdp_request : 1;
  int server : 1;
  int connected : 1;
  int closing : 1;
} flags_t;

typedef struct {
  flags_t f;
  uint32_t id;  // Non-zero indicates a valid (in-use) slot.
  int security;
  int scn;      // Server channel number
  int scn_notified;
  bt_bdaddr_t addr;
  int is_service_uuid_valid;
  uint8_t service_uuid[16];
  char service_name[256];
  int fd;
  int app_fd;  // Temporary storage for the half of the socketpair that's sent back to upper layers.
  int app_uid; // UID of the app for which this socket was created.
  int mtu;
  uint8_t *packet;
  int sdp_handle;
  int rfc_handle;
  int rfc_port_handle;
  int role;
  list_t *incoming_queue;
} rfc_slot_t;

static rfc_slot_t rfc_slots[MAX_RFC_CHANNEL];
static uint32_t rfc_slot_id;
static volatile int pth = -1; // poll thread handle
static pthread_mutex_t slot_lock = PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;
static uid_set_t* uid_set = NULL;

static rfc_slot_t *find_free_slot(void);
static void cleanup_rfc_slot(rfc_slot_t *rs);
static void jv_dm_cback(tBTA_JV_EVT event, tBTA_JV *p_data, void *user_data);
static void *rfcomm_cback(tBTA_JV_EVT event, tBTA_JV *p_data, void *user_data);
static bool send_app_scn(rfc_slot_t *rs);

static bool is_init_done(void) {
  return pth != -1;
}

bt_status_t btsock_rfc_init(int poll_thread_handle, uid_set_t* set) {
  pth = poll_thread_handle;
  uid_set = set;

  memset(rfc_slots, 0, sizeof(rfc_slots));
  for (size_t i = 0; i < ARRAY_SIZE(rfc_slots); ++i) {
    rfc_slots[i].scn = -1;
    rfc_slots[i].sdp_handle = 0;
    rfc_slots[i].fd = INVALID_FD;
    rfc_slots[i].app_fd = INVALID_FD;
    rfc_slots[i].incoming_queue = list_new(osi_free);
    assert(rfc_slots[i].incoming_queue != NULL);
  }

  BTA_JvEnable(jv_dm_cback);

  return BT_STATUS_SUCCESS;
}

void btsock_rfc_cleanup(void) {
  pth = -1;

  pthread_mutex_lock(&slot_lock);
  for (size_t i = 0; i < ARRAY_SIZE(rfc_slots); ++i) {
    if (rfc_slots[i].id)
      cleanup_rfc_slot(&rfc_slots[i]);
    list_free(rfc_slots[i].incoming_queue);
    rfc_slots[i].incoming_queue = NULL;
  }
  pthread_mutex_unlock(&slot_lock);
}

static rfc_slot_t *find_free_slot(void) {
  for (size_t i = 0; i < ARRAY_SIZE(rfc_slots); ++i)
    if (rfc_slots[i].fd == INVALID_FD)
      return &rfc_slots[i];
  return NULL;
}

static rfc_slot_t *find_rfc_slot_by_id(uint32_t id) {
  assert(id != 0);

  for (size_t i = 0; i < ARRAY_SIZE(rfc_slots); ++i)
    if (rfc_slots[i].id == id)
      return &rfc_slots[i];

  LOG_ERROR(LOG_TAG, "%s unable to find RFCOMM slot id: %d", __func__, id);
  return NULL;
}

static rfc_slot_t *find_rfc_slot_by_pending_sdp(void) {
  uint32_t min_id = UINT32_MAX;
  int slot = -1;
  for (size_t i = 0; i < ARRAY_SIZE(rfc_slots); ++i)
    if (rfc_slots[i].id && rfc_slots[i].f.pending_sdp_request && rfc_slots[i].id < min_id) {
      min_id = rfc_slots[i].id;
      slot = i;
    }

  return (slot == -1) ? NULL : &rfc_slots[slot];
}

static rfc_slot_t* find_rfc_slot_by_scn(int scn)
{
    int i;
    if(scn > 0)
    {
        /* traverse it from the last entry, as incase of
         * server two entries will exist with the same scn
         * and the later entry is valid
         */
        for(i = MAX_RFC_CHANNEL-1; i >= 0; i--)
        {
            if(rfc_slots[i].scn == scn)
            {
                if(rfc_slots[i].id)
                    return &rfc_slots[i];
            }
        }
    }
    return NULL;
}

static bool is_requesting_sdp(void) {
  for (size_t i = 0; i < ARRAY_SIZE(rfc_slots); ++i)
    if (rfc_slots[i].id && rfc_slots[i].f.doing_sdp_request)
      return true;
  return false;
}

static rfc_slot_t *alloc_rfc_slot(const bt_bdaddr_t *addr, const char *name, const uint8_t *uuid, int channel, int flags, bool server) {
  int security = 0;
  if(flags & BTSOCK_FLAG_ENCRYPT)
      security |= server ? BTM_SEC_IN_ENCRYPT : BTM_SEC_OUT_ENCRYPT;
  if(flags & BTSOCK_FLAG_AUTH)
      security |= server ? BTM_SEC_IN_AUTHENTICATE : BTM_SEC_OUT_AUTHENTICATE;
  if(flags & BTSOCK_FLAG_AUTH_MITM)
      security |= server ? BTM_SEC_IN_MITM : BTM_SEC_OUT_MITM;
  if(flags & BTSOCK_FLAG_AUTH_16_DIGIT)
      security |= BTM_SEC_IN_MIN_16_DIGIT_PIN;

  rfc_slot_t *slot = find_free_slot();
  if (!slot) {
    LOG_ERROR(LOG_TAG, "%s unable to find free RFCOMM slot.", __func__);
    return NULL;
  }

  int fds[2] = { INVALID_FD, INVALID_FD };
  if (socketpair(AF_LOCAL, SOCK_STREAM, 0, fds) == -1) {
    LOG_ERROR(LOG_TAG, "%s error creating socketpair: %s", __func__, strerror(errno));
    return NULL;
  }

  // Increment slot id and make sure we don't use id=0.
  if (++rfc_slot_id == 0)
    rfc_slot_id = 1;

  slot->fd = fds[0];
  slot->app_fd = fds[1];
  slot->security = security;
  slot->scn = channel;
  slot->app_uid = -1;

  if(!is_uuid_empty(uuid)) {
    memcpy(slot->service_uuid, uuid, sizeof(slot->service_uuid));
    slot->is_service_uuid_valid = true;
  } else {
    memset(slot->service_uuid, 0, sizeof(slot->service_uuid));
    slot->is_service_uuid_valid = false;
  }
  if(name && *name) {
    strlcpy(slot->service_name, name, sizeof(slot->service_name));
  } else {
    memset(slot->service_name, 0, sizeof(slot->service_name));
  }
  if (addr)
    slot->addr = *addr;

  slot->id = rfc_slot_id;
  slot->f.server = server;

  return slot;
}

static rfc_slot_t *create_srv_accept_rfc_slot(rfc_slot_t *srv_rs, const bt_bdaddr_t *addr, int open_handle, int new_listen_handle) {
  rfc_slot_t *accept_rs = alloc_rfc_slot(addr, srv_rs->service_name, srv_rs->service_uuid, srv_rs->scn, 0, false);
  if (!accept_rs) {
    LOG_ERROR(LOG_TAG, "%s unable to allocate RFCOMM slot.", __func__);
    return NULL;
  }

  accept_rs->f.server = false;
  accept_rs->f.connected = true;
  accept_rs->security = srv_rs->security;
  accept_rs->mtu = srv_rs->mtu;
  accept_rs->role = srv_rs->role;
  accept_rs->rfc_handle = open_handle;
  accept_rs->rfc_port_handle = BTA_JvRfcommGetPortHdl(open_handle);
  accept_rs->app_uid = srv_rs->app_uid;

  srv_rs->rfc_handle = new_listen_handle;
  srv_rs->rfc_port_handle = BTA_JvRfcommGetPortHdl(new_listen_handle);

  assert(accept_rs->rfc_port_handle != srv_rs->rfc_port_handle);

  // now swap the slot id
  uint32_t new_listen_id = accept_rs->id;
  accept_rs->id = srv_rs->id;
  srv_rs->id = new_listen_id;

  return accept_rs;
}

bt_status_t btsock_rfc_listen(const char *service_name, const uint8_t *service_uuid, int channel, int *sock_fd, int flags, int app_uid) {
  assert(sock_fd != NULL);
  assert((service_uuid != NULL)
    || (channel >= 1 && channel <= MAX_RFC_CHANNEL)
    || ((flags & BTSOCK_FLAG_NO_SDP) != 0));

  *sock_fd = INVALID_FD;

  // TODO(sharvil): not sure that this check makes sense; seems like a logic error to call
  // functions on RFCOMM sockets before initializing the module. Probably should be an assert.
  if (!is_init_done())
    return BT_STATUS_NOT_READY;

  if((flags & BTSOCK_FLAG_NO_SDP) == 0) {
    if(is_uuid_empty(service_uuid)) {
      APPL_TRACE_DEBUG("BTA_JvGetChannelId: service_uuid not set AND "
              "BTSOCK_FLAG_NO_SDP is not set - changing to SPP");
      service_uuid = UUID_SPP;  // Use serial port profile to listen to specified channel
    } else {
      //Check the service_uuid. overwrite the channel # if reserved
      int reserved_channel = get_reserved_rfc_channel(service_uuid);
      if (reserved_channel > 0) {
            channel = reserved_channel;
      }
    }
  }

  int status = BT_STATUS_FAIL;
  pthread_mutex_lock(&slot_lock);

  rfc_slot_t *slot = alloc_rfc_slot(NULL, service_name, service_uuid, channel, flags, true);
  if (!slot) {
    LOG_ERROR(LOG_TAG, "%s unable to allocate RFCOMM slot.", __func__);
    goto out;
  }
  APPL_TRACE_DEBUG("BTA_JvGetChannelId: service_name: %s - channel: %d", service_name, channel);
  BTA_JvGetChannelId(BTA_JV_CONN_TYPE_RFCOMM, UINT_TO_PTR(slot->id), channel);
  *sock_fd = slot->app_fd;    // Transfer ownership of fd to caller.
  /*TODO:
   * We are leaking one of the app_fd's - either the listen socket, or the connection socket.
   * WE need to close this in native, as the FD might belong to another process
    - This is the server socket FD
    - For accepted connections, we close the FD after passing it to JAVA.
    - Try to simply remove the = -1 to free the FD at rs cleanup.*/
//        close(rs->app_fd);
  slot->app_fd = INVALID_FD;  // Drop our reference to the fd.
  slot->app_uid = app_uid;
  btsock_thread_add_fd(pth, slot->fd, BTSOCK_RFCOMM, SOCK_THREAD_FD_EXCEPTION, slot->id);

  status = BT_STATUS_SUCCESS;

out:;
  pthread_mutex_unlock(&slot_lock);
  return status;
}

bt_status_t btsock_rfc_connect(const bt_bdaddr_t *bd_addr, const uint8_t *service_uuid, int channel, int *sock_fd, int flags, int app_uid) {
  assert(sock_fd != NULL);
  assert(service_uuid != NULL || (channel >= 1 && channel <= MAX_RFC_CHANNEL));

  *sock_fd = INVALID_FD;

  // TODO(sharvil): not sure that this check makes sense; seems like a logic error to call
  // functions on RFCOMM sockets before initializing the module. Probably should be an assert.
  if (!is_init_done())
    return BT_STATUS_NOT_READY;

  int status = BT_STATUS_FAIL;
  pthread_mutex_lock(&slot_lock);

  rfc_slot_t *slot = alloc_rfc_slot(bd_addr, NULL, service_uuid, channel, flags, false);
  if (!slot) {
    LOG_ERROR(LOG_TAG, "%s unable to allocate RFCOMM slot.", __func__);
    goto out;
  }

  if (is_uuid_empty(service_uuid)) {
    tBTA_JV_STATUS ret = BTA_JvRfcommConnect(slot->security, slot->role, slot->scn, slot->addr.address, rfcomm_cback, (void *)(uintptr_t)slot->id);
    if (ret != BTA_JV_SUCCESS) {
      LOG_ERROR(LOG_TAG, "%s unable to initiate RFCOMM connection: %d", __func__, ret);
      cleanup_rfc_slot(slot);
      goto out;
    }

    if (!send_app_scn(slot)) {
      LOG_ERROR(LOG_TAG, "%s unable to send channel number.", __func__);
      cleanup_rfc_slot(slot);
      goto out;
    }
  } else {
    tSDP_UUID sdp_uuid;
    sdp_uuid.len = 16;
    memcpy(sdp_uuid.uu.uuid128, service_uuid, sizeof(sdp_uuid.uu.uuid128));

    if (!is_requesting_sdp()) {
      BTA_JvStartDiscovery((uint8_t *)bd_addr->address, 1, &sdp_uuid, (void *)(uintptr_t)slot->id);
      slot->f.pending_sdp_request = false;
      slot->f.doing_sdp_request = true;
    } else {
      slot->f.pending_sdp_request = true;
      slot->f.doing_sdp_request = false;
    }
  }

  *sock_fd = slot->app_fd;    // Transfer ownership of fd to caller.
  slot->app_fd = INVALID_FD;  // Drop our reference to the fd.
  slot->app_uid = app_uid;
  btsock_thread_add_fd(pth, slot->fd, BTSOCK_RFCOMM, SOCK_THREAD_FD_RD, slot->id);
  status = BT_STATUS_SUCCESS;

out:;
  pthread_mutex_unlock(&slot_lock);
  return status;
}

bt_status_t btsock_rfc_get_sockopt(int channel, btsock_option_type_t option_name,
                                            void *option_value, int *option_len)
{
    int status = BT_STATUS_FAIL;

    APPL_TRACE_DEBUG("btsock_rfc_get_sockopt channel is %d ", channel);
    if((channel < 1) || (channel > 30) || (option_value == NULL) || (option_len == NULL))
    {
        APPL_TRACE_ERROR("invalid rfc channel:%d or option_value:%p, option_len:%p",
                                             channel, option_value, option_len);
        return BT_STATUS_PARM_INVALID;
    }
    rfc_slot_t* rs = find_rfc_slot_by_scn(channel);
    if((rs) && ((option_name == BTSOCK_OPT_GET_MODEM_BITS)))
    {
        if(PORT_SUCCESS == PORT_GetModemStatus(rs->rfc_port_handle, (UINT8 *)option_value))
        {
            *option_len = sizeof(UINT8);
            status = BT_STATUS_SUCCESS;
        }
    }
    return status;
}

bt_status_t btsock_rfc_set_sockopt(int channel, btsock_option_type_t option_name,
                                            void *option_value, int option_len)
{
    int status = BT_STATUS_FAIL;

    APPL_TRACE_DEBUG("btsock_rfc_get_sockopt channel is %d ", channel);
    if((channel < 1) || (channel > 30) || (option_value == NULL) || (option_len <= 0)
                     || (option_len > (int)sizeof(UINT8)))
    {
        APPL_TRACE_ERROR("invalid rfc channel:%d or option_value:%p, option_len:%d",
                                        channel, option_value, option_len);
        return BT_STATUS_PARM_INVALID;
    }
    rfc_slot_t* rs = find_rfc_slot_by_scn(channel);
    if((rs) && ((option_name == BTSOCK_OPT_SET_MODEM_BITS)))
    {
        if((*((UINT8 *)option_value)) & MODEM_SIGNAL_DTRDSR)
        {
            if(PORT_SUCCESS != PORT_Control(rs->rfc_port_handle, PORT_SET_DTRDSR))
                return status;
        }
        if((*((UINT8 *)option_value)) & MODEM_SIGNAL_RTSCTS)
        {
            if(PORT_SUCCESS != PORT_Control(rs->rfc_port_handle, PORT_SET_CTSRTS))
                return status;
        }
        if((*((UINT8 *)option_value)) & MODEM_SIGNAL_RI)
        {
            if(PORT_SUCCESS != PORT_Control(rs->rfc_port_handle, PORT_SET_RI))
                return status;
        }
        if((*((UINT8 *)option_value)) & MODEM_SIGNAL_DCD)
        {
            if(PORT_SUCCESS != PORT_Control(rs->rfc_port_handle, PORT_SET_DCD))
                return status;
        }
        status = BT_STATUS_SUCCESS;
    }
    else if((rs) && ((option_name == BTSOCK_OPT_CLR_MODEM_BITS)))
    {
        if((*((UINT8 *)option_value)) & MODEM_SIGNAL_DTRDSR)
        {
            if(PORT_SUCCESS != PORT_Control(rs->rfc_port_handle, PORT_CLR_DTRDSR))
                return status;
        }
        if((*((UINT8 *)option_value)) & MODEM_SIGNAL_RTSCTS)
        {
            if(PORT_SUCCESS != PORT_Control(rs->rfc_port_handle, PORT_CLR_CTSRTS))
                return status;
        }
        if((*((UINT8 *)option_value)) & MODEM_SIGNAL_RI)
        {
            if(PORT_SUCCESS != PORT_Control(rs->rfc_port_handle, PORT_CLR_RI))
                return status;
        }
        if((*((UINT8 *)option_value)) & MODEM_SIGNAL_DCD)
        {
            if(PORT_SUCCESS != PORT_Control(rs->rfc_port_handle, PORT_CLR_DCD))
                return status;
        }
        status = BT_STATUS_SUCCESS;
    }

    return status;
}

static int create_server_sdp_record(rfc_slot_t *slot) {
    if(slot->scn == 0) {
        return false;
    }
  slot->sdp_handle = add_rfc_sdp_rec(slot->service_name, slot->service_uuid, slot->scn);
  return (slot->sdp_handle > 0);
}

static void free_rfc_slot_scn(rfc_slot_t *slot) {
  if (slot->scn <= 0)
    return;

  if(slot->f.server && !slot->f.closing && slot->rfc_handle) {
    BTA_JvRfcommStopServer(slot->rfc_handle, (void *)(uintptr_t)slot->id);
    slot->rfc_handle = 0;
  }

  if (slot->f.server)
    BTM_FreeSCN(slot->scn);
  slot->scn = 0;
}

static void cleanup_rfc_slot(rfc_slot_t *slot) {
  if (slot->fd != INVALID_FD) {
    shutdown(slot->fd, SHUT_RDWR);
    close(slot->fd);
    slot->fd = INVALID_FD;
  }

  if (slot->app_fd != INVALID_FD) {
    close(slot->app_fd);
    slot->app_fd = INVALID_FD;
  }

  if (slot->sdp_handle > 0) {
    del_rfc_sdp_rec(slot->sdp_handle);
    slot->sdp_handle = 0;
  }

  if (slot->rfc_handle && !slot->f.closing && !slot->f.server) {
    BTA_JvRfcommClose(slot->rfc_handle, (void *)(uintptr_t)slot->id);
    slot->rfc_handle = 0;
  }

  free_rfc_slot_scn(slot);
  list_clear(slot->incoming_queue);

  slot->rfc_port_handle = 0;
  memset(&slot->f, 0, sizeof(slot->f));
  slot->id = 0;
  slot->scn_notified = false;
}

static bool send_app_scn(rfc_slot_t *slot) {
  if(slot->scn_notified == true) {
    //already send, just return success.
    return true;
  }
  slot->scn_notified = true;
  return sock_send_all(slot->fd, (const uint8_t*)&slot->scn, sizeof(slot->scn)) == sizeof(slot->scn);
}

static bool send_app_connect_signal(int fd, const bt_bdaddr_t* addr, int channel, int status, int send_fd) {
  sock_connect_signal_t cs;
  cs.size = sizeof(cs);
  cs.bd_addr = *addr;
  cs.channel = channel;
  cs.status = status;
  cs.max_rx_packet_size = 0; // not used for RFCOMM
  cs.max_tx_packet_size = 0; // not used for RFCOMM
  if (send_fd == INVALID_FD)
    return sock_send_all(fd, (const uint8_t *)&cs, sizeof(cs)) == sizeof(cs);

  return sock_send_fd(fd, (const uint8_t *)&cs, sizeof(cs), send_fd) == sizeof(cs);
}

static void on_cl_rfc_init(tBTA_JV_RFCOMM_CL_INIT *p_init, uint32_t id) {
  pthread_mutex_lock(&slot_lock);

  rfc_slot_t *slot = find_rfc_slot_by_id(id);
  if (!slot)
    goto out;

  if (p_init->status == BTA_JV_SUCCESS)
    slot->rfc_handle = p_init->handle;
  else
    cleanup_rfc_slot(slot);

out:;
  pthread_mutex_unlock(&slot_lock);
}

static void on_srv_rfc_listen_started(tBTA_JV_RFCOMM_START *p_start, uint32_t id) {
  pthread_mutex_lock(&slot_lock);

  rfc_slot_t *slot = find_rfc_slot_by_id(id);
  if (!slot)
    goto out;

  if (p_start->status == BTA_JV_SUCCESS) {
    slot->rfc_handle = p_start->handle;
  } else
    cleanup_rfc_slot(slot);

out:;
  pthread_mutex_unlock(&slot_lock);
}

static uint32_t on_srv_rfc_connect(tBTA_JV_RFCOMM_SRV_OPEN *p_open, uint32_t id) {
  uint32_t new_listen_slot_id = 0;
  pthread_mutex_lock(&slot_lock);

  rfc_slot_t *srv_rs = find_rfc_slot_by_id(id);
  if (!srv_rs)
    goto out;

  rfc_slot_t *accept_rs = create_srv_accept_rfc_slot(srv_rs, (const bt_bdaddr_t *)p_open->rem_bda, p_open->handle, p_open->new_listen_handle);
  if (!accept_rs)
    goto out;

  // Start monitoring the socket.
  btsock_thread_add_fd(pth, srv_rs->fd, BTSOCK_RFCOMM, SOCK_THREAD_FD_EXCEPTION, srv_rs->id);
  btsock_thread_add_fd(pth, accept_rs->fd, BTSOCK_RFCOMM, SOCK_THREAD_FD_RD, accept_rs->id);
  send_app_connect_signal(srv_rs->fd, &accept_rs->addr, srv_rs->scn, 0, accept_rs->app_fd);
  accept_rs->app_fd = INVALID_FD;  // Ownership of the application fd has been transferred.
  new_listen_slot_id = srv_rs->id;

out:;
  pthread_mutex_unlock(&slot_lock);
  return new_listen_slot_id;
}

static void on_cli_rfc_connect(tBTA_JV_RFCOMM_OPEN *p_open, uint32_t id) {
  pthread_mutex_lock(&slot_lock);

  rfc_slot_t *slot = find_rfc_slot_by_id(id);
  if (!slot)
    goto out;

  if (p_open->status != BTA_JV_SUCCESS) {
    cleanup_rfc_slot(slot);
    goto out;
  }

  slot->rfc_port_handle = BTA_JvRfcommGetPortHdl(p_open->handle);
  memcpy(slot->addr.address, p_open->rem_bda, 6);

  if (send_app_connect_signal(slot->fd, &slot->addr, slot->scn, 0, -1))
    slot->f.connected = true;
  else
    LOG_ERROR(LOG_TAG, "%s unable to send connect completion signal to caller.", __func__);

out:;
  pthread_mutex_unlock(&slot_lock);
}

static void on_rfc_close(UNUSED_ATTR tBTA_JV_RFCOMM_CLOSE *p_close, uint32_t id) {
  pthread_mutex_lock(&slot_lock);

  // rfc_handle already closed when receiving rfcomm close event from stack.
  rfc_slot_t *slot = find_rfc_slot_by_id(id);
  if (slot)
    cleanup_rfc_slot(slot);

  pthread_mutex_unlock(&slot_lock);
}

static void on_rfc_write_done(UNUSED_ATTR tBTA_JV_RFCOMM_WRITE *p, uint32_t id) {
  int app_uid = -1;
  pthread_mutex_lock(&slot_lock);

  rfc_slot_t *slot = find_rfc_slot_by_id(id);
  if (slot) {
    app_uid = slot->app_uid;
    if (!slot->f.outgoing_congest) {
      btsock_thread_add_fd(pth, slot->fd, BTSOCK_RFCOMM, SOCK_THREAD_FD_RD, slot->id);
    }
  }

  pthread_mutex_unlock(&slot_lock);

  uid_set_add_tx(uid_set, app_uid, p->len);
}

static void on_rfc_outgoing_congest(tBTA_JV_RFCOMM_CONG *p, uint32_t id) {
  pthread_mutex_lock(&slot_lock);

  rfc_slot_t *slot = find_rfc_slot_by_id(id);
  if (slot) {
    slot->f.outgoing_congest = p->cong ? 1 : 0;
    if (!slot->f.outgoing_congest)
      btsock_thread_add_fd(pth, slot->fd, BTSOCK_RFCOMM, SOCK_THREAD_FD_RD, slot->id);
  }

  pthread_mutex_unlock(&slot_lock);
}

static void *rfcomm_cback(tBTA_JV_EVT event, tBTA_JV *p_data, void *user_data) {
  void *new_user_data = NULL;

  switch (event) {
    case BTA_JV_RFCOMM_START_EVT:
      on_srv_rfc_listen_started(&p_data->rfc_start, (uintptr_t)user_data);
      break;

    case BTA_JV_RFCOMM_CL_INIT_EVT:
      on_cl_rfc_init(&p_data->rfc_cl_init, (uintptr_t)user_data);
      break;

    case BTA_JV_RFCOMM_OPEN_EVT:
      BTA_JvSetPmProfile(p_data->rfc_open.handle,BTA_JV_PM_ID_1,BTA_JV_CONN_OPEN);
      on_cli_rfc_connect(&p_data->rfc_open, (uintptr_t)user_data);
      break;

    case BTA_JV_RFCOMM_SRV_OPEN_EVT:
      BTA_JvSetPmProfile(p_data->rfc_srv_open.handle,BTA_JV_PM_ALL,BTA_JV_CONN_OPEN);
      new_user_data = (void *)(uintptr_t)on_srv_rfc_connect(&p_data->rfc_srv_open, (uintptr_t)user_data);
      break;

    case BTA_JV_RFCOMM_CLOSE_EVT:
      APPL_TRACE_DEBUG("BTA_JV_RFCOMM_CLOSE_EVT: user_data:%d", (uintptr_t)user_data);
      on_rfc_close(&p_data->rfc_close, (uintptr_t)user_data);
      break;

    case BTA_JV_RFCOMM_WRITE_EVT:
      on_rfc_write_done(&p_data->rfc_write, (uintptr_t)user_data);
      break;

    case BTA_JV_RFCOMM_CONG_EVT:
      on_rfc_outgoing_congest(&p_data->rfc_cong, (uintptr_t)user_data);
      break;

    case BTA_JV_RFCOMM_DATA_IND_EVT:
      // Unused.
      break;

    default:
      LOG_ERROR(LOG_TAG, "%s unhandled event %d, slot id: %zi", __func__, event, (uintptr_t)user_data);
      break;
  }
  return new_user_data;
}

static void jv_dm_cback(tBTA_JV_EVT event, tBTA_JV *p_data, void *user_data) {
  uint32_t id = PTR_TO_UINT(user_data);
  switch(event) {
    case BTA_JV_GET_SCN_EVT:
    {
      pthread_mutex_lock(&slot_lock);
      rfc_slot_t* rs = find_rfc_slot_by_id(id);
      int new_scn = p_data->scn;

      if(rs && (new_scn != 0))
      {
        rs->scn = new_scn;
        /* BTA_JvCreateRecordByUser will only create a record if a UUID is specified,
         * else it just allocate a RFC channel and start the RFCOMM thread - needed
         * for the java
         * layer to get a RFCOMM channel.
         * If uuid is null the create_sdp_record() will be called from Java when it
         * has received the RFCOMM and L2CAP channel numbers through the sockets.*/

        // Send channel ID to java layer
        if(!send_app_scn(rs)){
          //closed
          APPL_TRACE_DEBUG("send_app_scn() failed, close rs->id:%d", rs->id);
          cleanup_rfc_slot(rs);
        } else {
          if(rs->is_service_uuid_valid == true) {
            // We already have data for SDP record, create it (RFC-only profiles)
            BTA_JvCreateRecordByUser(UINT_TO_PTR(rs->id));
          } else {
            APPL_TRACE_DEBUG("is_service_uuid_valid==false - don't set SDP-record, "
                    "just start the RFCOMM server", rs->id);
            //now start the rfcomm server after sdp & channel # assigned
            BTA_JvRfcommStartServer(rs->security, rs->role, rs->scn, MAX_RFC_SESSION,
                    rfcomm_cback, UINT_TO_PTR(rs->id));
          }
        }
      } else if(rs) {
        APPL_TRACE_ERROR("jv_dm_cback: Error: allocate channel %d, slot found:%p", rs->scn, rs);
        cleanup_rfc_slot(rs);
      }
      pthread_mutex_unlock(&slot_lock);
      break;
    }
    case BTA_JV_GET_PSM_EVT:
    {
      APPL_TRACE_DEBUG("Received PSM: 0x%04x", p_data->psm);
      on_l2cap_psm_assigned(id, p_data->psm);
      break;
    }
    case BTA_JV_CREATE_RECORD_EVT: {
      pthread_mutex_lock(&slot_lock);

      rfc_slot_t *slot = find_rfc_slot_by_id(id);
      if (slot && create_server_sdp_record(slot)) {
        // Start the rfcomm server after sdp & channel # assigned.
        BTA_JvRfcommStartServer(slot->security, slot->role, slot->scn, MAX_RFC_SESSION, rfcomm_cback, (void *)(uintptr_t)slot->id);
      } else if(slot) {
        APPL_TRACE_ERROR("jv_dm_cback: cannot start server, slot found:%p", slot);
        cleanup_rfc_slot(slot);
      }

      pthread_mutex_unlock(&slot_lock);
      break;
    }

    case BTA_JV_DISCOVERY_COMP_EVT: {
      pthread_mutex_lock(&slot_lock);
      rfc_slot_t *slot = find_rfc_slot_by_id(id);
      if (p_data->disc_comp.status == BTA_JV_SUCCESS && p_data->disc_comp.scn) {
        if (slot && slot->f.doing_sdp_request) {
          // Establish the connection if we successfully looked up a channel number to connect to.
          if (BTA_JvRfcommConnect(slot->security, slot->role, p_data->disc_comp.scn, slot->addr.address, rfcomm_cback, (void *)(uintptr_t)slot->id) == BTA_JV_SUCCESS) {
            slot->scn = p_data->disc_comp.scn;
            slot->f.doing_sdp_request = false;
            if (!send_app_scn(slot))
              cleanup_rfc_slot(slot);
          } else {
            cleanup_rfc_slot(slot);
          }
        } else if (slot) {
          // TODO(sharvil): this is really a logic error and we should probably assert.
          LOG_ERROR(LOG_TAG, "%s SDP response returned but RFCOMM slot %d did not request SDP record.", __func__, id);
        }
      } else if (slot) {
        cleanup_rfc_slot(slot);
      }

      // Find the next slot that needs to perform an SDP request and service it.
      slot = find_rfc_slot_by_pending_sdp();
      if (slot) {
        tSDP_UUID sdp_uuid;
        sdp_uuid.len = 16;
        memcpy(sdp_uuid.uu.uuid128, slot->service_uuid, sizeof(sdp_uuid.uu.uuid128));
        BTA_JvStartDiscovery((uint8_t *)slot->addr.address, 1, &sdp_uuid, (void *)(uintptr_t)slot->id);
        slot->f.pending_sdp_request = false;
        slot->f.doing_sdp_request = true;
      }

      pthread_mutex_unlock(&slot_lock);
      break;
    }

    default:
      APPL_TRACE_DEBUG("unhandled event:%d, slot id:%d", event, id);
      break;
  }
}

typedef enum {
  SENT_FAILED,
  SENT_NONE,
  SENT_PARTIAL,
  SENT_ALL,
} sent_status_t;

static sent_status_t send_data_to_app(int fd, BT_HDR *p_buf) {
  if (p_buf->len == 0)
    return SENT_ALL;

  ssize_t sent;
  OSI_NO_INTR(sent = send(fd, p_buf->data + p_buf->offset, p_buf->len,
                          MSG_DONTWAIT));

  if (sent == -1) {
    if (errno == EAGAIN || errno == EWOULDBLOCK)
      return SENT_NONE;
    LOG_ERROR(LOG_TAG, "%s error writing RFCOMM data back to app: %s", __func__, strerror(errno));
    return SENT_FAILED;
  }

  if (sent == 0)
    return SENT_FAILED;

  if (sent == p_buf->len)
    return SENT_ALL;

  p_buf->offset += sent;
  p_buf->len -= sent;
  return SENT_PARTIAL;
}

static bool flush_incoming_que_on_wr_signal(rfc_slot_t *slot) {
  while (!list_is_empty(slot->incoming_queue)) {
    BT_HDR *p_buf = list_front(slot->incoming_queue);
    switch (send_data_to_app(slot->fd, p_buf)) {
      case SENT_NONE:
      case SENT_PARTIAL:
        //monitor the fd to get callback when app is ready to receive data
        btsock_thread_add_fd(pth, slot->fd, BTSOCK_RFCOMM, SOCK_THREAD_FD_WR, slot->id);
        return true;

      case SENT_ALL:
        list_remove(slot->incoming_queue, p_buf);
        break;

      case SENT_FAILED:
        list_remove(slot->incoming_queue, p_buf);
        return false;
    }
  }

  //app is ready to receive data, tell stack to start the data flow
  //fix me: need a jv flow control api to serialize the call in stack
  APPL_TRACE_DEBUG("enable data flow, rfc_handle:0x%x, rfc_port_handle:0x%x, user_id:%d",
      slot->rfc_handle, slot->rfc_port_handle, slot->id);
  extern int PORT_FlowControl_MaxCredit(uint16_t handle, bool enable);
  PORT_FlowControl_MaxCredit(slot->rfc_port_handle, true);
  return true;
}

void btsock_rfc_signaled(UNUSED_ATTR int fd, int flags, uint32_t user_id) {
  pthread_mutex_lock(&slot_lock);

  rfc_slot_t *slot = find_rfc_slot_by_id(user_id);
  if (!slot)
    goto out;

  bool need_close = false;

  // Data available from app, tell stack we have outgoing data.
  if (flags & SOCK_THREAD_FD_RD && !slot->f.server) {
    if (slot->f.connected) {
      // Make sure there's data pending in case the peer closed the socket.
      int size = 0;
      if (!(flags & SOCK_THREAD_FD_EXCEPTION) || (ioctl(slot->fd, FIONREAD, &size) == 0 && size)) {
        BTA_JvRfcommWrite(slot->rfc_handle, slot->id);
      }
    } else {
      LOG_ERROR(LOG_TAG, "%s socket signaled for read while disconnected, slot: %d, channel: %d", __func__, slot->id, slot->scn);
      need_close = true;
    }
  }

  if (flags & SOCK_THREAD_FD_WR) {
    // App is ready to receive more data, tell stack to enable data flow.
    if (!slot->f.connected || !flush_incoming_que_on_wr_signal(slot)) {
      LOG_ERROR(LOG_TAG, "%s socket signaled for write while disconnected (or write failure), slot: %d, channel: %d", __func__, slot->id, slot->scn);
      need_close = true;
    }
  }

  if (need_close || (flags & SOCK_THREAD_FD_EXCEPTION)) {
    // Clean up if there's no data pending.
    int size = 0;
    if (need_close || ioctl(slot->fd, FIONREAD, &size) != 0 || !size)
      cleanup_rfc_slot(slot);
  }

out:;
  pthread_mutex_unlock(&slot_lock);
}

int bta_co_rfc_data_incoming(void *user_data, BT_HDR *p_buf) {
  int app_uid = -1;
  uint64_t bytes_rx = 0;

  pthread_mutex_lock(&slot_lock);

  int ret = 0;
  uint32_t id = (uintptr_t)user_data;
  rfc_slot_t *slot = find_rfc_slot_by_id(id);
  if (!slot)
    goto out;

  app_uid = slot->app_uid;
  bytes_rx = p_buf->len;

  if (list_is_empty(slot->incoming_queue)) {
    switch (send_data_to_app(slot->fd, p_buf)) {
      case SENT_NONE:
      case SENT_PARTIAL:
        list_append(slot->incoming_queue, p_buf);
        btsock_thread_add_fd(pth, slot->fd, BTSOCK_RFCOMM, SOCK_THREAD_FD_WR, slot->id);
        break;

      case SENT_ALL:
        osi_free(p_buf);
        ret = 1;  // Enable data flow.
        break;

      case SENT_FAILED:
        osi_free(p_buf);
        cleanup_rfc_slot(slot);
        break;
    }
  } else {
    list_append(slot->incoming_queue, p_buf);
  }

out:;
  pthread_mutex_unlock(&slot_lock);

  uid_set_add_rx(uid_set, app_uid, bytes_rx);

  return ret;  // Return 0 to disable data flow.
}

int bta_co_rfc_data_outgoing_size(void *user_data, int *size) {
  pthread_mutex_lock(&slot_lock);

  uint32_t id = (uintptr_t)user_data;
  int ret = false;
  *size = 0;
  rfc_slot_t *slot = find_rfc_slot_by_id(id);
  if (!slot)
    goto out;

  if (ioctl(slot->fd, FIONREAD, size) == 0) {
    ret = true;
  } else {
    LOG_ERROR(LOG_TAG, "%s unable to determine bytes remaining to be read on fd %d: %s", __func__, slot->fd, strerror(errno));
    cleanup_rfc_slot(slot);
  }

out:;
  pthread_mutex_unlock(&slot_lock);
  return ret;
}

int bta_co_rfc_data_outgoing(void *user_data, uint8_t *buf, uint16_t size) {
  pthread_mutex_lock(&slot_lock);

  uint32_t id = (uintptr_t)user_data;
  int ret = false;
  rfc_slot_t *slot = find_rfc_slot_by_id(id);
  if (!slot)
    goto out;

  ssize_t received;
  OSI_NO_INTR(received = recv(slot->fd, buf, size, 0));

  if(received == size) {
    ret = true;
  } else {
    LOG_ERROR(LOG_TAG, "%s error receiving RFCOMM data from app: %s", __func__, strerror(errno));
    cleanup_rfc_slot(slot);
  }

out:;
  pthread_mutex_unlock(&slot_lock);
  return ret;
}
