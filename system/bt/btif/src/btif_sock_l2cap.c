/*
* Copyright (C) 2014 Samsung System LSI
* Copyright (C) 2013 The Android Open Source Project
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
*      http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*/

#define LOG_TAG "bt_btif_sock"

#include "btif_sock_l2cap.h"

#include <errno.h>
#include <pthread.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <hardware/bt_sock.h>

#include "osi/include/allocator.h"
#include "osi/include/log.h"

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
#include "l2c_api.h"
#include "l2cdefs.h"
#include "port_api.h"
#include "sdp_api.h"

#ifdef ANDROID
#include <cutils/log.h>
#endif
#define asrt(s) if (!(s)) APPL_TRACE_ERROR("## %s assert %s failed at line:%d ##",__FUNCTION__, \
        #s, __LINE__)

struct packet {
    struct packet *next, *prev;
    uint32_t len;
    uint8_t *data;
};

typedef struct l2cap_socket {

    struct l2cap_socket   *prev;                 //link to prev list item
    struct l2cap_socket   *next;                 //link to next list item
    bt_bdaddr_t            addr;                 //other side's address
    char                   name[256];            //user-friendly name of the service
    uint32_t               id;                   //just a tag to find this struct
    int                    app_uid;              // The UID of the app who requested this socket
    int                    handle;               //handle from lower layers
    unsigned               security;             //security flags
    int                    channel;              //channel (fixed_chan) or PSM (!fixed_chan)
    int                    our_fd;               //fd from our side
    int                    app_fd;               //fd from app's side

    unsigned               bytes_buffered;
    struct packet         *first_packet;         //fist packet to be delivered to app
    struct packet         *last_packet;          //last packet to be delivered to app

    fixed_queue_t         *incoming_que;         //data that came in but has not yet been read
    unsigned               fixed_chan       :1;  //fixed channel (or psm?)
    unsigned               server           :1;  //is a server? (or connecting?)
    unsigned               connected        :1;  //is connected?
    unsigned               outgoing_congest :1;  //should we hold?
    unsigned               server_psm_sent  :1;  //The server shall only send PSM once.
    BOOLEAN                is_le_coc;            //is le connection oriented channel?
} l2cap_socket;

static bt_status_t btSock_start_l2cap_server_l(l2cap_socket *sock);

static pthread_mutex_t state_lock = PTHREAD_MUTEX_INITIALIZER;

l2cap_socket *socks = NULL;
static uid_set_t* uid_set = NULL;
static int pth = -1;

static void btsock_l2cap_cbk(tBTA_JV_EVT event, tBTA_JV *p_data, void *user_data);

/* TODO: Consider to remove this buffer, as we have a buffer in l2cap as well, and we risk
 *       a buffer overflow with this implementation if the socket data is not read from
 *       JAVA for a while. In such a case we should use flow control to tell the sender to
 *       back off.
 *       BUT remember we need to avoid blocking the BTA task execution - hence we cannot
 *       directly write to the socket.
 *       we should be able to change to store the data pointer here, and just wait
 *       confirming the l2cap_ind until we have more space in the buffer. */

/* returns FALSE if none - caller must free "data" memory when done with it */
static char packet_get_head_l(l2cap_socket *sock, uint8_t **data, uint32_t *len)
{
    struct packet *p = sock->first_packet;

    if (!p)
        return FALSE;

    if (data)
        *data = sock->first_packet->data;
    if (len)
        *len = sock->first_packet->len;
    sock->first_packet = p->next;
    if (sock->first_packet)
        sock->first_packet->prev = NULL;
    else
        sock->last_packet = NULL;

    if(len)
        sock->bytes_buffered -= *len;

    osi_free(p);

    return TRUE;
}

static struct packet *packet_alloc(const uint8_t *data, uint32_t len)
{
    struct packet *p = osi_calloc(sizeof(*p));
    uint8_t *buf = osi_malloc(len);

    p->data = buf;
    p->len = len;
    memcpy(p->data, data, len);
    return p;
}

/* makes a copy of the data, returns TRUE on success */
static char packet_put_head_l(l2cap_socket *sock, const void *data, uint32_t len)
{
    struct packet *p = packet_alloc((const uint8_t*)data, len);

    /*
     * We do not check size limits here since this is used to undo "getting" a
     * packet that the user read incompletely. That is to say the packet was
     * already in the queue. We do check thos elimits in packet_put_tail_l() since
     * that function is used to put new data into the queue.
     */

    if (!p)
        return FALSE;

    p->prev = NULL;
    p->next = sock->first_packet;
    sock->first_packet = p;
    if (p->next)
        p->next->prev = p;
    else
        sock->last_packet = p;

    sock->bytes_buffered += len;

    return TRUE;
}

/* makes a copy of the data, returns TRUE on success */
static char packet_put_tail_l(l2cap_socket *sock, const void *data, uint32_t len)
{
    struct packet *p = packet_alloc((const uint8_t*)data, len);

    if (sock->bytes_buffered >= L2CAP_MAX_RX_BUFFER) {
        LOG_ERROR(LOG_TAG, "packet_put_tail_l: buffer overflow");
        return FALSE;
    }

    if (!p) {
        LOG_ERROR(LOG_TAG, "packet_put_tail_l: unable to allocate packet...");
        return FALSE;
    }

    p->next = NULL;
    p->prev = sock->last_packet;
    sock->last_packet = p;
    if (p->prev)
        p->prev->next = p;
    else
        sock->first_packet = p;

    sock->bytes_buffered += len;

    return TRUE;
}

