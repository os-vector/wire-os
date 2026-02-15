/******************************************************************************
 *
 * Copyright (c) 2016-2017, The Linux Foundation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above
 *     copyright notice, this list of conditions and the following
 *     disclaimer in the documentation and/or other materials provided
 *     with the distribution.
 *   * Neither the name of The Linux Foundation nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE
 *
 *****************************************************************************/

#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <malloc.h>
#include <poll.h>
#include <grp.h>
#include <sys/un.h>
#include <cutils/sockets.h>
#include <stdlib.h>
#include "property_ops.h"

static int open_prop_socket()
{
    int fd, ret = 0;

    fd = socket(PF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (fd < 0) {
        ret = -errno;
    } else {
        struct sockaddr_un un;
        memset(&un, 0, sizeof(struct sockaddr_un));
        un.sun_family = AF_UNIX;
        snprintf(un.sun_path, sizeof(un.sun_path),
                        ANDROID_SOCKET_DIR"/%s", PROP_SERVICE_NAME);

    if (TEMP_FAILURE_RETRY(connect(fd, (struct sockaddr *)&un,
                                       sizeof(struct sockaddr_un))) < 0) {
            ALOGE("Failed to connect to %s socket", PROP_SERVICE_NAME);
            close(fd);
            ret = -errno;
        } else {
        ret = fd;  // return socket fd.
        }
    }
    return ret;
}

static int send_setprop_msg(const char *msg)
{
    int fd, ret = 0;

    fd = open_prop_socket();
    if (fd < 0) {
       ALOGE("Failed to open Socket");
       return fd; // pass error back to caller.
    }

    int msg_len = strlen(msg);
    const int num_bytes = TEMP_FAILURE_RETRY(send(fd, msg, msg_len, 0));

    if (num_bytes == msg_len) {
        // Successfully wrote to the property service but now wait
        // for 250 ms for property service to finish its work. It
        // acknowledges its completion by closing the socket.
        struct pollfd pollfds[1];
        pollfds[0].fd = fd;
        pollfds[0].events = 0;
        const int poll_result = TEMP_FAILURE_RETRY(poll(pollfds, 1, 250 /* ms */));
        if (poll_result == 1 && (pollfds[0].revents & POLLHUP) != 0) {
            ret = 0;
        } else {
            ALOGE("setprop poll timed out");
            ret = 0; // Ignore timeout and treat it like a success.
        }
        LOG("Sent %d bytes of data (%s) to Socket", num_bytes, msg);
    }
    close(fd);
    return ret;
}

static int send_getprop_msg(const char *msg, char *resp)
{
    int fd = open_prop_socket();
    if (fd < 0) {
       ALOGE("Failed to open Socket");
       return fd; // pass error back to caller.
    }

    int ret = 0;
    int msg_len = strlen(msg);
    const int num_bytes = TEMP_FAILURE_RETRY(send(fd, msg, msg_len, 0));

    if (num_bytes == msg_len) {
        // Successfully wrote to the property service but now wait
        // for 250 ms for property service to finish its work. It
        // acknowledges its completion by closing the socket.
        struct pollfd pollfds[1];
        pollfds[0].fd = fd;
        pollfds[0].events = 0;
        const int poll_result = TEMP_FAILURE_RETRY(poll(pollfds, 1, 250 /* ms */));
        if (poll_result == 1 && (pollfds[0].revents & POLLHUP) != 0) {
            //Handle data sent from service
            char recv_buf[MAX_ALLOWED_LINE_LEN+1];
            memset(recv_buf, 0 , sizeof(recv_buf));
            int nbytes = recv(fd, recv_buf, sizeof(recv_buf), 0);
            if (nbytes <= 0) {
                ALOGE("recv failed (%s)",strerror(errno));
                ret = -1;
            } else {
                recv_buf[nbytes] = '\0';
                LOG("Received %d bytes of data (%s) from Socket",nbytes, recv_buf);
                int i = 1;
                while(i < strlen(recv_buf)){
                    resp[i-1] = recv_buf[i];
                    i++;
                }
                ret = 0;
            }
        } else {
            ALOGE("getprop poll timed out");
            ret = -1; // Don't try to recv in case of time out.
        }
    }
    close(fd);
    return ret;
}

bool set_property_value(const char* prop_name, unsigned char *prop_val)
{
    char msg[MAX_ALLOWED_LINE_LEN+1]; // +1 for msg type.
    memset(msg, 0 , sizeof(msg));

    snprintf(msg, MAX_ALLOWED_LINE_LEN+1, "%c%s=%s",
             PROP_MSG_SETPROP, prop_name, prop_val);

    const int err = send_setprop_msg(msg);
    if (err < 0) {
       ALOGE("Failed to send message to Set %s", prop_name);
       return false;
    }

    return true;
}

bool get_property_value(const char* prop_name, unsigned char *prop_val)
{
    char msg[MAX_ALLOWED_LINE_LEN+1]; // +1 for msg type.
    char resp[MAX_ALLOWED_LINE_LEN];

    memset(msg,  0 , sizeof(msg));
    memset(resp, 0 , sizeof(resp));

    snprintf(msg, sizeof(msg), "%c%s=",
            PROP_MSG_GETPROP, prop_name);

    const int err = send_getprop_msg(msg, resp);
    if (err < 0) {
       LOG("Failed to send message to Get %s", prop_name);
       return false;
    }

    // Extract prop value from response.
    char *delimiter = strchr(resp, '=');
    const char *curr_line_ptr = delimiter+1; //+1 for delimiter
    if(strlen(curr_line_ptr) <= 0) {
        LOG("%s has invalid length", prop_name);
        return false;
    }

    strlcpy(prop_val, curr_line_ptr, PROP_VALUE_MAX);

    return true;
}

void dump_persist(void)
{
    //TO BE IMPLEMENTED
}
