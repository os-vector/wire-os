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

#define LOG_TAG "bt_btif_sock"

#include "btif_sock_util.h"

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>

#include <hardware/bluetooth.h>
#include <hardware/bt_sock.h>

#include "bt_target.h"
#include "bta_api.h"
#include "bta_jv_api.h"
#include "bta_jv_co.h"
#include "btif_common.h"
#include "btif_sock_sdp.h"
#include "btif_sock_thread.h"
#include "btif_util.h"
#include "btm_api.h"
#include "btm_int.h"
#include "btu.h"
#include "bt_common.h"
#include "hcimsgs.h"
#include "osi/include/log.h"
#include "port_api.h"
#include "sdp_api.h"

#define asrt(s) if(!(s)) BTIF_TRACE_ERROR("## %s assert %s failed at line:%d ##",__FUNCTION__, #s, __LINE__)

int sock_send_all(int sock_fd, const uint8_t* buf, int len)
{
    int s = len;

    while(s)
    {
        ssize_t ret;
        OSI_NO_INTR(ret = send(sock_fd, buf, s, 0));
        if(ret <= 0)
        {
            BTIF_TRACE_ERROR("sock fd:%d send errno:%d, ret:%d", sock_fd, errno, ret);
            return -1;
        }
        buf += ret;
        s -= ret;
    }
    return len;
}
int sock_recv_all(int sock_fd, uint8_t* buf, int len)
{
    int r = len;

    while(r)
    {
        ssize_t ret;
        OSI_NO_INTR(ret = recv(sock_fd, buf, r, MSG_WAITALL));
        if(ret <= 0)
        {
            BTIF_TRACE_ERROR("sock fd:%d recv errno:%d, ret:%d", sock_fd, errno, ret);
            return -1;
        }
        buf += ret;
        r -= ret;
    }
    return len;
}

int sock_send_fd(int sock_fd, const uint8_t* buf, int len, int send_fd)
{
    struct msghdr msg;
    unsigned char *buffer = (unsigned char *)buf;
    memset(&msg, 0, sizeof(msg));

    struct cmsghdr *cmsg;
    char msgbuf[CMSG_SPACE(1)];
    asrt(send_fd != -1);
    if(sock_fd == -1 || send_fd == -1)
        return -1;
    // Add any pending outbound file descriptors to the message
           // See "man cmsg" really
    msg.msg_control = msgbuf;
    msg.msg_controllen = sizeof msgbuf;
    cmsg = CMSG_FIRSTHDR(&msg);
    cmsg->cmsg_level = SOL_SOCKET;
    cmsg->cmsg_type = SCM_RIGHTS;
    cmsg->cmsg_len = CMSG_LEN(sizeof send_fd);
    memcpy(CMSG_DATA(cmsg), &send_fd, sizeof send_fd);

    // We only write our msg_control during the first write
    int ret_len = len;
    while (len > 0) {
        struct iovec iv;
        memset(&iv, 0, sizeof(iv));

        iv.iov_base = buffer;
        iv.iov_len = len;

        msg.msg_iov = &iv;
        msg.msg_iovlen = 1;

        ssize_t ret;
        OSI_NO_INTR(ret = sendmsg(sock_fd, &msg, MSG_NOSIGNAL));
        if (ret < 0) {
            BTIF_TRACE_ERROR("fd:%d, send_fd:%d, sendmsg ret:%d, errno:%d, %s",
                              sock_fd, send_fd, (int)ret, errno, strerror(errno));
            ret_len = -1;
            break;
        }

        buffer += ret;
        len -= ret;

        // Wipes out any msg_control too
        memset(&msg, 0, sizeof(msg));
    }
    BTIF_TRACE_DEBUG("close fd:%d after sent", send_fd);
    // TODO: This seems wrong - if the FD is not opened in JAVA before this is called
    //       we get a "socket closed" exception in java, when reading from the socket...
    close(send_fd);
    return ret_len;
}

static const char* hex_table = "0123456789abcdef";
static inline void byte2hex(const char* data, char** str)
{
    **str = hex_table[(*data >> 4) & 0xf];
    ++*str;
    **str = hex_table[*data & 0xf];
    ++*str;
}
static inline void byte2char(const char* data, char** str)
{
    **str = *data < ' ' ? '.' : *data > '~' ? '.' : *data;
    ++(*str);
}
static inline void word2hex(const char* data, char** hex)
{
    byte2hex(&data[1], hex);
    byte2hex(&data[0], hex);
}
void dump_bin(const char* title, const char* data, int size)
{
    char line_buff[256];
    char *line;
    int i, j, addr;
    const int width = 16;
    LOG_DEBUG(LOG_TAG, "%s, size:%d, dump started {", title, size);
    if(size <= 0)
        return;
    //write offset
    line = line_buff;
    *line++ = ' ';
    *line++ = ' ';
    *line++ = ' ';
    *line++ = ' ';
    *line++ = ' ';
    *line++ = ' ';
    for(j = 0; j < width; j++)
    {
        byte2hex((const char*)&j, &line);
        *line++ = ' ';
    }
    *line = 0;
    LOG_DEBUG(LOG_TAG, "%s", line_buff);

    for(i = 0; i < size / width; i++)
    {
        line = line_buff;
        //write address:
        addr = i*width;
        word2hex((const char*)&addr, &line);
        *line++ = ':'; *line++ = ' ';
        //write hex of data
        for(j = 0; j < width; j++)
        {
            byte2hex(&data[j], &line);
            *line++ = ' ';
        }
        //write char of data
        for(j = 0; j < width; j++)
            byte2char(data++, &line);
        //wirte the end of line
        *line = 0;
        //output the line
        LOG_DEBUG(LOG_TAG, "%s", line_buff);
    }
    //last line of left over if any
    int leftover = size % width;
    if(leftover > 0)
    {
        line = line_buff;
        //write address:
        addr = i*width;
        word2hex((const char*)&addr, &line);
        *line++ = ':'; *line++ = ' ';
        //write hex of data
        for(j = 0; j < leftover; j++) {
            byte2hex(&data[j], &line);
            *line++ = ' ';
        }
        //write hex padding
        for(; j < width; j++) {
            *line++ = ' ';
            *line++ = ' ';
            *line++ = ' ';
        }
        //write char of data
        for(j = 0; j < leftover; j++)
            byte2char(data++, &line);
        //write the end of line
        *line = 0;
        //output the line
        LOG_DEBUG(LOG_TAG, "%s", line_buff);
    }
    LOG_DEBUG(LOG_TAG, "%s, size:%d, dump ended }", title, size);
}