static inline void bd_copy(UINT8* dest, UINT8* src, BOOLEAN swap)
{
    if (swap) {
        int i;
        for (i =0; i < 6 ;i++)
            dest[i]= src[5-i];
    }
    else memcpy(dest, src, 6);
}

static char is_inited(void)
{
    char ret;

    pthread_mutex_lock(&state_lock);
    ret = pth != -1;
    pthread_mutex_unlock(&state_lock);

    return ret;
}

/* only call with mutex taken */
static l2cap_socket *btsock_l2cap_find_by_id_l(uint32_t id)
{
    l2cap_socket *sock = socks;

    while (sock && sock->id != id)
        sock = sock->next;

    return sock;
}

static void btsock_l2cap_free_l(l2cap_socket *sock)
{
    uint8_t *buf;
    l2cap_socket *t = socks;

    while(t && t != sock)
        t = t->next;

    if (!t) /* prever double-frees */
        return;

    if (sock->next)
        sock->next->prev = sock->prev;

    if (sock->prev)
        sock->prev->next = sock->next;
    else
        socks = sock->next;

    shutdown(sock->our_fd, SHUT_RDWR);
    close(sock->our_fd);
    if (sock->app_fd != -1) {
        close(sock->app_fd);
    } else {
        APPL_TRACE_ERROR("SOCK_LIST: free(id = %d) - NO app_fd!", sock->id);
    }

    while (packet_get_head_l(sock, &buf, NULL))
        osi_free(buf);

    //lower-level close() should be idempotent... so let's call it and see...
    if (sock->is_le_coc)
    {
        // Only call if we are non server connections
        if (sock->handle >= 0 && (sock->server == FALSE)) {
                BTA_JvL2capClose(sock->handle);
        }
        if ((sock->channel >= 0) && (sock->server == TRUE)) {
            BTA_JvFreeChannel(sock->channel, BTA_JV_CONN_TYPE_L2CAP);
        }
    }
    else
    {
        // Only call if we are non server connections
        if ((sock->handle >= 0) && (sock->server == FALSE)) {
            if (sock->fixed_chan)
                BTA_JvL2capCloseLE(sock->handle);
            else
                BTA_JvL2capClose(sock->handle);
        }
        if ((sock->channel >= 0) && (sock->server == TRUE)) {
            if (sock->fixed_chan)
                BTA_JvFreeChannel(sock->channel, BTA_JV_CONN_TYPE_L2CAP_LE);
            else
                BTA_JvFreeChannel(sock->channel, BTA_JV_CONN_TYPE_L2CAP);

            if (!sock->fixed_chan) {
                APPL_TRACE_DEBUG("%s stopping L2CAP server channel %d", __func__, sock->channel);
                BTA_JvL2capStopServer(sock->channel, UINT_TO_PTR(sock->id));
            }
        }
    }

    APPL_TRACE_DEBUG("%s: free(id = %d)", __func__, sock->id);
    osi_free(sock);
}

static l2cap_socket *btsock_l2cap_alloc_l(const char *name, const bt_bdaddr_t *addr,
        char is_server, int flags)
{
    unsigned security = 0;
    int fds[2];
    l2cap_socket *sock = osi_calloc(sizeof(*sock));

    if (flags & BTSOCK_FLAG_ENCRYPT)
        security |= is_server ? BTM_SEC_IN_ENCRYPT : BTM_SEC_OUT_ENCRYPT;
    if (flags & BTSOCK_FLAG_AUTH)
        security |= is_server ? BTM_SEC_IN_AUTHENTICATE : BTM_SEC_OUT_AUTHENTICATE;
    if (flags & BTSOCK_FLAG_AUTH_MITM)
        security |= is_server ? BTM_SEC_IN_MITM : BTM_SEC_OUT_MITM;
    if (flags & BTSOCK_FLAG_AUTH_16_DIGIT)
        security |= BTM_SEC_IN_MIN_16_DIGIT_PIN;

    if (socketpair(AF_LOCAL, SOCK_SEQPACKET, 0, fds)) {
        APPL_TRACE_ERROR("socketpair failed, errno:%d", errno);
        goto fail_sockpair;
    }

    sock->our_fd = fds[0];
    sock->app_fd = fds[1];
    sock->security = security;
    sock->server = is_server;
    sock->connected = FALSE;
    sock->handle = 0;
    sock->server_psm_sent = FALSE;
    sock->app_uid = -1;

    if (name)
        strncpy(sock->name, name, sizeof(sock->name) - 1);
    if (addr)
        sock->addr = *addr;

    sock->first_packet = NULL;
    sock->last_packet = NULL;

    sock->next = socks;
    sock->prev = NULL;
    if (socks)
        socks->prev = sock;
    sock->id = (socks ? socks->id : 0) + 1;
    socks = sock;
    /* paranoia cap on: verify no ID duplicates due to overflow and fix as needed */
    while (1) {
        l2cap_socket *t;
        t = socks->next;
        while (t && t->id != sock->id) {
            t = t->next;
        }
        if (!t && sock->id) /* non-zeor handle is unique -> we're done */
            break;
        /* if we're here, we found a duplicate */
        if (!++sock->id) /* no zero IDs allowed */
            sock->id++;
    }
    APPL_TRACE_DEBUG("SOCK_LIST: alloc(id = %d)", sock->id);
    return sock;

fail_sockpair:
    osi_free(sock);
    return NULL;
}

