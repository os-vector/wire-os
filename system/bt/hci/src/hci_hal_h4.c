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

#define LOG_TAG "bt_hci_h4"

#include <assert.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>

#include "hci_hal.h"
#include "osi/include/eager_reader.h"
#include "osi/include/log.h"
#include "osi/include/osi.h"
#include "osi/include/reactor.h"
#include "osi/include/thread.h"
#include "vendor.h"

#define HCI_HAL_SERIAL_BUFFER_SIZE 1026
#define HCI_BLE_EVENT 0x3e

// Increased HCI thread priority to keep up with the audio sub-system
// when streaming time sensitive data (A2DP).
#define HCI_THREAD_PRIORITY -19

#define BT_HCI_UNKNOWN_MESSAGE_TYPE_NUM 1010002

// Our interface and modules we import
static const hci_hal_t interface;
static const hci_hal_callbacks_t *callbacks;
static const vendor_t *vendor;

static thread_t *thread; // Not owned by us

static int uart_fd;
#if (defined(REMOVE_EAGER_THREADS) && (REMOVE_EAGER_THREADS == TRUE))
static hci_reader_t *uart_stream;
#else
static eager_reader_t *uart_stream;
#endif
static serial_data_type_t current_data_type;
static bool stream_has_interpretation;
static bool stream_corruption_detected;
static uint8_t stream_corruption_bytes_to_ignore;

#if (defined(REMOVE_EAGER_THREADS) && (REMOVE_EAGER_THREADS == TRUE))
static void event_uart_has_bytes(void *context);
#else
static void event_uart_has_bytes(eager_reader_t *reader, void *context);
static bool stream_corrupted_during_le_scan_workaround(const uint8_t byte_read);
#endif


// Interface functions

static bool hal_init(const hci_hal_callbacks_t *upper_callbacks, thread_t *upper_thread) {
  assert(upper_callbacks != NULL);
  assert(upper_thread != NULL);

  callbacks = upper_callbacks;
  thread = upper_thread;
  return true;
}

static bool hal_open() {
  LOG_INFO(LOG_TAG, "%s", __func__);
  // TODO(zachoverflow): close if already open / or don't reopen (maybe at the hci layer level)

  int fd_array[CH_MAX];
  int number_of_ports = vendor->send_command(VENDOR_OPEN_USERIAL, &fd_array);

  if (number_of_ports != 1) {
    LOG_ERROR(LOG_TAG, "%s opened the wrong number of ports: got %d, expected 1.", __func__, number_of_ports);
    goto error;
  }

  uart_fd = fd_array[0];
  if (uart_fd == INVALID_FD) {
    LOG_ERROR(LOG_TAG, "%s unable to open the uart serial port.", __func__);
    goto error;
  }

#if (defined(REMOVE_EAGER_THREADS) && (REMOVE_EAGER_THREADS == TRUE))
  uart_stream = hci_reader_new(uart_fd, HCI_HAL_SERIAL_BUFFER_SIZE, SIZE_MAX, thread, event_uart_has_bytes);
  if (!uart_stream) {
    LOG_ERROR("%s unable to create hci reader for the uart serial port.", __func__);
    goto error;
  }
#else
  uart_stream = eager_reader_new(uart_fd, &allocator_malloc, HCI_HAL_SERIAL_BUFFER_SIZE, SIZE_MAX, "hci_single_channel");
  if (!uart_stream) {
    LOG_ERROR(LOG_TAG, "%s unable to create eager reader for the uart serial port.", __func__);
    goto error;
  }
  eager_reader_register(uart_stream, thread_get_reactor(thread), event_uart_has_bytes, NULL);
  thread_set_priority(eager_reader_get_read_thread(uart_stream), HCI_THREAD_PRIORITY);
#endif

  stream_has_interpretation = false;
  stream_corruption_detected = false;
  stream_corruption_bytes_to_ignore = 0;

  // Raise thread priorities to keep up with audio
  thread_set_priority(thread, HCI_THREAD_PRIORITY);

  return true;

error:
  interface.close();
  return false;
}

