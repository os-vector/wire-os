/******************************************************************************
 *  Copyright (C) 2016, The Linux Foundation. All rights reserved.
 *
 *  Not a Contribution
 ******************************************************************************/
/*****************************************************************************
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

/*****************************************************************************
 *
 *  Filename:      audio_a2dp_hw.c
 *
 *  Description:   Implements hal for bluedroid a2dp audio device
 *
 *****************************************************************************/
//#define BT_AUDIO_SYSTRACE_LOG

#ifdef BT_AUDIO_SYSTRACE_LOG
#define ATRACE_TAG ATRACE_TAG_ALWAYS
#define PERF_SYSTRACE 1
#endif

#define LOG_TAG "bt_a2dp_hw"

#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <pthread.h>
#include <stdint.h>
#include <sys/errno.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/un.h>
#include <unistd.h>
#include <cutils/sockets.h>
#include <hardware/audio.h>
#include <hardware/hardware.h>
#include <system/audio.h>

#include "audio_a2dp_hw.h"
#include "bt_utils.h"
#include "osi/include/hash_map.h"
#include "osi/include/hash_map_utils.h"
#include "osi/include/log.h"
#include "osi/include/osi.h"
#include "osi/include/socket_utils/sockets.h"

#include <dlfcn.h>

#ifdef BT_HOST_IPC_ENABLED
#include "bthost_ipc.h"
#endif

#ifdef BT_AUDIO_SYSTRACE_LOG
#include <cutils/trace.h>
#endif

//#define BT_AUDIO_SAMPLE_LOG

#ifdef BT_AUDIO_SAMPLE_LOG
FILE *outputpcmsamplefile;
char btoutputfilename [50] = "/data/audio/output_sample";
static int number =0;
#endif
/*****************************************************************************
**  Constants & Macros
******************************************************************************/

#define CTRL_CHAN_RETRY_COUNT 3
#define USEC_PER_SEC 1000000L
#define SOCK_SEND_TIMEOUT_MS 2000  /* Timeout for sending */
#define SOCK_RECV_TIMEOUT_MS 5000  /* Timeout for receiving */

// set WRITE_POLL_MS to 0 for blocking sockets, nonzero for polled non-blocking sockets
#define WRITE_POLL_MS 20

#define CASE_RETURN_STR(const) case const: return #const;

#define FNLOG()             LOG_VERBOSE("%s", __FUNCTION__);
#define DEBUG(fmt, ...)     LOG_INFO("%s: " fmt,__FUNCTION__, ## __VA_ARGS__)
#define INFO(fmt, ...)      LOG_INFO("%s: " fmt,__FUNCTION__, ## __VA_ARGS__)
#define WARN(fmt, ...)      LOG_WARN(LOG_TAG, "%s: " fmt,__FUNCTION__, ## __VA_ARGS__)
#define ERROR(fmt, ...)     LOG_ERROR("%s: " fmt,__FUNCTION__, ## __VA_ARGS__)

#define ASSERTC(cond, msg, val) if (!(cond)) {ERROR("### ASSERT : %s line %d %s (%d) ###", __FILE__, __LINE__, msg, val);}
//#define BT_HOST_IPC_PATH "/system/lib/hw/bthost-ipc.so"
/*****************************************************************************
**  Local type definitions
******************************************************************************/
struct a2dp_stream_in;
struct a2dp_stream_out;

struct a2dp_audio_device {
    struct audio_hw_device device;
    struct a2dp_stream_in  *input;
    struct a2dp_stream_out *output;
};

struct a2dp_stream_out {
    struct audio_stream_out stream;
    struct a2dp_stream_common common;
    uint64_t frames_presented; // frames written, never reset
    uint64_t frames_rendered;  // frames written, reset on standby
};

struct a2dp_stream_in {
    struct audio_stream_in  stream;
    struct a2dp_stream_common common;
};

/*****************************************************************************
**  Static variables
******************************************************************************/
#ifdef BT_HOST_IPC_ENABLED
static void *lib_handle = NULL;
bt_host_ipc_interface_t *ipc_if = NULL;
#endif
/*****************************************************************************
**  Static functions
******************************************************************************/

static size_t out_get_buffer_size(const struct audio_stream *stream);

/*****************************************************************************
**  Externs
******************************************************************************/

/*****************************************************************************
**  Functions
******************************************************************************/
/* Function used only in debug mode */
static const char* dump_a2dp_ctrl_event(char event) __attribute__ ((unused));
#ifndef BT_HOST_IPC_ENABLED
static void a2dp_open_ctrl_path(struct a2dp_stream_common *common);
#endif
/*****************************************************************************
**   Miscellaneous helper functions
******************************************************************************/

static const char* dump_a2dp_ctrl_event(char event)
{
    switch(event)
    {
        CASE_RETURN_STR(A2DP_CTRL_CMD_NONE)
        CASE_RETURN_STR(A2DP_CTRL_CMD_CHECK_READY)
        CASE_RETURN_STR(A2DP_CTRL_CMD_START)
        CASE_RETURN_STR(A2DP_CTRL_CMD_STOP)
        CASE_RETURN_STR(A2DP_CTRL_CMD_SUSPEND)
        CASE_RETURN_STR(A2DP_CTRL_CMD_CHECK_STREAM_STARTED)
        CASE_RETURN_STR(A2DP_CTRL_CMD_OFFLOAD_SUPPORTED)
        CASE_RETURN_STR(A2DP_CTRL_CMD_OFFLOAD_NOT_SUPPORTED)
        default:
            return "UNKNOWN MSG ID";
    }
}

static int calc_audiotime(struct a2dp_config cfg, int bytes)
{
    int chan_count = popcount(cfg.channel_flags);
    int bytes_per_sample = 4;

    ASSERTC(cfg.format == AUDIO_FORMAT_PCM_8_24_BIT,
            "unsupported sample sz", cfg.format);

    return (int)(((int64_t)bytes * (1000000 / (chan_count * bytes_per_sample))) / cfg.rate);
}

static void ts_error_log(char *tag, int val, int buff_size, struct a2dp_config cfg)
{
    struct timespec now;
    static struct timespec prev = {0,0};
    unsigned long long now_us;
    unsigned long long diff_us;

    clock_gettime(CLOCK_MONOTONIC, &now);

    now_us = now.tv_sec*USEC_PER_SEC + now.tv_nsec/1000;

    diff_us = (now.tv_sec - prev.tv_sec) * USEC_PER_SEC + (now.tv_nsec - prev.tv_nsec)/1000;
    prev = now;
    if(diff_us > (unsigned long long)(calc_audiotime (cfg, buff_size) + 10000L))
    {
       ERROR("[%s] ts %08lld, diff %08lld, val %d %d", tag, now_us, diff_us, val, buff_size);
    }
}

#ifndef BT_HOST_IPC_ENABLED
/* logs timestamp with microsec precision
   pprev is optional in case a dedicated diff is required */
static void ts_log(char *tag, int val, struct timespec *pprev_opt)
{
    struct timespec now;
    static struct timespec prev = {0,0};
    unsigned long long now_us;
    unsigned long long diff_us;
    UNUSED(tag);
    UNUSED(val);

    clock_gettime(CLOCK_MONOTONIC, &now);

    now_us = now.tv_sec*USEC_PER_SEC + now.tv_nsec/1000;

    if (pprev_opt)
    {
        diff_us = (now.tv_sec - prev.tv_sec) * USEC_PER_SEC + (now.tv_nsec - prev.tv_nsec)/1000;
        *pprev_opt = now;
        DEBUG("[%s] ts %08lld, *diff %08lld, val %d", tag, now_us, diff_us, val);
    }
    else
    {
        diff_us = (now.tv_sec - prev.tv_sec) * USEC_PER_SEC + (now.tv_nsec - prev.tv_nsec)/1000;
        prev = now;
        DEBUG("[%s] ts %08lld, diff %08lld, val %d", tag, now_us, diff_us, val);
    }
}