bt_status_t btsock_l2cap_init(int handle, uid_set_t* set)
{
    APPL_TRACE_DEBUG("%s handle = %d", __func__);
    pthread_mutex_lock(&state_lock);
    pth = handle;
    socks = NULL;
    uid_set = set;
    pthread_mutex_unlock(&state_lock);

    return BT_STATUS_SUCCESS;
}

bt_status_t btsock_l2cap_cleanup()
{
    pthread_mutex_lock(&state_lock);
    pth = -1;
    while (socks)
        btsock_l2cap_free_l(socks);
    pthread_mutex_unlock(&state_lock);

    return BT_STATUS_SUCCESS;
}

static inline BOOLEAN send_app_psm_or_chan_l(l2cap_socket *sock)
{
    return sock_send_all(sock->our_fd, (const uint8_t*)&sock->channel, sizeof(sock->channel))
            == sizeof(sock->channel);
}

static BOOLEAN send_app_connect_signal(int fd, const bt_bdaddr_t* addr,
        int channel, int status, int send_fd, int tx_mtu)
{
    sock_connect_signal_t cs;
    cs.size = sizeof(cs);
    cs.bd_addr = *addr;
    cs.channel = channel;
    cs.status = status;
    cs.max_rx_packet_size = L2CAP_MAX_SDU_LENGTH;
    cs.max_tx_packet_size = tx_mtu;
    if (send_fd != -1) {
        if (sock_send_fd(fd, (const uint8_t*)&cs, sizeof(cs), send_fd) == sizeof(cs))
            return TRUE;
        else APPL_TRACE_ERROR("sock_send_fd failed, fd:%d, send_fd:%d", fd, send_fd);
    } else if (sock_send_all(fd, (const uint8_t*)&cs, sizeof(cs)) == sizeof(cs)) {
        return TRUE;
    }
    return FALSE;
}

static void on_srv_l2cap_listen_started(tBTA_JV_L2CAP_START *p_start, uint32_t id)
{
    l2cap_socket *sock;

    pthread_mutex_lock(&state_lock);
    sock = btsock_l2cap_find_by_id_l(id);
    if (sock) {
        if (p_start->status != BTA_JV_SUCCESS) {
            APPL_TRACE_ERROR("Error starting l2cap_listen - status: 0x%04x", p_start->status);
            btsock_l2cap_free_l(sock);
        }
        else {
            sock->handle = p_start->handle;
            APPL_TRACE_DEBUG("on_srv_l2cap_listen_started() sock->handle =%d id:%d",
                    sock->handle, sock->id);
            if(sock->server_psm_sent == FALSE) {
                if (!send_app_psm_or_chan_l(sock)) {
                    //closed
                    APPL_TRACE_DEBUG("send_app_psm() failed, close rs->id:%d", sock->id);
                    btsock_l2cap_free_l(sock);
                } else {
                    sock->server_psm_sent = TRUE;
                }
            }
        }
    }
    pthread_mutex_unlock(&state_lock);
}

static void on_cl_l2cap_init(tBTA_JV_L2CAP_CL_INIT *p_init, uint32_t id)
{
    l2cap_socket *sock;

    pthread_mutex_lock(&state_lock);
    sock = btsock_l2cap_find_by_id_l(id);
    if (sock) {
        if (p_init->status != BTA_JV_SUCCESS) {
            btsock_l2cap_free_l(sock);
        } else {
            sock->handle = p_init->handle;
        }
    }
    pthread_mutex_unlock(&state_lock);
}

/**
 * Here we allocate a new sock instance to mimic the BluetoothSocket. The socket will be a clone
 * of the sock representing the BluetoothServerSocket.
 * */
static void on_srv_l2cap_psm_connect_l(tBTA_JV_L2CAP_OPEN *p_open, l2cap_socket *sock)
{
    l2cap_socket *accept_rs;
    uint32_t new_listen_id;

    // Mutex locked by caller
    accept_rs = btsock_l2cap_alloc_l(sock->name, (const bt_bdaddr_t*)p_open->rem_bda, FALSE, 0);
    if (accept_rs) {
        accept_rs->connected = TRUE;
        accept_rs->security = sock->security;
        accept_rs->fixed_chan = sock->fixed_chan;
        accept_rs->channel = sock->channel;
        accept_rs->handle = sock->handle;
        accept_rs->app_uid = sock->app_uid;
        sock->handle = -1; /* We should no longer associate this handle with the server socket */
        accept_rs->is_le_coc = sock->is_le_coc;

    /* Swap IDs to hand over the GAP connection to the accepted socket, and start a new server on
       the newly create socket ID. */
        new_listen_id = accept_rs->id;
        accept_rs->id = sock->id;
        sock->id = new_listen_id;
    } else {
        APPL_TRACE_ERROR("Memory not allocated for accept_rs..");
    }

    if (accept_rs) {
        //start monitor the socket
        btsock_thread_add_fd(pth, sock->our_fd, BTSOCK_L2CAP, SOCK_THREAD_FD_EXCEPTION, sock->id);
        btsock_thread_add_fd(pth, accept_rs->our_fd, BTSOCK_L2CAP, SOCK_THREAD_FD_RD,
                accept_rs->id);
        APPL_TRACE_DEBUG("sending connect signal & app fd: %d to app server to accept() the"
                " connection", accept_rs->app_fd);
        APPL_TRACE_DEBUG("server fd:%d, scn:%d", sock->our_fd, sock->channel);
        send_app_connect_signal(sock->our_fd, &accept_rs->addr, sock->channel, 0,
                accept_rs->app_fd, p_open->tx_mtu);
        accept_rs->app_fd = -1; // The fd is closed after sent to app in send_app_connect_signal()
                                // But for some reason we still leak a FD - either the server socket
                                // one or the accept socket one.
        if(btSock_start_l2cap_server_l(sock) != BT_STATUS_SUCCESS) {
            btsock_l2cap_free_l(sock);
        }
    }
}