static void hal_close() {
  LOG_INFO(LOG_TAG, "%s", __func__);

#if (defined(REMOVE_EAGER_THREADS) && (REMOVE_EAGER_THREADS == TRUE))
  hci_reader_free(uart_stream);
#else
  eager_reader_free(uart_stream);
#endif
  uart_stream = NULL;

  vendor->send_command(VENDOR_CLOSE_USERIAL, NULL);
  uart_fd = INVALID_FD;
}

static size_t read_data(serial_data_type_t type, uint8_t *buffer, size_t max_size) {
  if (type < DATA_TYPE_ACL || type > DATA_TYPE_EVENT) {
    LOG_ERROR(LOG_TAG, "%s invalid data type: %d", __func__, type);
    return 0;
  } else if (!stream_has_interpretation) {
    LOG_ERROR(LOG_TAG, "%s with no valid stream intepretation.", __func__);
    return 0;
  } else if (current_data_type != type) {
    LOG_ERROR(LOG_TAG, "%s with different type than existing interpretation.", __func__);
    return 0;
  }

#if (defined(REMOVE_EAGER_THREADS) && (REMOVE_EAGER_THREADS == TRUE))
  return hci_reader_read(uart_stream, buffer, max_size);
#else
  return eager_reader_read(uart_stream, buffer, max_size);
#endif
}

static void packet_finished(serial_data_type_t type) {
  if (!stream_has_interpretation) {
    LOG_ERROR(LOG_TAG, "%s with no existing stream interpretation.", __func__);
  } else if (current_data_type != type) {
    LOG_ERROR(LOG_TAG, "%s with different type than existing interpretation.", __func__);
  }

#if (defined(REMOVE_EAGER_THREADS) && (REMOVE_EAGER_THREADS == TRUE))
  if (uart_stream->rd_ptr == uart_stream->wr_ptr) {
    uart_stream->rd_ptr = uart_stream->wr_ptr = 0;
    stream_has_interpretation = false;
  } else {
    uint8_t type_byte;
    type_byte = uart_stream->data_buffer[uart_stream->rd_ptr++];
    if (type_byte < DATA_TYPE_ACL || type_byte > DATA_TYPE_EVENT) {
      LOG_ERROR("%s Unknown HCI message type. Dropping this byte 0x%x, min %x, max %x", __func__,
                                                    type_byte, DATA_TYPE_ACL, DATA_TYPE_EVENT);
      return;
    }
    current_data_type = type_byte;
    callbacks->data_ready(current_data_type);
  }
#else
  stream_has_interpretation = false;
#endif
}

static uint16_t transmit_data(serial_data_type_t type, uint8_t *data, uint16_t length) {
  assert(data != NULL);
  assert(length > 0);

  if (type < DATA_TYPE_COMMAND || type > DATA_TYPE_SCO) {
    LOG_ERROR(LOG_TAG, "%s invalid data type: %d", __func__, type);
    return 0;
  }

  // Write the signal byte right before the data
  --data;
  uint8_t previous_byte = *data;
  *(data) = type;
  ++length;

  uint16_t transmitted_length = 0;
  while (length > 0) {
    ssize_t ret;
    OSI_NO_INTR(ret = write(uart_fd, data + transmitted_length, length));
    switch (ret) {
      case -1:
        LOG_ERROR(LOG_TAG, "In %s, error writing to the uart serial port: %s", __func__, strerror(errno));
        goto done;
      case 0:
        // If we wrote nothing, don't loop more because we
        // can't go to infinity or beyond
        goto done;
      default:
        transmitted_length += ret;
        length -= ret;
        break;
    }
  }

done:;
  // Be nice and restore the old value of that byte
  *(data) = previous_byte;

  // Remove the signal byte from our transmitted length, if it was actually written
  if (transmitted_length > 0)
    --transmitted_length;

  return transmitted_length;
}

static bool hal_dev_in_reset()
{
    return false;
}

// Internal functions

