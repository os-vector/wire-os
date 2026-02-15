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

#define LOG_TAG "bt_snoop"

#include <arpa/inet.h>
#include <assert.h>
#include <cutils/properties.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <limits.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/poll.h>
#include <unistd.h>
#ifndef ANDROID
#include <linux/types.h>
#endif

#include "bt_types.h"
#include "hci/include/btsnoop.h"
#include "hci/include/btsnoop_mem.h"
#include "hci_layer.h"
#include "osi/include/log.h"
#include "stack_config.h"

typedef enum {
  kCommandPacket = 1,
  kAclPacket = 2,
  kScoPacket = 3,
  kEventPacket = 4
} packet_type_t;

// Epoch in microseconds since 01/01/0000.
static const uint64_t BTSNOOP_EPOCH_DELTA = 0x00dcddb30f2f8000ULL;

static const stack_config_t *stack_config;
extern int client_socket_btsnoop;
static long int gmt_offset;
#define USEC_PER_SEC 1000000L
#define MAX_SNOOP_BUF_SIZE 1200

// External BT snoop
bool hci_ext_dump_enabled = false;

/* snoop config from the config file, required for userdebug
   build where snoop is enabled by default.
   power/perf measurements need the snoop to be disabled.
*/
bool btsnoop_conf_from_file = false;

static int logfile_fd = INVALID_FD;
static bool module_started;
static bool is_logging;
static bool logging_enabled_via_api;

// TODO(zachoverflow): merge btsnoop and btsnoop_net together
void btsnoop_net_open();
void btsnoop_net_close();
void btsnoop_net_write(const void *data, size_t length);

static void btsnoop_write_packet(packet_type_t type, const uint8_t *packet, bool is_received);
static void update_logging();

// Module lifecycle functions

static future_t *start_up(void) {
  time_t t = time(NULL);
  struct tm tm_cur;

  localtime_r (&t, &tm_cur);
  LOG_INFO(LOG_TAG, "%s Time GMT offset %ld\n", __func__, tm_cur.tm_gmtoff);
  gmt_offset = tm_cur.tm_gmtoff;

  module_started = true;
  stack_config->get_btsnoop_ext_options(&hci_ext_dump_enabled, &btsnoop_conf_from_file);
  if (btsnoop_conf_from_file == false) {
    hci_ext_dump_enabled = true;
  }
  update_logging();

  return NULL;
}

static future_t *shut_down(void) {
  module_started = false;
  if (hci_ext_dump_enabled == true) {
    STOP_SNOOP_LOGGING();
  }
  update_logging();

  return NULL;
}

//TODO: Fix this
#ifndef ANDROID
#define EXPORT_SYMBOL   __attribute__((visibility("default")))
#endif

EXPORT_SYMBOL const module_t btsnoop_module = {
  .name = BTSNOOP_MODULE,
  .init = NULL,
  .start_up = start_up,
  .shut_down = shut_down,
  .clean_up = NULL,
  .dependencies = {
    STACK_CONFIG_MODULE,
    NULL
  }
};

// Interface functions

static void set_api_wants_to_log(bool value) {
  logging_enabled_via_api = value;
  update_logging();
}


static void capture(const BT_HDR *buffer, bool is_received) {
  const uint8_t *p = buffer->data + buffer->offset;

  btsnoop_mem_capture(buffer);

  if (logfile_fd == INVALID_FD)
    return;

  switch (buffer->event & MSG_EVT_MASK) {
    case MSG_HC_TO_STACK_HCI_EVT:
      btsnoop_write_packet(kEventPacket, p, false);
#if (defined(BTC_INCLUDED) && BTC_INCLUDED == TRUE)
      btc_capture(buffer, kEventPacket);
#endif
      break;
    case MSG_HC_TO_STACK_HCI_ACL:
    case MSG_STACK_TO_HC_HCI_ACL:
      btsnoop_write_packet(kAclPacket, p, is_received);
      break;
    case MSG_HC_TO_STACK_HCI_SCO:
    case MSG_STACK_TO_HC_HCI_SCO:
      btsnoop_write_packet(kScoPacket, p, is_received);
      break;
    case MSG_STACK_TO_HC_HCI_CMD:
      btsnoop_write_packet(kCommandPacket, p, true);
#if (defined(BTC_INCLUDED) && BTC_INCLUDED == TRUE)
      btc_capture(buffer, kCommandPacket);
#endif
      break;
  }
}

static const btsnoop_t interface = {
  set_api_wants_to_log,
  capture
};

const btsnoop_t *btsnoop_get_interface() {
  stack_config = stack_config_get_interface();
  return &interface;
}

// Internal functions

static uint64_t btsnoop_timestamp(void) {
  struct timeval tv;
  gettimeofday(&tv, NULL);
  tv.tv_sec += gmt_offset;

  // Timestamp is in microseconds.
  uint64_t timestamp = ((uint64_t)tv.tv_sec) * 1000 * 1000LL;
  timestamp += tv.tv_usec;
  timestamp += BTSNOOP_EPOCH_DELTA;
  return timestamp;
}