static void on_srv_l2cap_le_connect_l(tBTA_JV_L2CAP_LE_OPEN *p_open, l2cap_socket *sock)
{
    l2cap_socket *accept_rs;
    uint32_t new_listen_id;

    // mutex locked by caller
    accept_rs = btsock_l2cap_alloc_l(sock->name, (const bt_bdaddr_t*)p_open->rem_bda, FALSE, 0);
    if (accept_rs) {

        //swap IDs
        new_listen_id = accept_rs->id;
        accept_rs->id = sock->id;
        sock->id = new_listen_id;

        accept_rs->handle = p_open->handle;
        accept_rs->connected = TRUE;
        accept_rs->security = sock->security;
        accept_rs->fixed_chan = sock->fixed_chan;
        accept_rs->channel = sock->channel;
        accept_rs->app_uid = sock->app_uid;

        //if we do not set a callback, this socket will be dropped */
        *(p_open->p_p_cback) = (void*)btsock_l2cap_cbk;
        *(p_open->p_user_data) = UINT_TO_PTR(accept_rs->id);

        //start monitor the socket
        btsock_thread_add_fd(pth, sock->our_fd, BTSOCK_L2CAP, SOCK_THREAD_FD_EXCEPTION, sock->id);
        btsock_thread_add_fd(pth, accept_rs->our_fd, BTSOCK_L2CAP, SOCK_THREAD_FD_RD,
                accept_rs->id);
        APPL_TRACE_DEBUG("sending connect signal & app fd:%dto app server to accept() the"
                " connection", accept_rs->app_fd);
        APPL_TRACE_DEBUG("server fd:%d, scn:%d", sock->our_fd, sock->channel);
        send_app_connect_signal(sock->our_fd, &accept_rs->addr, sock->channel, 0,
                accept_rs->app_fd, p_open->tx_mtu);
        accept_rs->app_fd = -1; //the fd is closed after sent to app
    }
}

static void on_cl_l2cap_psm_connect_l(tBTA_JV_L2CAP_OPEN *p_open, l2cap_socket *sock)
{
    bd_copy(sock->addr.address, p_open->rem_bda, 0);

    if (!send_app_psm_or_chan_l(sock)) {
        APPL_TRACE_ERROR("send_app_psm_or_chan_l failed");
        return;
    }

    if (send_app_connect_signal(sock->our_fd, &sock->addr, sock->channel, 0, -1, p_open->tx_mtu)) {
        //start monitoring the socketpair to get call back when app writing data
        APPL_TRACE_DEBUG("on_l2cap_connect_ind, connect signal sent, slot id:%d, psm:%d,"
                " server:%d", sock->id, sock->channel, sock->server);
        btsock_thread_add_fd(pth, sock->our_fd, BTSOCK_L2CAP, SOCK_THREAD_FD_RD, sock->id);
        sock->connected = TRUE;
    }
    else APPL_TRACE_ERROR("send_app_connect_signal failed");
}

static void on_cl_l2cap_le_connect_l(tBTA_JV_L2CAP_LE_OPEN *p_open, l2cap_socket *sock)
{
    bd_copy(sock->addr.address, p_open->rem_bda, 0);

    if (!send_app_psm_or_chan_l(sock)) {
        APPL_TRACE_ERROR("send_app_psm_or_chan_l failed");
        return;
    }

    if (send_app_connect_signal(sock->our_fd, &sock->addr, sock->channel, 0, -1, p_open->tx_mtu)) {
        //start monitoring the socketpair to get call back when app writing data
        APPL_TRACE_DEBUG("on_l2cap_connect_ind, connect signal sent, slot id:%d, Chan:%d,"
                " server:%d", sock->id, sock->channel, sock->server);
        btsock_thread_add_fd(pth, sock->our_fd, BTSOCK_L2CAP, SOCK_THREAD_FD_RD, sock->id);
        sock->connected = TRUE;
    }
    else APPL_TRACE_ERROR("send_app_connect_signal failed");
}

static void on_l2cap_connect(tBTA_JV *p_data, uint32_t id)
{
    l2cap_socket *sock;
    tBTA_JV_L2CAP_OPEN *psm_open = &p_data->l2c_open;
    tBTA_JV_L2CAP_LE_OPEN *le_open = &p_data->l2c_le_open;

    pthread_mutex_lock(&state_lock);
    sock = btsock_l2cap_find_by_id_l(id);
    if (!sock) {
        APPL_TRACE_ERROR("on_l2cap_connect on unknown socket");
    } else {
        if (sock->fixed_chan && le_open->status == BTA_JV_SUCCESS) {
            if (!sock->server)
                on_cl_l2cap_le_connect_l(le_open, sock);
            else
                on_srv_l2cap_le_connect_l(le_open, sock);
        } else if (!sock->fixed_chan && psm_open->status == BTA_JV_SUCCESS) {
            if (!sock->server)
                on_cl_l2cap_psm_connect_l(psm_open, sock);
            else
                on_srv_l2cap_psm_connect_l(psm_open, sock);
        }
        else
            btsock_l2cap_free_l(sock);
    }
    pthread_mutex_unlock(&state_lock);
}