// WORKAROUND:
// As exhibited by b/23934838, during result-heavy LE scans, the UART byte
// stream can get corrupted, leading to assertions caused by mis-interpreting
// the bytes following the corruption.
// This workaround looks for tell-tale signs of a BLE event and attempts to
// skip the correct amount of bytes in the stream to re-synchronize onto
// a packet boundary.
// Function returns true if |byte_read| has been processed by the workaround.
#if (!defined(REMOVE_EAGER_THREADS) || ((defined(REMOVE_EAGER_THREADS) && (REMOVE_EAGER_THREADS == FALSE))))
static bool stream_corrupted_during_le_scan_workaround(const uint8_t byte_read)
{
  if (!stream_corruption_detected && byte_read == HCI_BLE_EVENT) {
    LOG_ERROR(LOG_TAG, "%s HCI stream corrupted (message type 0x3E)!", __func__);
    stream_corruption_detected = true;
    return true;
  }

  if (stream_corruption_detected) {
    if (stream_corruption_bytes_to_ignore == 0) {
      stream_corruption_bytes_to_ignore = byte_read;
      LOG_ERROR(LOG_TAG, "%s About to skip %d bytes...", __func__, stream_corruption_bytes_to_ignore);
    } else {
      --stream_corruption_bytes_to_ignore;
    }

    if (stream_corruption_bytes_to_ignore == 0) {
      LOG_ERROR(LOG_TAG, "%s Back to our regularly scheduled program...", __func__);
      stream_corruption_detected = false;
    }
    return true;
  }

  return false;
}
#endif
// See what data is waiting, and notify the upper layer
#if (defined(REMOVE_EAGER_THREADS) && (REMOVE_EAGER_THREADS == TRUE))
static void event_uart_has_bytes(void *context) {
  uint8_t type_byte;
  int bytes_read;
  hci_reader_t *reader = (hci_reader_t *) context;
  bytes_read = read(reader->inbound_fd, reader->data_buffer + reader->wr_ptr, reader->buffer_size - reader->wr_ptr);

  if (bytes_read <= 0)  {
    LOG_ERROR("%s could not read HCI message type", __func__);
    return;
  }
  reader->wr_ptr += bytes_read;
  if (!stream_has_interpretation) {
    type_byte = reader->data_buffer[reader->rd_ptr++];

    if (type_byte < DATA_TYPE_ACL || type_byte > DATA_TYPE_EVENT) {
      LOG_ERROR("%s Unknown HCI message type. Dropping this byte 0x%x, min %x, max %x", __func__, type_byte, DATA_TYPE_ACL, DATA_TYPE_EVENT);
      return;
    }

    stream_has_interpretation = true;
    current_data_type = type_byte;
  }

  callbacks->data_ready(current_data_type);
}
#else
static void event_uart_has_bytes(eager_reader_t *reader, UNUSED_ATTR void *context) {
  if (stream_has_interpretation) {
    callbacks->data_ready(current_data_type);
  } else {
    uint8_t type_byte;
    if (eager_reader_read(reader, &type_byte, 1) == 0) {
      LOG_ERROR(LOG_TAG, "%s could not read HCI message type", __func__);
      return;
    }

    if (stream_corrupted_during_le_scan_workaround(type_byte))
      return;

    if (type_byte < DATA_TYPE_ACL || type_byte > DATA_TYPE_EVENT) {
      LOG_ERROR(LOG_TAG, "%s Unknown HCI message type 0x%x (min=0x%x max=0x%x). Aborting...",
                __func__, type_byte, DATA_TYPE_ACL, DATA_TYPE_EVENT);
      LOG_EVENT_INT(BT_HCI_UNKNOWN_MESSAGE_TYPE_NUM, type_byte);
      assert(false && "Unknown HCI message type");
      return;
    }

    stream_has_interpretation = true;
    current_data_type = type_byte;
  }
}
#endif

static const hci_hal_t interface = {
  hal_init,

  hal_open,
  hal_close,

  read_data,
  packet_finished,
  transmit_data,
  hal_dev_in_reset
};

const hci_hal_t *hci_hal_h4_get_interface() {
  vendor = vendor_get_interface();
  return &interface;
}

const hci_hal_t *hci_hal_h4_get_test_interface(vendor_t *vendor_interface) {
  vendor = vendor_interface;
  return &interface;
}