static const char* dump_a2dp_hal_state(int event)
{
    switch(event)
    {
        CASE_RETURN_STR(AUDIO_A2DP_STATE_STARTING)
        CASE_RETURN_STR(AUDIO_A2DP_STATE_STARTED)
        CASE_RETURN_STR(AUDIO_A2DP_STATE_STOPPING)
        CASE_RETURN_STR(AUDIO_A2DP_STATE_STOPPED)
        CASE_RETURN_STR(AUDIO_A2DP_STATE_SUSPENDED)
        CASE_RETURN_STR(AUDIO_A2DP_STATE_STANDBY)
        default:
            return "UNKNOWN STATE ID";
    }
}

//#ifndef BT_HOST_IPC_ENABLED
/*****************************************************************************
**
**   bluedroid stack adaptation
**
*****************************************************************************/

static int skt_connect(char *path, size_t buffer_sz)
{
    int ret;
    int skt_fd;
    struct sockaddr_un remote;
    int len;

    INFO("connect to %s (sz %zu)", path, buffer_sz);

    skt_fd = socket(AF_LOCAL, SOCK_STREAM, 0);

#ifdef ANDROID
    if(socket_local_client_connect(skt_fd, path,
            ANDROID_SOCKET_NAMESPACE_ABSTRACT, SOCK_STREAM) < 0)
#else
    memset(&remote, 0, sizeof(remote));
    remote.sun_family = AF_LOCAL;
    strncpy(remote.sun_path, path, sizeof(remote.sun_path)-1);
    if(connect(skt_fd, (struct sockaddr*)&remote, sizeof(remote)) < 0)
#endif
    {
        ERROR("failed to connect (%s)", strerror(errno));
        close(skt_fd);
        return -1;
    }

    len = buffer_sz;
    ret = setsockopt(skt_fd, SOL_SOCKET, SO_SNDBUF, (char*)&len, (int)sizeof(len));
    if (ret < 0)
        ERROR("setsockopt failed (%s)", strerror(errno));

    ret = setsockopt(skt_fd, SOL_SOCKET, SO_RCVBUF, (char*)&len, (int)sizeof(len));
    if (ret < 0)
        ERROR("setsockopt failed (%s)", strerror(errno));

    /* Socket send/receive timeout value */
    struct timeval tv;
    tv.tv_sec = SOCK_SEND_TIMEOUT_MS / 1000;
    tv.tv_usec = (SOCK_SEND_TIMEOUT_MS % 1000) * 1000;

    ret = setsockopt(skt_fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
    if (ret < 0)
        ERROR("setsockopt failed (%s)", strerror(errno));

    tv.tv_sec = SOCK_RECV_TIMEOUT_MS / 1000;
    tv.tv_usec = (SOCK_RECV_TIMEOUT_MS % 1000) * 1000;

    ret = setsockopt(skt_fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    if (ret < 0)
        ERROR("setsockopt failed (%s)", strerror(errno));

    INFO("connected to stack fd = %d", skt_fd);

    return skt_fd;
}

static int skt_read(int fd, void *p, size_t len)
{
    ssize_t read;

    FNLOG();

    ts_log("skt_read recv", len, NULL);

    OSI_NO_INTR(read = recv(fd, p, len, MSG_NOSIGNAL));
    if (read == -1)
        ERROR("read failed with errno=%d\n", errno);

    return (int)read;
}

static int skt_write(int fd, const void *p, size_t len)
{
    ssize_t sent;
    FNLOG();

    ts_log("skt_write", len, NULL);

    if (WRITE_POLL_MS == 0) {
        // do not poll, use blocking send
        OSI_NO_INTR(sent = send(fd, p, len, MSG_NOSIGNAL));
        if (sent == -1)
            ERROR("write failed with error(%s)", strerror(errno));

        return (int)sent;
    }

    // use non-blocking send, poll
    int ms_timeout = SOCK_SEND_TIMEOUT_MS;
    size_t count = 0;
    while (count < len) {
        OSI_NO_INTR(sent = send(fd, p, len - count, MSG_NOSIGNAL | MSG_DONTWAIT));
        if (sent == -1) {
            if (errno != EAGAIN && errno != EWOULDBLOCK) {
                ERROR("write failed with error(%s)", strerror(errno));
                return -1;
            }
            if (ms_timeout >= WRITE_POLL_MS) {
                usleep(WRITE_POLL_MS * 1000);
                ms_timeout -= WRITE_POLL_MS;
                continue;
            }
            WARN("write timeout exceeded, sent %zu bytes", count);
            return -1;
        }
        count += sent;
        p = (const uint8_t *)p + sent;
    }
    return (int)count;
}

static int skt_disconnect(int fd)
{
    INFO("fd %d", fd);

    if (fd != AUDIO_SKT_DISCONNECTED)
    {
        shutdown(fd, SHUT_RDWR);
        close(fd);
    }
    return 0;
}



/*****************************************************************************
**
**  AUDIO CONTROL PATH
**
*****************************************************************************/

static int a2dp_ctrl_receive(struct a2dp_stream_common *common, void* buffer, int length)
{
    ssize_t ret;
    int i;

    for (i = 0;; i++) {
        OSI_NO_INTR(ret = recv(common->ctrl_fd, buffer, length, MSG_NOSIGNAL));
        if (ret > 0) {
            break;
        }
        if (ret == 0) {
            ERROR("ack failed: peer closed");
            break;
        }
        if (errno != EWOULDBLOCK && errno != EAGAIN) {
            ERROR("ack failed: error(%s)", strerror(errno));
            break;
        }
        if (i == (CTRL_CHAN_RETRY_COUNT - 1)) {
            ERROR("ack failed: max retry count");
            break;
        }
        INFO("ack failed (%s), retrying", strerror(errno));
    }
    if (ret <= 0) {
        skt_disconnect(common->ctrl_fd);
        common->ctrl_fd = AUDIO_SKT_DISCONNECTED;
    }
    return ret;
}

static int a2dp_command(struct a2dp_stream_common *common, char cmd)
{
    char ack;

    INFO("A2DP COMMAND %s", dump_a2dp_ctrl_event(cmd));

    if (common->ctrl_fd == AUDIO_SKT_DISCONNECTED) {
        INFO("recovering from previous error");
        a2dp_open_ctrl_path(common);
        if (common->ctrl_fd == AUDIO_SKT_DISCONNECTED) {
            ERROR("failure to open ctrl path");
            return -1;
        }
    }

    /* send command */
    ssize_t sent;
    OSI_NO_INTR(sent = send(common->ctrl_fd, &cmd, 1, MSG_NOSIGNAL));
    if (sent == -1)
    {
        ERROR("cmd failed (%s)", strerror(errno));
        skt_disconnect(common->ctrl_fd);
        common->ctrl_fd = AUDIO_SKT_DISCONNECTED;
        return -1;
    }

    /* wait for ack byte */
    if (a2dp_ctrl_receive(common, &ack, 1) < 0) {
        ERROR("A2DP COMMAND %s: no ACK", dump_a2dp_ctrl_event(cmd));
        return -1;
    }

    INFO("A2DP COMMAND %s DONE STATUS %d", dump_a2dp_ctrl_event(cmd), ack);

    if (ack == A2DP_CTRL_ACK_INCALL_FAILURE)
        return ack;
    if (ack != A2DP_CTRL_ACK_SUCCESS) {
        ERROR("A2DP COMMAND %s error %d", dump_a2dp_ctrl_event(cmd), ack);
        return -1;
    }

    return 0;
}

static int check_a2dp_ready(struct a2dp_stream_common *common)
{
    INFO("state %s", dump_a2dp_hal_state(common->state));
    if (a2dp_command(common, A2DP_CTRL_CMD_CHECK_READY) < 0)
    {
        ERROR("check a2dp ready failed");
        return -1;
    }
    return 0;
}

static int a2dp_read_audio_config(struct a2dp_stream_common *common)
{
    uint32_t sample_rate;
    uint8_t channel_count;

    if (a2dp_command(common, A2DP_CTRL_GET_AUDIO_CONFIG) < 0)
    {
        ERROR("check a2dp ready failed");
        return -1;
    }

    if (a2dp_ctrl_receive(common, &sample_rate, 4) < 0)
        return -1;
    if (a2dp_ctrl_receive(common, &channel_count, 1) < 0)
        return -1;

    common->cfg.channel_flags = (channel_count == 1 ? AUDIO_CHANNEL_IN_MONO : AUDIO_CHANNEL_IN_STEREO);
    common->cfg.format = AUDIO_STREAM_DEFAULT_FORMAT;
    common->cfg.rate = sample_rate;

    INFO("got config %d %d", common->cfg.format, common->cfg.rate);

    return 0;
}

static void a2dp_open_ctrl_path(struct a2dp_stream_common *common)
{
    int i;

    /* retry logic to catch any timing variations on control channel */
    for (i = 0; i < CTRL_CHAN_RETRY_COUNT; i++)
    {
        /* connect control channel if not already connected */
        if ((common->ctrl_fd = skt_connect(A2DP_CTRL_PATH, common->buffer_sz)) > 0)
        {
            /* success, now check if stack is ready */
            if (check_a2dp_ready(common) == 0)
                break;

            ERROR("error : a2dp not ready, wait 250 ms and retry");
            usleep(250000);
            skt_disconnect(common->ctrl_fd);
            common->ctrl_fd = AUDIO_SKT_DISCONNECTED;
        }

        /* ctrl channel not ready, wait a bit */
        usleep(250000);
    }
}

static void a2dp_sink_open_ctrl_path(struct a2dp_stream_common *common)
{
    int i;

    /* retry logic to catch any timing variations on control channel */
    for (i = 0; i < CTRL_CHAN_RETRY_COUNT; i++)
    {
        /* connect control channel if not already connected */
        if ((common->ctrl_fd = skt_connect(A2DP_AVK_CTRL_PATH, common->buffer_sz)) > 0)
        {
            /* success, now check if stack is ready */
            if (check_a2dp_ready(common) == 0)
                break;

            ERROR("error : a2dp not ready, wait 250 ms and retry");
            TEMP_FAILURE_RETRY(usleep(250000));
            skt_disconnect(common->ctrl_fd);
            common->ctrl_fd = AUDIO_SKT_DISCONNECTED;
        }

        /* ctrl channel not ready, wait a bit */
        TEMP_FAILURE_RETRY(usleep(250000));
    }
}

/*****************************************************************************
**
** AUDIO DATA PATH
**
*****************************************************************************/

static void a2dp_stream_common_init(struct a2dp_stream_common *common)
{
    pthread_mutexattr_t lock_attr;

    FNLOG();

    pthread_mutexattr_init(&lock_attr);
    pthread_mutexattr_settype(&lock_attr, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(&common->lock, &lock_attr);

    common->ctrl_fd = AUDIO_SKT_DISCONNECTED;
    common->audio_fd = AUDIO_SKT_DISCONNECTED;
    common->state = AUDIO_A2DP_STATE_STOPPED;

    /* manages max capacity of socket pipe */
    common->buffer_sz = AUDIO_STREAM_OUTPUT_BUFFER_SZ;
}

static int start_audio_datapath(struct a2dp_stream_common *common)
{
    INFO("state %d", common->state);

    #ifdef BT_AUDIO_SYSTRACE_LOG
    char trace_buf[512];
    #endif

    INFO("state %s", dump_a2dp_hal_state(common->state));

    int oldstate = common->state;
    common->state = AUDIO_A2DP_STATE_STARTING;

    int a2dp_status = a2dp_command(common, A2DP_CTRL_CMD_START);
    #ifdef BT_AUDIO_SYSTRACE_LOG
    snprintf(trace_buf, 32, "start_audio_data_path:");
    if (PERF_SYSTRACE)
    {
        ATRACE_BEGIN(trace_buf);
    }
    #endif


    #ifdef BT_AUDIO_SYSTRACE_LOG
    if (PERF_SYSTRACE)
    {
        ATRACE_END();
    }
    #endif
    if (a2dp_status < 0)
    {
        ERROR("Audiopath start failed (status %d)", a2dp_status);
        goto error;
    }
    else if (a2dp_status == A2DP_CTRL_ACK_INCALL_FAILURE)
    {
        ERROR("Audiopath start failed - in call, move to suspended");
        goto error;
    }

#ifndef BTA_AV_SPLIT_A2DP_ENABLED
    /* connect socket if not yet connected */
    if (common->audio_fd == AUDIO_SKT_DISCONNECTED)
    {
        common->audio_fd = skt_connect(A2DP_DATA_PATH, common->buffer_sz);
        if (common->audio_fd < 0)
        {
            ERROR("Audiopath start failed - error opening data socket");
            goto error;
        }
        common->state = AUDIO_A2DP_STATE_STARTED;
    }
#else
    common->state = AUDIO_A2DP_STATE_STARTED;
#endif
    return 0;

error:
    common->state = oldstate;
    return -1;
}

static int start_avk_audio_datapath(struct a2dp_stream_common *common)
{
    INFO("state %d", common->state);

    #ifdef BT_AUDIO_SYSTRACE_LOG
    char trace_buf[512];
    #endif

    INFO("state %s", dump_a2dp_hal_state(common->state));

    if (common->ctrl_fd == AUDIO_SKT_DISCONNECTED) {
        INFO("%s AUDIO_SKT_DISCONNECTED", __func__);
        return -1;
    }

    int oldstate = common->state;
    common->state = AUDIO_A2DP_STATE_STARTING;

    int a2dp_status = a2dp_command(common, A2DP_CTRL_CMD_START);
    #ifdef BT_AUDIO_SYSTRACE_LOG
    snprintf(trace_buf, 32, "start_audio_data_path:");
    if (PERF_SYSTRACE)
    {
        ATRACE_BEGIN(trace_buf);
    }
    #endif


    #ifdef BT_AUDIO_SYSTRACE_LOG
    if (PERF_SYSTRACE)
    {
        ATRACE_END();
    }
    #endif
    if (a2dp_status < 0)
    {
        ERROR("%s Audiopath start failed (status %d)", __func__, a2dp_status);

        common->state = oldstate;
        return -1;
    }
    else if (a2dp_status == A2DP_CTRL_ACK_INCALL_FAILURE)
    {
        ERROR("%s Audiopath start failed - in call, move to suspended", __func__);
        common->state = oldstate;
        return -1;
    }

    /* connect socket if not yet connected */
    if (common->audio_fd == AUDIO_SKT_DISCONNECTED)
    {
        common->audio_fd = skt_connect(A2DP_AVK_DATA_PATH, common->buffer_sz);
        if (common->audio_fd < 0)
        {
            common->state = oldstate;
            return -1;
        }

        common->state = AUDIO_A2DP_STATE_STARTED;
    }

    return 0;
}

static int stop_audio_datapath(struct a2dp_stream_common *common)
{
    int oldstate = common->state;

    INFO("state %s", dump_a2dp_hal_state(common->state));

    /* prevent any stray output writes from autostarting the stream
       while stopping audiopath */
    common->state = AUDIO_A2DP_STATE_STOPPING;

    if (a2dp_command(common, A2DP_CTRL_CMD_STOP) < 0)
    {
        ERROR("audiopath stop failed");
        common->state = oldstate;
        return -1;
    }

    common->state = AUDIO_A2DP_STATE_STOPPED;

#ifndef BTA_AV_SPLIT_A2DP_ENABLED
    /* disconnect audio path */
    skt_disconnect(common->audio_fd);
    common->audio_fd = AUDIO_SKT_DISCONNECTED;
#endif

    return 0;
}

static int suspend_audio_datapath(struct a2dp_stream_common *common, bool standby)
{
    INFO("state %s", dump_a2dp_hal_state(common->state));

    if (common->state == AUDIO_A2DP_STATE_STOPPING)
        return -1;

    if (a2dp_command(common, A2DP_CTRL_CMD_SUSPEND) < 0)
        return -1;

    if (standby)
        common->state = AUDIO_A2DP_STATE_STANDBY;
    else
        common->state = AUDIO_A2DP_STATE_SUSPENDED;

#ifndef BTA_AV_SPLIT_A2DP_ENABLED
    /* disconnect audio path */
    skt_disconnect(common->audio_fd);

    common->audio_fd = AUDIO_SKT_DISCONNECTED;
#endif

    return 0;
}


static int check_a2dp_stream_started(struct a2dp_stream_out *out)
{
   if (a2dp_command(&out->common, A2DP_CTRL_CMD_CHECK_STREAM_STARTED) < 0)
   {
       INFO("Btif not in stream state");
       return -1;
   }
   return 0;
}
#endif

/*****************************************************************************
**
**  audio output callbacks
**
*****************************************************************************/

static ssize_t out_write(struct audio_stream_out *stream, const void* buffer,
                         size_t bytes)
{
    struct a2dp_stream_out *out = (struct a2dp_stream_out *)stream;
    int sent = -1;
    #ifdef BT_AUDIO_SYSTRACE_LOG
    char trace_buf[512];
    #endif

    DEBUG("write %zu bytes (fd %d)", bytes, out->common.audio_fd);

    pthread_mutex_lock(&out->common.lock);
    if (out->common.state == AUDIO_A2DP_STATE_SUSPENDED ||
            out->common.state == AUDIO_A2DP_STATE_STOPPING) {
        DEBUG("stream suspended or closing");
        goto finish;
    }

    /* only allow autostarting if we are in stopped or standby */
    if ((out->common.state == AUDIO_A2DP_STATE_STOPPED) ||
        (out->common.state == AUDIO_A2DP_STATE_STANDBY))
    {
#ifdef BT_HOST_IPC_ENABLED
        if (ipc_if->start_audio_datapath(&out->common) < 0)
#else
        if (start_audio_datapath(&out->common) < 0)
#endif
        {
            goto finish;
        }
    }
    else if (out->common.state != AUDIO_A2DP_STATE_STARTED)
    {
        ERROR("stream not in stopped or standby");
        goto finish;
    }
    #ifdef BT_AUDIO_SAMPLE_LOG
    if (outputpcmsamplefile)
    {
        fwrite (buffer,1,bytes,outputpcmsamplefile);
    }
    #endif

    ts_error_log("a2dp_out_write", bytes, out->common.buffer_sz, out->common.cfg);

    pthread_mutex_unlock(&out->common.lock);

    #ifdef BT_AUDIO_SYSTRACE_LOG
    snprintf(trace_buf, 32, "out_write:");
    if (PERF_SYSTRACE)
    {
        ATRACE_BEGIN(trace_buf);
    }
    #endif

#ifdef BT_HOST_IPC_ENABLED
    sent = ipc_if->skt_write(out->common.audio_fd, buffer,  bytes);
#else
    sent = skt_write(out->common.audio_fd, buffer,  bytes);
#endif
    pthread_mutex_lock(&out->common.lock);

    #ifdef BT_AUDIO_SYSTRACE_LOG
    if (PERF_SYSTRACE)
    {
        ATRACE_END();
    }
    #endif

    if (sent == -1)
    {
#ifdef BT_HOST_IPC_ENABLED
        ipc_if->skt_disconnect(out->common.audio_fd);
#else
        skt_disconnect(out->common.audio_fd);
#endif
        out->common.audio_fd = AUDIO_SKT_DISCONNECTED;
        if ((out->common.state != AUDIO_A2DP_STATE_SUSPENDED) &&
                (out->common.state != AUDIO_A2DP_STATE_STOPPING)) {
            out->common.state = AUDIO_A2DP_STATE_STOPPED;
        } else {
            ERROR("write failed : stream suspended, avoid resetting state");
        }
        goto finish;
    }

finish: ;
    const size_t frames = bytes / audio_stream_out_frame_size(stream);
    out->frames_rendered += frames;
    out->frames_presented += frames;
    pthread_mutex_unlock(&out->common.lock);

    // If send didn't work out, sleep to emulate write delay.
    if (sent == -1) {
        const int us_delay = calc_audiotime(out->common.cfg, bytes);
        DEBUG("emulate a2dp write delay (%d us)", us_delay);
        usleep(us_delay);
    }
    return bytes;
}

static uint32_t out_get_sample_rate(const struct audio_stream *stream)
{
    struct a2dp_stream_out *out = (struct a2dp_stream_out *)stream;

    INFO("rate %" PRIu32,out->common.cfg.rate);

    return out->common.cfg.rate;
}

static int out_set_sample_rate(struct audio_stream *stream, uint32_t rate)
{
    struct a2dp_stream_out *out = (struct a2dp_stream_out *)stream;

    INFO("out_set_sample_rate : %" PRIu32, rate);

    if (rate != AUDIO_STREAM_DEFAULT_RATE)
    {
        ERROR("only rate %d supported", AUDIO_STREAM_DEFAULT_RATE);
        return -1;
    }

    out->common.cfg.rate = rate;

    return 0;
}

static size_t out_get_buffer_size(const struct audio_stream *stream)
{
    struct a2dp_stream_out *out = (struct a2dp_stream_out *)stream;
    // period_size is the AudioFlinger mixer buffer size.
    const size_t period_size = out->common.buffer_sz / AUDIO_STREAM_OUTPUT_BUFFER_PERIODS;
    const size_t mixer_unit_size = 16 /* frames */ * 4 /* framesize */;

    INFO("socket buffer size: %zu  period size: %zu", out->common.buffer_sz, period_size);
    if (period_size % mixer_unit_size != 0) {
        ERROR("period size %zu not a multiple of %zu", period_size, mixer_unit_size);
    }

    return period_size;
}

static uint32_t out_get_channels(const struct audio_stream *stream)
{
    struct a2dp_stream_out *out = (struct a2dp_stream_out *)stream;

    INFO("channels 0x%" PRIx32, out->common.cfg.channel_flags);

    return out->common.cfg.channel_flags;
}

static audio_format_t out_get_format(const struct audio_stream *stream)
{
    struct a2dp_stream_out *out = (struct a2dp_stream_out *)stream;
    INFO("format 0x%x", out->common.cfg.format);
    return out->common.cfg.format;
}

static int out_set_format(struct audio_stream *stream, audio_format_t format)
{
    UNUSED(stream);
    UNUSED(format);
    INFO("setting format not yet supported (0x%x)", format);
    return -ENOSYS;
}

static int out_standby(struct audio_stream *stream)
{
    struct a2dp_stream_out *out = (struct a2dp_stream_out *)stream;
    int retVal = 0;

    FNLOG();

    pthread_mutex_lock(&out->common.lock);
    // Do nothing in SUSPENDED state.
    if (out->common.state != AUDIO_A2DP_STATE_SUSPENDED)
#ifdef BT_HOST_IPC_ENABLED
        retVal =  ipc_if->suspend_audio_datapath(&out->common, true);
#else
        retVal =  suspend_audio_datapath(&out->common, true);
#endif
    out->frames_rendered = 0; // rendered is reset, presented is not
    pthread_mutex_unlock (&out->common.lock);

    return retVal;
}

static int out_dump(const struct audio_stream *stream, int fd)
{
    UNUSED(stream);
    UNUSED(fd);
    FNLOG();
    return 0;
}

static int out_set_parameters(struct audio_stream *stream, const char *kvpairs)
{
    struct a2dp_stream_out *out = (struct a2dp_stream_out *)stream;

    INFO("out_set_parameters: state %d", out->common.state);

    hash_map_t *params = hash_map_utils_new_from_string_params(kvpairs);
    int status = 0;
    char *keyval;
    if (!params)
      return status;

    /* dump params */
    hash_map_utils_dump_string_keys_string_values(params);

#ifdef BTA_AV_SPLIT_A2DP_ENABLED
    keyval = (char *)hash_map_get(params, "A2dpStarted");

    if (keyval != NULL)
    {
        INFO("out_set_parameters, param: A2dpStarted");
        if (strcmp(keyval, "true") == 0)
        {
            INFO("out_set_parameters, value: true");
            pthread_mutex_lock(&out->common.lock);
            if (out->common.state == AUDIO_A2DP_STATE_SUSPENDED)
            {
                INFO("stream suspended");
                status = -1;
            }
            else if ((out->common.state == AUDIO_A2DP_STATE_STOPPED) ||
                (out->common.state == AUDIO_A2DP_STATE_STANDBY))
            {
#ifdef BT_HOST_IPC_ENABLED
                if (ipc_if->start_audio_datapath(&out->common) < 0)
#else
                if (start_audio_datapath(&out->common) < 0)
#endif
                {
                    INFO("stream start failed");
                    status = -1;
                }
            }
            else if (out->common.state != AUDIO_A2DP_STATE_STARTED)
            {
                ERROR("stream not in stopped or standby");
                status = -1;
            }
            pthread_mutex_unlock(&out->common.lock);
            INFO("stream start completes with status: %d", status);
        }
        else if (strcmp(keyval, "false") == 0)
        {
            INFO("out_set_parameters, value: false");
            pthread_mutex_lock(&out->common.lock);
            if (out->common.state != AUDIO_A2DP_STATE_SUSPENDED)
#ifdef BT_HOST_IPC_ENABLED
                status = ipc_if->suspend_audio_datapath(&out->common, true);
#else
                status = suspend_audio_datapath(&out->common, true);
#endif
            else
            {
                ERROR("stream alreday suspended");
            }
            pthread_mutex_unlock(&out->common.lock);
            INFO("stream stop completes with status: %d", status);
        }
    }
#endif

    keyval = (char *)hash_map_get(params, "closing");

    if (keyval && strcmp(keyval, "true") == 0)
    {
        DEBUG("stream closing, disallow any writes");
            pthread_mutex_lock(&out->common.lock);
        out->common.state = AUDIO_A2DP_STATE_STOPPING;
            pthread_mutex_unlock(&out->common.lock);
    }

    keyval = (char *)hash_map_get(params, "A2dpSuspended");

    if (keyval)
    {
        if (strcmp(keyval, "true") == 0) 
        {
            pthread_mutex_lock(&out->common.lock);
            if (out->common.state == AUDIO_A2DP_STATE_STARTED)
#ifdef BT_HOST_IPC_ENABLED
                status = ipc_if->suspend_audio_datapath(&out->common, false);
#else
                status = suspend_audio_datapath(&out->common, false);
#endif
            else
            {
#ifdef BT_HOST_IPC_ENABLED
                if (ipc_if->check_a2dp_stream_started(&out->common) == 0)
#else
                if (check_a2dp_stream_started(out) == 0)
#endif
                   /*Btif and A2dp HAL state can be out of sync
                    *check state of btif and suspend audio.
                    *Happens when remote initiates start.*/
#ifdef BT_HOST_IPC_ENABLED
                    status = ipc_if->suspend_audio_datapath(&out->common, false);
#else
                    status = suspend_audio_datapath(&out->common, false);
#endif
                else
                    out->common.state = AUDIO_A2DP_STATE_SUSPENDED;
            }
            pthread_mutex_unlock(&out->common.lock);
        }
        else
        {
            pthread_mutex_lock(&out->common.lock);
            /* Do not start the streaming automatically. If the phone was streaming
             * prior to being suspended, the next out_write shall trigger the
             * AVDTP start procedure */
            if (out->common.state == AUDIO_A2DP_STATE_SUSPENDED)
                out->common.state = AUDIO_A2DP_STATE_STANDBY;
            /* Irrespective of the state, return 0 */
            pthread_mutex_unlock(&out->common.lock);
        }
    }

    hash_map_free(params);

    return status;
}

static char * out_get_parameters(const struct audio_stream *stream, const char *keys)
{
    UNUSED(stream);
    UNUSED(keys);
    FNLOG();

    /* add populating param here */

    return strdup("");
}

static uint32_t out_get_latency(const struct audio_stream_out *stream)
{
    int latency_us;

    struct a2dp_stream_out *out = (struct a2dp_stream_out *)stream;

    FNLOG();

    latency_us = ((out->common.buffer_sz * 1000 ) /
                    audio_stream_out_frame_size(&out->stream) /
                    out->common.cfg.rate) * 1000;


    return (latency_us / 1000) + 200;
}

static int out_set_volume(struct audio_stream_out *stream, float left,
                          float right)
{
    UNUSED(stream);
    UNUSED(left);
    UNUSED(right);

    FNLOG();

    /* volume controlled in audioflinger mixer (digital) */

    return -ENOSYS;
}

static int out_get_presentation_position(const struct audio_stream_out *stream,
                                         uint64_t *frames, struct timespec *timestamp)
{
    struct a2dp_stream_out *out = (struct a2dp_stream_out *)stream;

    FNLOG();
    if (stream == NULL || frames == NULL || timestamp == NULL)
        return -EINVAL;

    int ret = -EWOULDBLOCK;
    pthread_mutex_lock(&out->common.lock);
    uint64_t latency_frames = (uint64_t)out_get_latency(stream) * out->common.cfg.rate / 1000;
    if (out->frames_presented >= latency_frames) {
        *frames = out->frames_presented - latency_frames;
        clock_gettime(CLOCK_MONOTONIC, timestamp); // could also be associated with out_write().
        ret = 0;
    }
    pthread_mutex_unlock(&out->common.lock);
    return ret;
}

static int out_get_render_position(const struct audio_stream_out *stream,
                                   uint32_t *dsp_frames)
{
    struct a2dp_stream_out *out = (struct a2dp_stream_out *)stream;

    FNLOG();
    if (stream == NULL || dsp_frames == NULL)
        return -EINVAL;

    pthread_mutex_lock(&out->common.lock);
    uint64_t latency_frames = (uint64_t)out_get_latency(stream) * out->common.cfg.rate / 1000;
    if (out->frames_rendered >= latency_frames) {
        *dsp_frames = (uint32_t)(out->frames_rendered - latency_frames);
    } else {
        *dsp_frames = 0;
    }
    pthread_mutex_unlock(&out->common.lock);
    return 0;
}

static int out_add_audio_effect(const struct audio_stream *stream, effect_handle_t effect)
{
    UNUSED(stream);
    UNUSED(effect);

    FNLOG();
    return 0;
}

static int out_remove_audio_effect(const struct audio_stream *stream, effect_handle_t effect)
{
    UNUSED(stream);
    UNUSED(effect);

    FNLOG();
    return 0;
}

/*
 * AUDIO INPUT STREAM
 */

static uint32_t in_get_sample_rate(const struct audio_stream *stream)
{
    struct a2dp_stream_in *in = (struct a2dp_stream_in *)stream;

    FNLOG();
    return in->common.cfg.rate;
}

static int in_set_sample_rate(struct audio_stream *stream, uint32_t rate)
{
    struct a2dp_stream_in *in = (struct a2dp_stream_in *)stream;

    FNLOG();

    if (in->common.cfg.rate > 0 && in->common.cfg.rate == rate)
        return 0;
    else
        return -1;
}

static size_t in_get_buffer_size(const struct audio_stream *stream)
{
    UNUSED(stream);

    FNLOG();
    return 320;
}

static uint32_t in_get_channels(const struct audio_stream *stream)
{
    struct a2dp_stream_in *in = (struct a2dp_stream_in *)stream;

    FNLOG();
    return in->common.cfg.channel_flags;
}

static audio_format_t in_get_format(const struct audio_stream *stream)
{
    UNUSED(stream);

    FNLOG();
    return AUDIO_FORMAT_PCM_16_BIT;
}

static int in_set_format(struct audio_stream *stream, audio_format_t format)
{
    UNUSED(stream);
    UNUSED(format);

    FNLOG();
    if (format == AUDIO_FORMAT_PCM_16_BIT)
        return 0;
    else
        return -1;
}

static int in_standby(struct audio_stream *stream)
{
    struct a2dp_stream_in *in = (struct a2dp_stream_in *)stream;
    int retVal = 0;

    FNLOG();

    pthread_mutex_lock(&in->common.lock);

    /*Need not check State here as btif layer does
    check of btif state , during remote initited suspend
    DUT need to clear flag else start will not happen but
    Do nothing in SUSPENDED state. */
    if (in->common.state != AUDIO_A2DP_STATE_SUSPENDED)
        retVal = suspend_audio_datapath(&in->common, true);
    pthread_mutex_unlock (&in->common.lock);

    return retVal;
    return 0;
}

static int in_dump(const struct audio_stream *stream, int fd)
{
    UNUSED(stream);
    UNUSED(fd);

    FNLOG();
    return 0;
}

static int in_set_parameters(struct audio_stream *stream, const char *kvpairs)
{
    UNUSED(stream);
    UNUSED(kvpairs);

    FNLOG();
    return 0;
}

static char * in_get_parameters(const struct audio_stream *stream,
                                const char *keys)
{
    UNUSED(stream);
    UNUSED(keys);

    FNLOG();
    return strdup("");
}

static int in_set_gain(struct audio_stream_in *stream, float gain)
{
    UNUSED(stream);
    UNUSED(gain);

    FNLOG();
    return 0;
}

static ssize_t in_read(struct audio_stream_in *stream, void* buffer,
                       size_t bytes)
{
    struct a2dp_stream_in *in = (struct a2dp_stream_in *)stream;
    int read;
    int us_delay;

    DEBUG("read %zu bytes, state: %d", bytes, in->common.state);

    pthread_mutex_lock(&in->common.lock);
    if (in->common.state == AUDIO_A2DP_STATE_SUSPENDED ||
            in->common.state == AUDIO_A2DP_STATE_STOPPING)
    {
        DEBUG("stream suspended");
        goto error;
    }

    /* only allow autostarting if we are in stopped or standby */
    if ((in->common.state == AUDIO_A2DP_STATE_STOPPED) ||
        (in->common.state == AUDIO_A2DP_STATE_STANDBY))
    {
#ifdef BT_HOST_IPC_ENABLED
        if (ipc_if->start_audio_datapath(&in->common) < 0)
#else
        if (start_avk_audio_datapath(&in->common) < 0)
#endif
        {
            goto error;
        }
    }
    else if (in->common.state != AUDIO_A2DP_STATE_STARTED)
    {
        ERROR("stream not in stopped or standby");
        goto error;
    }

    pthread_mutex_unlock(&in->common.lock);
#ifdef BT_HOST_IPC_ENABLED
    read = ipc_if->skt_read(in->common.audio_fd, buffer, bytes);
#else
    read = skt_read(in->common.audio_fd, buffer, bytes);
#endif
    pthread_mutex_lock(&in->common.lock);

    if (read == -1)
    {
#ifdef BT_HOST_IPC_ENABLED
        ipc_if->skt_disconnect(in->common.audio_fd);
#else
        skt_disconnect(in->common.audio_fd);
#endif
        in->common.audio_fd = AUDIO_SKT_DISCONNECTED;
        if ((in->common.state != AUDIO_A2DP_STATE_SUSPENDED) &&
                (in->common.state != AUDIO_A2DP_STATE_STOPPING)) {
            in->common.state = AUDIO_A2DP_STATE_STOPPED;
        } else {
            ERROR("read failed : stream suspended, avoid resetting state");
        }
        goto error;
    } else if (read == 0) {
        DEBUG("read time out - return zeros");
        memset(buffer, 0, bytes);
        read = bytes;
    }
    pthread_mutex_unlock(&in->common.lock);

    DEBUG("read %d bytes out of %zu bytes", read, bytes);
    return read;

error:
    pthread_mutex_unlock(&in->common.lock);
    memset(buffer, 0, bytes);
    us_delay = calc_audiotime(in->common.cfg, bytes);
    DEBUG("emulate a2dp read delay (%d us)", us_delay);

    usleep(us_delay);
    return bytes;
}

static uint32_t in_get_input_frames_lost(struct audio_stream_in *stream)
{
    UNUSED(stream);

    FNLOG();
    return 0;
}

static int in_add_audio_effect(const struct audio_stream *stream, effect_handle_t effect)
{
    UNUSED(stream);
    UNUSED(effect);

    FNLOG();
    return 0;
}

static int in_remove_audio_effect(const struct audio_stream *stream, effect_handle_t effect)
{
    UNUSED(stream);
    UNUSED(effect);

    FNLOG();

    return 0;
}

static int adev_open_output_stream(struct audio_hw_device *dev,
                                   audio_io_handle_t handle,
                                   audio_devices_t devices,
                                   audio_output_flags_t flags,
                                   struct audio_config *config,
                                   struct audio_stream_out **stream_out,
                                   const char *address)

{
    struct a2dp_audio_device *a2dp_dev = (struct a2dp_audio_device *)dev;
    struct a2dp_stream_out *out;
    int ret = 0;
    UNUSED(address);
    UNUSED(handle);
    UNUSED(devices);
    UNUSED(flags);

    INFO("opening output");

    out = (struct a2dp_stream_out *)calloc(1, sizeof(struct a2dp_stream_out));

    if (!out)
        return -ENOMEM;
    #ifdef BT_AUDIO_SAMPLE_LOG
    snprintf(btoutputfilename, sizeof(btoutputfilename), "%s%d%s", btoutputfilename, number,".pcm");
    outputpcmsamplefile = fopen (btoutputfilename, "ab");
    number++;
    #endif

#ifdef BT_HOST_IPC_ENABLED
    lib_handle = dlopen("libbthost_if.so", RTLD_NOW);
    if (!lib_handle)
    {
        INFO("Failed to load bthost-ipc library %s",dlerror());
        ret = -1;
        goto err_open;
    }
    else
    {
        ipc_if = (bt_host_ipc_interface_t*) dlsym(lib_handle,"BTHOST_IPC_INTERFACE");
        if (!ipc_if)
        {
            ERROR("Failed to load BT IPC library symbol");
            ret  = -1;
            goto err_open;
        }
    }
#endif
    out->stream.common.get_sample_rate = out_get_sample_rate;
    out->stream.common.set_sample_rate = out_set_sample_rate;
    out->stream.common.get_buffer_size = out_get_buffer_size;
    out->stream.common.get_channels = out_get_channels;
    out->stream.common.get_format = out_get_format;
    out->stream.common.set_format = out_set_format;
    out->stream.common.standby = out_standby;
    out->stream.common.dump = out_dump;
    out->stream.common.set_parameters = out_set_parameters;
    out->stream.common.get_parameters = out_get_parameters;
    out->stream.common.add_audio_effect = out_add_audio_effect;
    out->stream.common.remove_audio_effect = out_remove_audio_effect;
    out->stream.get_latency = out_get_latency;
    out->stream.set_volume = out_set_volume;
    out->stream.write = out_write;
    out->stream.get_render_position = out_get_render_position;
    out->stream.get_presentation_position = out_get_presentation_position;


    /* initialize a2dp specifics */
#ifdef BT_HOST_IPC_ENABLED
    ipc_if->a2dp_stream_common_init(&out->common);
#else
    a2dp_stream_common_init(&out->common);
#endif
    out->common.cfg.channel_flags = AUDIO_STREAM_DEFAULT_CHANNEL_FLAG;
    out->common.cfg.format = AUDIO_FORMAT_PCM_8_24_BIT;
    out->common.cfg.rate = AUDIO_STREAM_DEFAULT_RATE;

   /* set output config values */
   if (config)
   {
      config->format = out_get_format((const struct audio_stream *)&out->stream);
      config->sample_rate = out_get_sample_rate((const struct audio_stream *)&out->stream);
      config->channel_mask = out_get_channels((const struct audio_stream *)&out->stream);
   }
    *stream_out = &out->stream;
    a2dp_dev->output = out;

#ifdef BT_HOST_IPC_ENABLED
    ipc_if->a2dp_open_ctrl_path(&out->common);
#else
    a2dp_open_ctrl_path(&out->common);
#endif
    if (out->common.ctrl_fd == AUDIO_SKT_DISCONNECTED)
    {
        ERROR("ctrl socket failed to connect (%s)", strerror(errno));
        ret = -1;
        goto err_open;
    }

#ifdef BT_HOST_IPC_ENABLED
    if (ipc_if->a2dp_command(&out->common, A2DP_CTRL_CMD_OFFLOAD_NOT_SUPPORTED) == 0) {
#else
    if (a2dp_command(&out->common, A2DP_CTRL_CMD_OFFLOAD_NOT_SUPPORTED) == 0) {
#endif
        DEBUG("Streaming mode set successfully");
    }
    INFO("success");
    /* Delay to ensure Headset is in proper state when START is initiated
       from DUT immediately after the connection due to ongoing music playback. */
    usleep(1000000);
    return 0;

err_open:
    free(out);
    *stream_out = NULL;
    a2dp_dev->output = NULL;
    ERROR("failed");
    return ret;
}

static void adev_close_output_stream(struct audio_hw_device *dev,
                                     struct audio_stream_out *stream)
{
    struct a2dp_audio_device *a2dp_dev = (struct a2dp_audio_device *)dev;
    struct a2dp_stream_out *out = (struct a2dp_stream_out *)stream;

    INFO("closing output (state %d)", out->common.state);

    pthread_mutex_lock(&out->common.lock);
    if ((out->common.state == AUDIO_A2DP_STATE_STARTED) ||
            (out->common.state == AUDIO_A2DP_STATE_STOPPING))
#ifdef BT_HOST_IPC_ENABLED
        ipc_if->stop_audio_datapath(&out->common);
#else
        stop_audio_datapath(&out->common);
#endif

    #ifdef BT_AUDIO_SAMPLE_LOG
    ALOGV("close file output");
    fclose (outputpcmsamplefile);
    #endif

#ifdef BT_HOST_IPC_ENABLED
    ipc_if->skt_disconnect(out->common.ctrl_fd);
#else
    skt_disconnect(out->common.ctrl_fd);
#endif
    out->common.ctrl_fd = AUDIO_SKT_DISCONNECTED;
#ifdef BT_HOST_IPC_ENABLED
    if (lib_handle)
        dlclose(lib_handle);
#endif
    free(stream);
    a2dp_dev->output = NULL;
    pthread_mutex_unlock(&out->common.lock);

    INFO("done");
}

static int adev_set_parameters(struct audio_hw_device *dev, const char *kvpairs)
{
    struct a2dp_audio_device *a2dp_dev = (struct a2dp_audio_device *)dev;
    struct a2dp_stream_out *out = a2dp_dev->output;
    int retval = 0;

    if (out == NULL)
        return retval;



    retval = out->stream.common.set_parameters((struct audio_stream *)out, kvpairs);

    return retval;
}

static char * adev_get_parameters(const struct audio_hw_device *dev,
                                  const char *keys)
{
    UNUSED(dev);

    FNLOG();

    hash_map_t *params = hash_map_utils_new_from_string_params(keys);
    hash_map_utils_dump_string_keys_string_values(params);
    hash_map_free(params);

    return strdup("");
}

static int adev_init_check(const struct audio_hw_device *dev)
{
    UNUSED(dev);
    FNLOG();

    return 0;
}

static int adev_set_voice_volume(struct audio_hw_device *dev, float volume)
{
    UNUSED(dev);
    UNUSED(volume);

    FNLOG();

    return -ENOSYS;
}

static int adev_set_master_volume(struct audio_hw_device *dev, float volume)
{
    UNUSED(dev);
    UNUSED(volume);

    FNLOG();

    return -ENOSYS;
}

static int adev_set_mode(struct audio_hw_device *dev, int mode)
{
    UNUSED(dev);
    UNUSED(mode);

    FNLOG();

    return 0;
}

static int adev_set_mic_mute(struct audio_hw_device *dev, bool state)
{
    UNUSED(dev);
    UNUSED(state);

    FNLOG();

    return -ENOSYS;
}

static int adev_get_mic_mute(const struct audio_hw_device *dev, bool *state)
{
    UNUSED(dev);
    UNUSED(state);

    FNLOG();

    return -ENOSYS;
}

static size_t adev_get_input_buffer_size(const struct audio_hw_device *dev,
                                         const struct audio_config *config)
{
    UNUSED(dev);
    UNUSED(config);

    FNLOG();

    return 320;
}

static int adev_open_input_stream(struct audio_hw_device *dev,
                                  audio_io_handle_t handle,
                                  audio_devices_t devices,
                                  struct audio_config *config,
                                  struct audio_stream_in **stream_in,
                                  audio_input_flags_t flags,
                                  const char *address,
                                  audio_source_t source)
{
    struct a2dp_audio_device *a2dp_dev = (struct a2dp_audio_device *)dev;
    struct a2dp_stream_in *in;
    int ret;
    UNUSED(address);
    UNUSED(config);
    UNUSED(devices);
    UNUSED(flags);
    UNUSED(handle);
    UNUSED(source);

    FNLOG();

    in = (struct a2dp_stream_in *)calloc(1, sizeof(struct a2dp_stream_in));

    if (!in)
        return -ENOMEM;

#ifdef BT_HOST_IPC_ENABLED
    lib_handle = dlopen("libbthost_if.so", RTLD_NOW);
    if (!lib_handle)
    {
        INFO("Failed to load bthost-ipc library %s",dlerror());
        ret = -1;
        goto err_open;
    }
    else
    {
        ipc_if = (bt_host_ipc_interface_t*) dlsym(lib_handle,"BTHOST_IPC_INTERFACE");
        if (!ipc_if)
        {
            ERROR("Failed to load BT IPC library symbol");
            ret =  -1;
            goto err_open;
        }
    }
#endif
    in->stream.common.get_sample_rate = in_get_sample_rate;
    in->stream.common.set_sample_rate = in_set_sample_rate;
    in->stream.common.get_buffer_size = in_get_buffer_size;
    in->stream.common.get_channels = in_get_channels;
    in->stream.common.get_format = in_get_format;
    in->stream.common.set_format = in_set_format;
    in->stream.common.standby = in_standby;
    in->stream.common.dump = in_dump;
    in->stream.common.set_parameters = in_set_parameters;
    in->stream.common.get_parameters = in_get_parameters;
    in->stream.common.add_audio_effect = in_add_audio_effect;
    in->stream.common.remove_audio_effect = in_remove_audio_effect;
    in->stream.set_gain = in_set_gain;
    in->stream.read = in_read;
    in->stream.get_input_frames_lost = in_get_input_frames_lost;

    /* initialize a2dp specifics */
#ifdef BT_HOST_IPC_ENABLED
    ipc_if->a2dp_stream_common_init(&in->common);
#else
    a2dp_stream_common_init(&in->common);
#endif
    *stream_in = &in->stream;
    a2dp_dev->input = in;

#ifdef BT_HOST_IPC_ENABLED
    ipc_if->a2dp_open_ctrl_path(&in->common);
#else
    a2dp_sink_open_ctrl_path(&in->common);
#endif
    if (in->common.ctrl_fd == AUDIO_SKT_DISCONNECTED)
    {
        ERROR("ctrl socket failed to connect (%s)", strerror(errno));
        ret = -1;
        goto err_open;
    }

#ifdef BT_HOST_IPC_ENABLED
    if (ipc_if->a2dp_command(&in->common, A2DP_CTRL_CMD_OFFLOAD_NOT_SUPPORTED) == 0) {
#else
    if (a2dp_command(&in->common, A2DP_CTRL_CMD_OFFLOAD_NOT_SUPPORTED) == 0) {
#endif
        DEBUG("Streaming mode set successfully");
    }
#ifdef BT_HOST_IPC_ENABLED
    if (ipc_if->a2dp_read_audio_config(&in->common) < 0) {
#else
    if (a2dp_read_audio_config(&in->common) < 0) {
#endif
        ERROR("a2dp_read_audio_config failed (%s)", strerror(errno));
        ret = -1;
        goto err_open;
    }

    DEBUG("success");
    return 0;

err_open:
    free(in);
    *stream_in = NULL;
    a2dp_dev->input = NULL;
    ERROR("failed");
    return ret;
}

static void adev_close_input_stream(struct audio_hw_device *dev,
                                   struct audio_stream_in *stream)
{
    struct a2dp_audio_device *a2dp_dev = (struct a2dp_audio_device *)dev;
    struct a2dp_stream_in* in = (struct a2dp_stream_in *)stream;
    a2dp_state_t state = in->common.state;

    INFO("closing input (state %d)", state);

    if ((state == AUDIO_A2DP_STATE_STARTED) || (state == AUDIO_A2DP_STATE_STOPPING))
#ifdef BT_HOST_IPC_ENABLED
        ipc_if->stop_audio_datapath(&in->common);
#else
        stop_audio_datapath(&in->common);
#endif

#ifdef BT_HOST_IPC_ENABLED
    ipc_if->skt_disconnect(in->common.ctrl_fd);
#else
    skt_disconnect(in->common.ctrl_fd);
#endif
    in->common.ctrl_fd = AUDIO_SKT_DISCONNECTED;
    free(stream);
    a2dp_dev->input = NULL;
#ifdef BT_HOST_IPC_ENABLED
    if (lib_handle)
        dlclose(lib_handle);
#endif
    DEBUG("done");
}

static int adev_dump(const audio_hw_device_t *device, int fd)
{
    UNUSED(device);
    UNUSED(fd);

    FNLOG();

    return 0;
}

static int adev_close(hw_device_t *device)
{
    FNLOG();

    free(device);
    return 0;
}

static int adev_open(const hw_module_t* module, const char* name,
                     hw_device_t** device)
{
    struct a2dp_audio_device *adev;

    INFO(" adev_open in A2dp_hw module");
    FNLOG();

    if (strcmp(name, AUDIO_HARDWARE_INTERFACE) != 0)
    {
        ERROR("interface %s not matching [%s]", name, AUDIO_HARDWARE_INTERFACE);
        return -EINVAL;
    }

    adev = calloc(1, sizeof(struct a2dp_audio_device));

    if (!adev)
        return -ENOMEM;

    adev->device.common.tag = HARDWARE_DEVICE_TAG;
    adev->device.common.version = AUDIO_DEVICE_API_VERSION_2_0;
    adev->device.common.module = (struct hw_module_t *) module;
    adev->device.common.close = adev_close;

    adev->device.init_check = adev_init_check;
    adev->device.set_voice_volume = adev_set_voice_volume;
    adev->device.set_master_volume = adev_set_master_volume;
    adev->device.set_mode = adev_set_mode;
    adev->device.set_mic_mute = adev_set_mic_mute;
    adev->device.get_mic_mute = adev_get_mic_mute;
    adev->device.set_parameters = adev_set_parameters;
    adev->device.get_parameters = adev_get_parameters;
    adev->device.get_input_buffer_size = adev_get_input_buffer_size;
    adev->device.open_output_stream = adev_open_output_stream;
    adev->device.close_output_stream = adev_close_output_stream;
    adev->device.open_input_stream = adev_open_input_stream;
    adev->device.close_input_stream = adev_close_input_stream;
    adev->device.dump = adev_dump;

    adev->output = NULL;


    *device = &adev->device.common;

    return 0;
}

static struct hw_module_methods_t hal_module_methods = {
    .open = adev_open,
};

__attribute__ ((visibility ("default")))
struct audio_module HAL_MODULE_INFO_SYM = {
    .common = {
        .tag = HARDWARE_MODULE_TAG,
        .version_major = 1,
        .version_minor = 0,
        .id = AUDIO_HARDWARE_MODULE_ID,
        .name = "A2DP Audio HW HAL",
        .author = "The Android Open Source Project",
        .methods = &hal_module_methods,
    },
};