static void on_l2cap_close(tBTA_JV_L2CAP_CLOSE * p_close, uint32_t id)
{
    l2cap_socket *sock;

    pthread_mutex_lock(&state_lock);
    sock = btsock_l2cap_find_by_id_l(id);
    if (sock) {
        APPL_TRACE_DEBUG("on_l2cap_close, slot id:%d, fd:%d, %s:%d, server:%d",
                sock->id, sock->our_fd, sock->fixed_chan ? "fixed_chan" : "PSM",
                sock->channel, sock->server);
        // TODO: This does not seem to be called...
        // I'm not sure if this will be called for non-server sockets?
        if(!sock->fixed_chan && (sock->server == TRUE)) {
            BTA_JvFreeChannel(sock->channel, BTA_JV_CONN_TYPE_L2CAP);
        }
        btsock_l2cap_free_l(sock);
    }
    pthread_mutex_unlock(&state_lock);
}

static void on_l2cap_outgoing_congest(tBTA_JV_L2CAP_CONG *p, uint32_t id)
{
    l2cap_socket *sock;

    pthread_mutex_lock(&state_lock);
    sock = btsock_l2cap_find_by_id_l(id);
    if (sock) {
        sock->outgoing_congest = p->cong ? 1 : 0;
        //mointer the fd for any outgoing data
        if (!sock->outgoing_congest) {
            APPL_TRACE_DEBUG("on_l2cap_outgoing_congest: adding fd to btsock_thread...");
            btsock_thread_add_fd(pth, sock->our_fd, BTSOCK_L2CAP, SOCK_THREAD_FD_RD, sock->id);

        }
    }
    pthread_mutex_unlock(&state_lock);
}

static void on_l2cap_write_done(void* req_id, uint16_t len, uint32_t id)
{
    l2cap_socket *sock;

    if (req_id != NULL) {
        osi_free(req_id); //free the buffer
    }

    int app_uid = -1;

    pthread_mutex_lock(&state_lock);
    sock = btsock_l2cap_find_by_id_l(id);
    if (sock) {
        app_uid = sock->app_uid;
        if (!sock->outgoing_congest) {
            //monitor the fd for any outgoing data
            APPL_TRACE_DEBUG("on_l2cap_write_done: adding fd to btsock_thread...");
            btsock_thread_add_fd(pth, sock->our_fd, BTSOCK_L2CAP, SOCK_THREAD_FD_RD, sock->id);
        }
    }
    pthread_mutex_unlock(&state_lock);

    uid_set_add_tx(uid_set, app_uid, len);
}

static void on_l2cap_write_fixed_done(void* req_id, uint16_t len, uint32_t id)
{
    l2cap_socket *sock;

    if (req_id != NULL) {
        osi_free(req_id); //free the buffer
    }

    int app_uid = -1;
    pthread_mutex_lock(&state_lock);
    sock = btsock_l2cap_find_by_id_l(id);
    if (sock) {
        app_uid = sock->app_uid;
        if (!sock->outgoing_congest) {
            //monitor the fd for any outgoing data
            btsock_thread_add_fd(pth, sock->our_fd, BTSOCK_L2CAP, SOCK_THREAD_FD_RD, sock->id);
        }
    }
    pthread_mutex_unlock(&state_lock);

    uid_set_add_tx(uid_set, app_uid, len);
}

static void on_l2cap_data_ind(tBTA_JV *evt, uint32_t id)
{
    l2cap_socket *sock;

    int app_uid = -1;
    UINT32 bytes_read = 0;

    pthread_mutex_lock(&state_lock);
    sock = btsock_l2cap_find_by_id_l(id);
    if (sock) {
        app_uid = sock->app_uid;

        if (sock->fixed_chan) { /* we do these differently */

            tBTA_JV_LE_DATA_IND *p_le_data_ind = &evt->le_data_ind;
            BT_HDR *p_buf = p_le_data_ind->p_buf;
            uint8_t *data = (uint8_t*)(p_buf + 1) + p_buf->offset;

            if (packet_put_tail_l(sock, data, p_buf->len)) {
                bytes_read = p_buf->len;
                btsock_thread_add_fd(pth, sock->our_fd, BTSOCK_L2CAP, SOCK_THREAD_FD_WR, sock->id);
            } else {//connection must be dropped
                APPL_TRACE_DEBUG("on_l2cap_data_ind() unable to push data to socket - closing"
                        " fixed channel");
                BTA_JvL2capCloseLE(sock->handle);
                btsock_l2cap_free_l(sock);
            }

        } else {

            UINT8 buffer[L2CAP_MAX_SDU_LENGTH];
            UINT32  count;

            if (BTA_JvL2capReady(sock->handle, &count) == BTA_JV_SUCCESS) {
                if (BTA_JvL2capRead(sock->handle, sock->id, buffer, count) == BTA_JV_SUCCESS) {
                    if (packet_put_tail_l(sock, buffer, count)) {
                        bytes_read = count;
                        btsock_thread_add_fd(pth, sock->our_fd, BTSOCK_L2CAP, SOCK_THREAD_FD_WR,
                                sock->id);
                    } else {//connection must be dropped
                        APPL_TRACE_DEBUG("on_l2cap_data_ind() unable to push data to socket"
                                " - closing channel");
                        BTA_JvL2capClose(sock->handle);
                        btsock_l2cap_free_l(sock);
                    }
                }
            }
        }
    }
    pthread_mutex_unlock(&state_lock);

    uid_set_add_rx(uid_set, app_uid, bytes_read);
}

