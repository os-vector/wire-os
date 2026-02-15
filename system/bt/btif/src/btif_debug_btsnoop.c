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

#include <assert.h>
#include <resolv.h>
#include <zlib.h>

#include "btif/include/btif_debug.h"
#include "btif/include/btif_debug_btsnoop.h"
#include "hci/include/btsnoop_mem.h"
#include "include/bt_target.h"
#include "osi/include/ringbuffer.h"

#define REDUCE_HCI_TYPE_TO_SIGNIFICANT_BITS(type) (type >> 8)

// Total btsnoop memory log buffer size
#ifndef BTSNOOP_MEM_BUFFER_SIZE
static const size_t BTSNOOP_MEM_BUFFER_SIZE = (256 * 1024);
#endif

// Block size for copying buffers (for compression/encoding etc.)
static const size_t BLOCK_SIZE = 16384;

// Maximum line length in bugreport (should be multiple of 4 for base64 output)
static const uint8_t MAX_LINE_LENGTH = 128;

static ringbuffer_t *buffer = NULL;
static uint64_t last_timestamp_ms = 0;

static size_t btsnoop_calculate_packet_length(uint16_t type, const uint8_t *data, size_t length);

static void btsnoop_cb(const uint16_t type, const uint8_t *data, const size_t length) {
  btsnooz_header_t header;

  size_t included_length = btsnoop_calculate_packet_length(type, data, length);
  if (included_length == 0)
    return;

  // Make room in the ring buffer

  while (ringbuffer_available(buffer) < (included_length + sizeof(btsnooz_header_t))) {
    ringbuffer_pop(buffer, (uint8_t *)&header, sizeof(btsnooz_header_t));
    ringbuffer_delete(buffer, header.length - 1);
  }

  // Insert data

  const uint64_t now = btif_debug_ts();

  header.type = REDUCE_HCI_TYPE_TO_SIGNIFICANT_BITS(type);
  header.length = included_length + 1;  // +1 for type byte
  header.packet_length = length + 1;  // +1 for type byte.
  header.delta_time_ms = last_timestamp_ms ? now - last_timestamp_ms : 0;
  last_timestamp_ms = now;

  ringbuffer_insert(buffer, (uint8_t *)&header, sizeof(btsnooz_header_t));
  ringbuffer_insert(buffer, data, included_length);
}

static size_t btsnoop_calculate_packet_length(uint16_t type, const uint8_t *data, size_t length) {
  static const size_t HCI_ACL_HEADER_SIZE = 4;
  static const size_t L2CAP_HEADER_SIZE = 4;
  static const size_t L2CAP_CID_OFFSET = (HCI_ACL_HEADER_SIZE + 2);
  static const uint16_t L2CAP_SIGNALING_CID = 0x0001;

  // Maximum amount of ACL data to log.
  // Enough for an RFCOMM frame up to the frame check;
  // not enough for a HID report or audio data.
  static const size_t MAX_HCI_ACL_LEN = 14;

  // Calculate packet length to be included

  switch (type) {
    case BT_EVT_TO_LM_HCI_CMD:
      return length;

    case BT_EVT_TO_BTU_HCI_EVT:
      return length;

    case BT_EVT_TO_LM_HCI_ACL:
    case BT_EVT_TO_BTU_HCI_ACL:
    {
      size_t len_hci_acl = HCI_ACL_HEADER_SIZE + L2CAP_HEADER_SIZE;
      // Check if we have enough data for an L2CAP header
      if (length > len_hci_acl) {
        uint16_t l2cap_cid = data[L2CAP_CID_OFFSET] | (data[L2CAP_CID_OFFSET + 1] << 8);
        if (l2cap_cid == L2CAP_SIGNALING_CID) {
          // For the signaling CID, take the full packet.
          // That way, the PSM setup is captured, allowing decoding of PSMs down the road.
          return length;
        } else {
          // Otherwise, return as much as we reasonably can
          len_hci_acl = MAX_HCI_ACL_LEN;
        }
      }
      return len_hci_acl < length ? len_hci_acl : length;
    }

    case BT_EVT_TO_LM_HCI_SCO:
    case BT_EVT_TO_BTU_HCI_SCO:
      // We're not logging SCO packets at this time since they are not currently used.
      // FALLTHROUGH
    default:
      return 0;
  }
}

static bool btsnoop_compress(ringbuffer_t *rb_dst, ringbuffer_t *rb_src) {
  assert(rb_dst != NULL);
  assert(rb_src != NULL);

  z_stream zs = {.zalloc = Z_NULL, .zfree = Z_NULL, .opaque = Z_NULL};
  if (deflateInit(&zs, Z_DEFAULT_COMPRESSION) != Z_OK)
    return false;

  bool rc = true;
  uint8_t block_src[BLOCK_SIZE];
  uint8_t block_dst[BLOCK_SIZE];

  const size_t num_blocks = (ringbuffer_size(rb_src) + BLOCK_SIZE - 1) / BLOCK_SIZE;
  for (size_t i = 0; i < num_blocks; ++i) {
    zs.avail_in = ringbuffer_peek(rb_src, i * BLOCK_SIZE, block_src, BLOCK_SIZE);
    zs.next_in = block_src;

    do {
      zs.avail_out = BLOCK_SIZE;
      zs.next_out = block_dst;

      int err = deflate(&zs, (i == num_blocks - 1) ? Z_FINISH : Z_NO_FLUSH);
      if (err == Z_STREAM_ERROR) {
        rc = false;
        break;
      }

      const size_t length = BLOCK_SIZE - zs.avail_out;
      ringbuffer_insert(rb_dst, block_dst, length);
    } while (zs.avail_out == 0);
  }

  deflateEnd(&zs);
  return rc;
}

void btif_debug_btsnoop_init(void) {
  if (buffer == NULL)
    buffer = ringbuffer_init(BTSNOOP_MEM_BUFFER_SIZE);
  btsnoop_mem_set_callback(btsnoop_cb);
}

void btif_debug_btsnoop_dump(int fd) {
  dprintf(fd, "--- BEGIN:BTSNOOP_LOG_SUMMARY (%zu bytes in) ---\n", ringbuffer_size(buffer));

  ringbuffer_t *ringbuffer = ringbuffer_init(BTSNOOP_MEM_BUFFER_SIZE);
  if (ringbuffer == NULL) {
    dprintf(fd, "%s Unable to allocate memory for compression", __func__);
    return;
  }

  // Prepend preamble

  btsnooz_preamble_t preamble;
  preamble.version = BTSNOOZ_CURRENT_VERSION;
  preamble.last_timestamp_ms = last_timestamp_ms;
  ringbuffer_insert(ringbuffer, (uint8_t *)&preamble, sizeof(btsnooz_preamble_t));

  // Compress data

  bool rc = btsnoop_compress(ringbuffer, buffer);
  if (rc == false) {
    dprintf(fd, "%s Log compression failed", __func__);
    goto error;
  }

  // Base64 encode & output

  uint8_t b64_in[3] = {0};
  char b64_out[5] = {0};

  size_t line_length = 0;
  while (ringbuffer_size(ringbuffer) > 0) {
    size_t read = ringbuffer_pop(ringbuffer, b64_in, 3);
    if (line_length >= MAX_LINE_LENGTH) {
      dprintf(fd, "\n");
      line_length = 0;
    }
    line_length += b64_ntop(b64_in, read, b64_out, 5);
    dprintf(fd, "%s", b64_out);
  }

  dprintf(fd, "\n--- END:BTSNOOP_LOG_SUMMARY ---\n");

error:
  ringbuffer_free(ringbuffer);
}
