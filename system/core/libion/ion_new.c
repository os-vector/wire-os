/*
 * Copyright (c) 2018, The Linux Foundation. All rights reserved.

 * Not a Contribution.

 *
 *
 *   Copyright 2011 Google, Inc
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 */

/* Memory Allocator function for ion */

#define LOG_TAG "ion"

#include <errno.h>
#include <fcntl.h>
#include <linux/ion.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <unistd.h>

#include <log/log.h>

#define UNUSED_PARAM(x) ((void)(x))

enum ion_version { ION_VERSION_UNKNOWN, ION_VERSION_MODERN, ION_VERSION_LEGACY };

int ion_is_legacy(int fd) {
    UNUSED_PARAM(fd);
    return 0;
}

int ion_open() {
    int fd = open("/dev/ion", O_RDONLY | O_CLOEXEC);
    if (fd < 0) ALOGE("open /dev/ion failed!\n");

    return fd;
}

int ion_close(int fd) {
    int ret = close(fd);
    if (ret < 0) return -errno;
    return ret;
}

static int ion_ioctl(int fd, int req, void* arg) {
    int ret = ioctl(fd, req, arg);
    if (ret < 0) {
        int ret_errno = errno;

        ALOGE("ioctl %x failed with code %d: %s\n", req, ret, strerror(ret_errno));
        return -ret_errno;
    }
    return ret;
}

int ion_alloc(int fd, size_t len, size_t align, unsigned int heap_mask, unsigned int flags,
              ion_user_handle_t* handle) {
    UNUSED_PARAM(fd);
    UNUSED_PARAM(len);
    UNUSED_PARAM(align);
    UNUSED_PARAM(heap_mask);
    UNUSED_PARAM(flags);
    UNUSED_PARAM(handle);

    return -EINVAL;
}

int ion_free(int fd, ion_user_handle_t handle) {
    UNUSED_PARAM(fd);
    UNUSED_PARAM(handle);

    return -EINVAL;
}

int ion_map(int fd, ion_user_handle_t handle, size_t length, int prot, int flags, off_t offset,
            unsigned char** ptr, int* map_fd) {
    UNUSED_PARAM(fd);
    UNUSED_PARAM(handle);
    UNUSED_PARAM(length);
    UNUSED_PARAM(prot);
    UNUSED_PARAM(flags);
    UNUSED_PARAM(offset);
    UNUSED_PARAM(ptr);
    UNUSED_PARAM(map_fd);

    return -EINVAL;
}

int ion_share(int fd, ion_user_handle_t handle, int* share_fd) {
    UNUSED_PARAM(fd);
    UNUSED_PARAM(handle);
    UNUSED_PARAM(share_fd);

    return -EINVAL;
}

int ion_alloc_fd(int fd, size_t len, size_t align, unsigned int heap_mask, unsigned int flags,
                 int* handle_fd) {
    UNUSED_PARAM(align);

    int ret;

    if (!ion_is_legacy(fd)) {
        struct ion_allocation_data data = {
            .len = len,
            .heap_id_mask = heap_mask,
            .flags = flags,
        };

        ret = ion_ioctl(fd, ION_IOC_ALLOC, &data);
        if (ret < 0) return ret;
        *handle_fd = data.fd;
    } else {
        ret = -EINVAL;
    }
    return ret;
}

int ion_import(int fd, int share_fd, ion_user_handle_t* handle) {
    UNUSED_PARAM(fd);
    UNUSED_PARAM(share_fd);
    UNUSED_PARAM(handle);

    return -EINVAL;
}

int ion_sync_fd(int fd, int handle_fd) {
    UNUSED_PARAM(fd);
    UNUSED_PARAM(handle_fd);

    return -EINVAL;
}

int ion_query_heap_cnt(int fd, int* cnt) {
    int ret;
    struct ion_heap_query query;

    memset(&query, 0, sizeof(query));

    ret = ion_ioctl(fd, ION_IOC_HEAP_QUERY, &query);
    if (ret < 0) return ret;

    *cnt = query.cnt;
    return ret;
}

int ion_query_get_heaps(int fd, int cnt, void* buffers) {
    int ret;
    struct ion_heap_query query = {
        .cnt = cnt, .heaps = (uintptr_t)buffers,
    };

    ret = ion_ioctl(fd, ION_IOC_HEAP_QUERY, &query);
    return ret;
}