static void btsock_l2cap_cbk(tBTA_JV_EVT event, tBTA_JV *p_data, void *user_data)
{
    uint32_t sock_id = PTR_TO_UINT(user_data);

    switch (event) {
    case BTA_JV_L2CAP_START_EVT:
        on_srv_l2cap_listen_started(&p_data->l2c_start, sock_id);
        break;

    case BTA_JV_L2CAP_CL_INIT_EVT:
        on_cl_l2cap_init(&p_data->l2c_cl_init, sock_id);
        break;

    case BTA_JV_L2CAP_OPEN_EVT:
        on_l2cap_connect(p_data, sock_id);
        BTA_JvSetPmProfile(p_data->l2c_open.handle, BTA_JV_PM_ID_1,BTA_JV_CONN_OPEN);
        break;

    case BTA_JV_L2CAP_CLOSE_EVT:
        APPL_TRACE_DEBUG("BTA_JV_L2CAP_CLOSE_EVT: id: %u", sock_id);
        on_l2cap_close(&p_data->l2c_close, sock_id);
        break;

    case BTA_JV_L2CAP_DATA_IND_EVT:
        on_l2cap_data_ind(p_data, sock_id);
        APPL_TRACE_DEBUG("BTA_JV_L2CAP_DATA_IND_EVT");
        break;

    case BTA_JV_L2CAP_READ_EVT:
        APPL_TRACE_DEBUG("BTA_JV_L2CAP_READ_EVT not used");
        break;

    case BTA_JV_L2CAP_WRITE_EVT:
        APPL_TRACE_DEBUG("BTA_JV_L2CAP_WRITE_EVT: id: %u", sock_id);
        on_l2cap_write_done(UINT_TO_PTR(p_data->l2c_write.req_id), p_data->l2c_write.len, sock_id);
        break;

    case BTA_JV_L2CAP_WRITE_FIXED_EVT:
        APPL_TRACE_DEBUG("BTA_JV_L2CAP_WRITE_FIXED_EVT: id: %u", sock_id);
        on_l2cap_write_fixed_done(UINT_TO_PTR(p_data->l2c_write_fixed.req_id), p_data->l2c_write.len, sock_id);
        break;

    case BTA_JV_L2CAP_CONG_EVT:
        on_l2cap_outgoing_congest(&p_data->l2c_cong, sock_id);
        break;

    default:
        APPL_TRACE_ERROR("unhandled event %d, slot id: %u", event, sock_id);
        break;
    }
}

/* L2CAP default options for OBEX socket connections */
const tL2CAP_FCR_OPTS obex_l2c_fcr_opts_def =
{
    L2CAP_FCR_ERTM_MODE,            /* Mandatory for OBEX over l2cap */
    OBX_FCR_OPT_TX_WINDOW_SIZE_BR_EDR,/* Tx window size */
    OBX_FCR_OPT_MAX_TX_B4_DISCNT,   /* Maximum transmissions before disconnecting */
    OBX_FCR_OPT_RETX_TOUT,          /* Retransmission timeout (2 secs) */
    OBX_FCR_OPT_MONITOR_TOUT,       /* Monitor timeout (12 secs) */
    OBX_FCR_OPT_MAX_PDU_SIZE        /* MPS segment size */
};
const tL2CAP_ERTM_INFO obex_l2c_etm_opt =
{
        L2CAP_FCR_ERTM_MODE,            /* Mandatory for OBEX over l2cap */
        L2CAP_FCR_CHAN_OPT_ERTM,        /* Mandatory for OBEX over l2cap */
        OBX_USER_RX_BUF_SIZE,
        OBX_USER_TX_BUF_SIZE,
        OBX_FCR_RX_BUF_SIZE,
        OBX_FCR_TX_BUF_SIZE
};

/**
 * When using a dynamic PSM, a PSM allocation is requested from btsock_l2cap_listen_or_connect().
 * The PSM allocation event is refeived in the JV-callback - currently located in RFC-code -
 * and this function is called with the newly allocated PSM.
 */
void on_l2cap_psm_assigned(int id, int psm) {
    /* Setup ETM settings:
     *  mtu will be set below */
    pthread_mutex_lock(&state_lock);
    l2cap_socket *sock = btsock_l2cap_find_by_id_l(id);

    if (sock) {
        sock->channel = psm;

        if (btSock_start_l2cap_server_l(sock) != BT_STATUS_SUCCESS)
            btsock_l2cap_free_l(sock);
    } else {
        APPL_TRACE_ERROR("%s: Error: sock is null", __func__);
    }

    pthread_mutex_unlock(&state_lock);
}