static void update_logging() {
  bool should_log = module_started &&
    (logging_enabled_via_api || stack_config->get_btsnoop_turned_on() || hci_ext_dump_enabled);

  if (should_log == is_logging)
    return;

  is_logging = should_log;
  if (should_log) {
    if (hci_ext_dump_enabled == true) {
      START_SNOOP_LOGGING();
    }
    const char *log_path = stack_config->get_btsnoop_log_path();

    // Save the old log if configured to do so
    if (stack_config->get_btsnoop_should_save_last()) {
      char last_log_path[PATH_MAX];
      snprintf(last_log_path, PATH_MAX, "%s.%" PRIu64, log_path,
               btsnoop_timestamp());
      if (!rename(log_path, last_log_path) && errno != ENOENT)
        LOG_ERROR(LOG_TAG, "%s unable to rename '%s' to '%s': %s", __func__, log_path, last_log_path, strerror(errno));
    }

    logfile_fd = open(log_path, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH);
    if (logfile_fd == INVALID_FD) {
      LOG_ERROR(LOG_TAG, "%s unable to open '%s': %s", __func__, log_path, strerror(errno));
      is_logging = false;
      return;
    }

    write(logfile_fd, "btsnoop\0\0\0\0\1\0\0\x3\xea", 16);
    btsnoop_net_open();
  } else {
    if (logfile_fd != INVALID_FD)
      close(logfile_fd);

    logfile_fd = INVALID_FD;
    btsnoop_net_close();
  }
}

static void btsnoop_write(const void *data, size_t length) {
  if (client_socket_btsnoop != -1) {
    btsnoop_net_write(data, length);
    /* skip writing to file if external client is connected*/
    return;
  }

  if (logfile_fd != INVALID_FD)
    write(logfile_fd, data, length);
}

#ifdef DEBUG_SNOOP
static uint64_t time_now_us() {
    struct timespec ts_now;
    clock_gettime(CLOCK_BOOTTIME, &ts_now);
    return ((uint64_t)ts_now.tv_sec * USEC_PER_SEC) + ((uint64_t)ts_now.tv_nsec / 1000);
}
#endif

static void btsnoop_write_packet(packet_type_t type, const uint8_t *packet, bool is_received) {
  int length_he = 0;
  int length;
  int flags;
  int drops = 0;
  struct pollfd pfd;
#ifdef DEBUG_SNOOP
  uint64_t ts_begin;
  uint64_t ts_end, ts_diff;
#endif
  uint8_t snoop_buf[MAX_SNOOP_BUF_SIZE] = {0};
  uint32_t offset = 0;

  switch (type) {
    case kCommandPacket:
      length_he = packet[2] + 4;
      flags = 2;
      break;
    case kAclPacket:
      length_he = (packet[3] << 8) + packet[2] + 5;
      flags = is_received;
      break;
    case kScoPacket:
      length_he = packet[2] + 4;
      flags = is_received;
      break;
    case kEventPacket:
      length_he = packet[1] + 3;
      flags = 3;
      break;
  }

  uint64_t timestamp = btsnoop_timestamp();
  uint32_t time_hi = timestamp >> 32;
  uint32_t time_lo = timestamp & 0xFFFFFFFF;

  length = htonl(length_he);
  flags = htonl(flags);
  drops = htonl(drops);
  time_hi = htonl(time_hi);
  time_lo = htonl(time_lo);

  /* store the length in both original and included fields */
  memcpy(snoop_buf + offset, &length, 4);
  offset += 4;
  memcpy(snoop_buf + offset, &length, 4);
  offset += 4;

  /* flags:  */
  memcpy(snoop_buf + offset, &flags, 4);
  offset += 4;

  /* drops: none */
  memcpy(snoop_buf + offset, &drops, 4);
  offset += 4;

  /* time */
  memcpy(snoop_buf + offset, &time_hi, 4);
  offset += 4;
  memcpy(snoop_buf + offset, &time_lo, 4);
  offset = offset + 4;

  snoop_buf[offset] = type;
  offset += 1;
  if (offset + length_he + 1 > MAX_SNOOP_BUF_SIZE) {
    LOG_ERROR(LOG_TAG, "Bad packet length, downgrading the length to %d from %d",
                                      MAX_SNOOP_BUF_SIZE - offset - 1, length_he);
    length_he = MAX_SNOOP_BUF_SIZE - offset - 1;
  }
  memcpy(snoop_buf + offset, packet, length_he - 1);

  if (client_socket_btsnoop != -1) {
    pfd.fd = client_socket_btsnoop;
    pfd.events = POLLOUT;
#ifdef DEBUG_SNOOP
    ts_begin = time_now_us();
#endif

    if (poll(&pfd, 1, 10) == 0) {
      LOG_ERROR(LOG_TAG, "btsnoop poll : Taking more than 10 ms : skip dump");
#ifdef DEBUG_SNOOP
      ts_end = time_now_us();
      ts_diff = ts_end - ts_begin;
      if (ts_diff > 10000) {
        LOG_ERROR(LOG_TAG, "btsnoop poll T/O : took more time %08lld us", ts_diff);
      }
#endif
      return;
    }

#ifdef DEBUG_SNOOP
    ts_end = time_now_us();
    ts_diff = ts_end - ts_begin;
    if (ts_diff > 10000) {
      LOG_ERROR(LOG_TAG, "btsnoop poll : took more time %08lld us", ts_diff);
    }
#endif
  }
#ifdef DEBUG_SNOOP
  ts_begin = time_now_us();
#endif

  btsnoop_write(snoop_buf, offset + length_he - 1);

#ifdef DEBUG_SNOOP
  ts_end = time_now_us();
  ts_diff = ts_end - ts_begin;
  if (ts_diff > 10000) {
    LOG_ERROR(LOG_TAG, "btsnoop write : Write took more time %08lld us", ts_diff);
  }
#endif
}
