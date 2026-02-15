/******************************************************************************
 *
 *  Copyright (C) 2015 Google Inc.
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

#include <stdio.h>
#include <string.h>
#include <time.h>

#include "btcore/include/bdaddr.h"
#include "btif/include/btif_debug.h"
#include "btif/include/btif_debug_conn.h"

#define NUM_CONNECTION_EVENTS  16
#define TEMP_BUFFER_SIZE       30

typedef struct conn_event_t {
  uint64_t ts;
  btif_debug_conn_state_t state;
  bt_bdaddr_t bda;
  tGATT_DISCONN_REASON disconnect_reason;
} conn_event_t;

static conn_event_t connection_events[NUM_CONNECTION_EVENTS];
static uint8_t current_event = 0;

static char *format_ts(const uint64_t ts, char *buffer, int len) {
  const uint64_t ms = ts / 1000;
  const time_t secs = ms / 1000;
  struct tm *ptm = localtime(&secs);

  char tempbuff[20] = {0};
  if (ptm) {
    strftime(tempbuff, sizeof(tempbuff), "%m-%d %H:%M:%S", ptm);
    snprintf(buffer, len, "%s.%03u", tempbuff, (uint16_t)(ms % 1000));
  }

  return buffer;
}

static char *format_state(const btif_debug_conn_state_t state) {
  switch (state) {
    case BTIF_DEBUG_CONNECTED:
      return "CONNECTED   ";
    case BTIF_DEBUG_DISCONNECTED:
      return "DISCONNECTED";
  }
  return "UNKNOWN";
}

static void next_event() {
  ++current_event;
  if (current_event == NUM_CONNECTION_EVENTS)
    current_event = 0;
}

void btif_debug_conn_state(const bt_bdaddr_t bda, const btif_debug_conn_state_t state,
    const tGATT_DISCONN_REASON disconnect_reason) {
  next_event();

  conn_event_t *evt = &connection_events[current_event];
  evt->ts = btif_debug_ts();
  evt->state = state;
  evt->disconnect_reason = disconnect_reason;
  memcpy(&evt->bda, &bda, sizeof(bt_bdaddr_t));
}

void btif_debug_conn_dump(int fd) {
  const uint8_t current_event_local = current_event; // Cache to avoid threading issues
  uint8_t dump_event = current_event_local;
  char ts_buffer[TEMP_BUFFER_SIZE] = {0};
  char name_buffer[TEMP_BUFFER_SIZE] = {0};

  dprintf(fd, "\nConnection Events:\n");
  if (connection_events[dump_event].ts == 0)
    dprintf(fd, "  None\n");

  while (connection_events[dump_event].ts) {
    conn_event_t *evt = &connection_events[dump_event];
    dprintf(fd, "  %s %s %s",
            format_ts(evt->ts, ts_buffer, sizeof(ts_buffer)),
            format_state(evt->state),
            bdaddr_to_string(&evt->bda, name_buffer, sizeof(name_buffer))
        );
    if (evt->state == BTIF_DEBUG_DISCONNECTED)
      dprintf(fd, " reason=%d", evt->disconnect_reason);
    dprintf(fd,"\n");

    // Go to previous event; wrap if needed
    if (dump_event > 0)
      --dump_event;
    else
      dump_event = NUM_CONNECTION_EVENTS - 1;

    // Check if we dumped all events
    if (dump_event == current_event_local)
      break;
  }
}