static bt_status_t btSock_start_l2cap_server_l(l2cap_socket *sock) {
    tL2CAP_CFG_INFO cfg;
    bt_status_t stat = BT_STATUS_SUCCESS;
    /* Setup ETM settings:
     *  mtu will be set below */
    memset(&cfg, 0, sizeof(tL2CAP_CFG_INFO));

    cfg.fcr_present = TRUE;
    cfg.fcr = obex_l2c_fcr_opts_def;

    if (sock->fixed_chan) {

        if (BTA_JvL2capStartServerLE(sock->security, 0, NULL, sock->channel,
                L2CAP_DEFAULT_MTU, NULL, btsock_l2cap_cbk, UINT_TO_PTR(sock->id))
                != BTA_JV_SUCCESS)
            stat = BT_STATUS_FAIL;

    } else {
        /* If we have a channel specified in the request, just start the server,
         * else we request a PSM and start the server after we receive a PSM. */
        if (sock->channel < 0) {
            if (sock->is_le_coc) {
                if (BTA_JvGetChannelId(BTA_JV_CONN_TYPE_L2CAP_LE, UINT_TO_PTR(sock->id), 0)
                        != BTA_JV_SUCCESS)
                    stat = BT_STATUS_FAIL;
            }
            else
            {
                if (BTA_JvGetChannelId(BTA_JV_CONN_TYPE_L2CAP, UINT_TO_PTR(sock->id), 0)
                        != BTA_JV_SUCCESS)
                    stat = BT_STATUS_FAIL;
            }
        } else {
            if (sock->is_le_coc) {
                if (BTA_JvL2capStartServer(BTA_JV_CONN_TYPE_L2CAP_LE, sock->security, 0, NULL,
                        sock->channel, L2CAP_MAX_SDU_LENGTH, &cfg, btsock_l2cap_cbk, UINT_TO_PTR(sock->id))
                        != BTA_JV_SUCCESS)
                    stat = BT_STATUS_FAIL;
            }
            else
            {
                if (BTA_JvL2capStartServer(BTA_JV_CONN_TYPE_L2CAP, sock->security, 0, &obex_l2c_etm_opt,
                        sock->channel, L2CAP_MAX_SDU_LENGTH, &cfg, btsock_l2cap_cbk, UINT_TO_PTR(sock->id))
                        != BTA_JV_SUCCESS)
                    stat = BT_STATUS_FAIL;
            }
        }
    }
    return stat;
}

static bt_status_t btsock_l2cap_listen_or_connect(const char *name, const bt_bdaddr_t *addr,
        int channel, int* sock_fd, int flags, char listen, int app_uid)
{
    bt_status_t stat;
    int fixed_chan = 1;
    l2cap_socket *sock;
    tL2CAP_CFG_INFO cfg;
    BOOLEAN is_le_coc = FALSE;

    if (!sock_fd)
        return BT_STATUS_PARM_INVALID;

    if(channel < 0) {
        // We need to auto assign a PSM
        fixed_chan = 0;
    } else {
        fixed_chan = (channel & L2CAP_MASK_FIXED_CHANNEL) != 0;
        is_le_coc = (channel & L2CAP_MASK_LE_COC_CHANNEL) != 0;
        channel &=~ (L2CAP_MASK_FIXED_CHANNEL | L2CAP_MASK_LE_COC_CHANNEL);
    }

    if (!is_inited())
        return BT_STATUS_NOT_READY;

    // TODO: This is kind of bad to lock here, but it is needed for the current design.
    pthread_mutex_lock(&state_lock);

    sock = btsock_l2cap_alloc_l(name, addr, listen, flags);
    if (!sock) {
        pthread_mutex_unlock(&state_lock);
        return BT_STATUS_NOMEM;
    }

    sock->fixed_chan = fixed_chan;
    sock->channel = channel;
    sock->app_uid = app_uid;
    sock->is_le_coc = is_le_coc;

    stat = BT_STATUS_SUCCESS;

    /* Setup ETM settings:
     *  mtu will be set below */
    memset(&cfg, 0, sizeof(tL2CAP_CFG_INFO));

    cfg.fcr_present = TRUE;
    cfg.fcr = obex_l2c_fcr_opts_def;

    /* "role" is never initialized in rfcomm code */
    if (listen) {
        stat = btSock_start_l2cap_server_l(sock);
    } else {
        if (fixed_chan) {
            if (BTA_JvL2capConnectLE(sock->security, 0, NULL, channel,
                    L2CAP_DEFAULT_MTU, NULL, sock->addr.address, btsock_l2cap_cbk,
                    UINT_TO_PTR(sock->id)) != BTA_JV_SUCCESS)
                stat = BT_STATUS_FAIL;

        } else {
            if (sock->is_le_coc) {
                if (BTA_JvL2capConnect(BTA_JV_CONN_TYPE_L2CAP_LE, sock->security, 0, NULL,
                        channel, L2CAP_MAX_SDU_LENGTH, &cfg, sock->addr.address,
                        btsock_l2cap_cbk, UINT_TO_PTR(sock->id)) != BTA_JV_SUCCESS)
                    stat = BT_STATUS_FAIL;
            }
            else
            {
                if (BTA_JvL2capConnect(BTA_JV_CONN_TYPE_L2CAP, sock->security, 0, &obex_l2c_etm_opt,
                        channel, L2CAP_MAX_SDU_LENGTH, &cfg, sock->addr.address,
                        btsock_l2cap_cbk, UINT_TO_PTR(sock->id)) != BTA_JV_SUCCESS)
                    stat = BT_STATUS_FAIL;
            }
        }
    }

    if (stat == BT_STATUS_SUCCESS) {
        *sock_fd = sock->app_fd;
        /* We pass the FD to JAVA, but since it runs in another process, we need to also close
         * it in native, either straight away, as done when accepting an incoming connection,
         * or when doing cleanup after this socket */
        sock->app_fd = -1;  /*This leaks the file descriptor. The FD should be closed in
                              JAVA but it apparently do not work */
        btsock_thread_add_fd(pth, sock->our_fd, BTSOCK_L2CAP, SOCK_THREAD_FD_EXCEPTION,
                sock->id);
    } else {
       btsock_l2cap_free_l(sock);
    }
    pthread_mutex_unlock(&state_lock);

    return stat;
}

bt_status_t btsock_l2cap_listen(const char* name, int channel, int* sock_fd, int flags, int app_uid)
{
    return btsock_l2cap_listen_or_connect(name, NULL, channel, sock_fd, flags, 1, app_uid);
}

bt_status_t btsock_l2cap_connect(const bt_bdaddr_t *bd_addr, int channel, int* sock_fd, int flags, int app_uid)
{
    return btsock_l2cap_listen_or_connect(NULL, bd_addr, channel, sock_fd, flags, 0, app_uid);
}

/* return TRUE if we have more to send and should wait for user readiness, FALSE else
 * (for example: unrecoverable error or no data)
 */
static BOOLEAN flush_incoming_que_on_wr_signal_l(l2cap_socket *sock)
{
    uint8_t *buf;
    uint32_t len;

    while (packet_get_head_l(sock, &buf, &len)) {
        ssize_t sent;
        OSI_NO_INTR(sent = send(sock->our_fd, buf, len, MSG_DONTWAIT));
        int saved_errno = errno;

        if (sent == (signed)len)
            osi_free(buf);
        else if (sent >= 0) {
            packet_put_head_l(sock, buf + sent, len - sent);
            osi_free(buf);
            if (!sent) /* special case if other end not keeping up */
                return TRUE;
        }
        else {
            packet_put_head_l(sock, buf, len);
            osi_free(buf);
            return saved_errno == EWOULDBLOCK || saved_errno == EAGAIN;
        }
    }

    return FALSE;
}

void btsock_l2cap_signaled(int fd, int flags, uint32_t user_id)
{
    l2cap_socket *sock;
    char drop_it = FALSE;

    /* We use MSG_DONTWAIT when sending data to JAVA, hence it can be accepted to hold the lock. */
    pthread_mutex_lock(&state_lock);
    sock = btsock_l2cap_find_by_id_l(user_id);
    if (sock) {
        if ((flags & SOCK_THREAD_FD_RD) && !sock->server) {
            //app sending data
            if (sock->connected) {
                int size = 0;

                if (!(flags & SOCK_THREAD_FD_EXCEPTION) || (ioctl(sock->our_fd, FIONREAD, &size)
                        == 0 && size)) {
                    uint8_t *buffer = osi_malloc(L2CAP_MAX_SDU_LENGTH);
                    /* Apparently we hijack the req_id (UINT32) to pass the pointer to the buffer to
                     * the write complete callback, which call a free... wonder if this works on a
                     * 64 bit platform? */
                    /* The socket is created with SOCK_SEQPACKET, hence we read one message at
                     * the time. The maximum size of a message is allocated to ensure data is
                     * not lost. This is okay to do as Android uses virtual memory, hence even
                     * if we only use a fraction of the memory it should not block for others
                     * to use the memory. As the definition of ioctl(FIONREAD) do not clearly
                     * define what value will be returned if multiple messages are written to
                     * the socket before any message is read from the socket, we could
                     * potentially risk to allocate way more memory than needed. One of the use
                     * cases for this socket is obex where multiple 64kbyte messages are
                     * typically written to the socket in a tight loop, hence we risk the ioctl
                     * will return the total amount of data in the buffer, which could be
                     * multiple 64kbyte chunks.
                     * UPDATE: As the stack cannot handle 64kbyte buffers, the size is reduced
                     * to around 8kbyte - and using malloc for buffer allocation here seems to
                     * be wrong
                     * UPDATE: Since we are responsible for freeing the buffer in the
                     * write_complete_ind, it is OK to use malloc. */
                    ssize_t count;
                    OSI_NO_INTR(count = recv(fd, buffer, L2CAP_MAX_SDU_LENGTH,
                                             MSG_NOSIGNAL | MSG_DONTWAIT));
                    APPL_TRACE_DEBUG("btsock_l2cap_signaled - %d bytes received from socket",
                                     count);

                    // TODO(armansito): |buffer|, which is created above via
                    // malloc, is being cast below to UINT32 to be used as
                    // the |req_id| parameter of BTA_JvL2capWriteFixed and
                    // BTA_JvL2capWrite. The "id" then gets freed in an
                    // obscure callback elsewhere. We need to watch out for
                    // this type of unsafe practice, as this is error prone
                    // and difficult to follow.
                    if (sock->fixed_chan) {
                        if (BTA_JvL2capWriteFixed(sock->channel,
                                                  (BD_ADDR*)&sock->addr,
                                                  PTR_TO_UINT(buffer),
                                                  btsock_l2cap_cbk, buffer,
                                                  count,
                                                  UINT_TO_PTR(user_id)) != BTA_JV_SUCCESS) {
                            // On fail, free the buffer
                            on_l2cap_write_fixed_done(buffer, count, user_id);
                        }
                    } else {
                        if (BTA_JvL2capWrite(sock->handle, PTR_TO_UINT(buffer),
                                             buffer, count,
                                             UINT_TO_PTR(user_id)) != BTA_JV_SUCCESS) {
                          // On fail, free the buffer
                          on_l2cap_write_done(buffer, count, user_id);
                        }
                    }
                }
            } else
                drop_it = TRUE;
        }
        if (flags & SOCK_THREAD_FD_WR) {
            //app is ready to receive more data, tell stack to enable the data flow
            if (flush_incoming_que_on_wr_signal_l(sock) && sock->connected)
                btsock_thread_add_fd(pth, sock->our_fd, BTSOCK_L2CAP, SOCK_THREAD_FD_WR, sock->id);
        }
        if (drop_it || (flags & SOCK_THREAD_FD_EXCEPTION)) {
            int size = 0;
            if (drop_it || ioctl(sock->our_fd, FIONREAD, &size) != 0 || size == 0)
                btsock_l2cap_free_l(sock);
        }
    }
    pthread_mutex_unlock(&state_lock);
}

