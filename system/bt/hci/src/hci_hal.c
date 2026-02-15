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
#include <string.h>
#include "hci_hal.h"
#include "hci_internals.h"
#include "bt_utils.h"
#if (defined(REMOVE_EAGER_THREADS) && (REMOVE_EAGER_THREADS == TRUE))
#include <assert.h>
#include "osi/include/eager_reader.h"
#include "osi/include/osi.h"
#include "osi/include/log.h"
#endif

bt_soc_type soc_type;

const hci_hal_t *hci_hal_get_interface() {
    soc_type = get_soc_type();

    if (soc_type == BT_SOC_ROME || soc_type == BT_SOC_CHEROKEE) {
        return hci_hal_h4_get_interface();
    } else {
        return hci_hal_mct_get_interface();
    }
}

#if (defined(REMOVE_EAGER_THREADS) && (REMOVE_EAGER_THREADS == TRUE))
void hci_reader_free(hci_reader_t *reader) {
  if (!reader)
    return;

  // Only unregister from the input if we actually did register
  if (reader->inbound_read_object)
    reactor_unregister(reader->inbound_read_object);

  // Free the current buffer, because it's not in the queue
  // and won't be freed below
  if (reader->data_buffer)
    osi_free(reader->data_buffer);

  osi_free(reader);
}

hci_reader_t *hci_reader_new(
    int fd_to_read,
    size_t buffer_size,
    size_t max_buffer_count,
    thread_t *thread,
    hci_reader_cb read_cb
    ) {

  assert(fd_to_read != INVALID_FD);
  assert(buffer_size > 0);
  assert(max_buffer_count > 0);


  hci_reader_t *ret = osi_calloc(sizeof(hci_reader_t));
  if (!ret) {
    LOG_ERROR("%s unable to allocate memory for new hci_reader.", __func__);
    goto error;
  }

  ret->inbound_fd = fd_to_read;

  ret->data_buffer = osi_calloc(buffer_size);
  ret->buffer_size = buffer_size;
  ret->rd_ptr = 0;
  ret->wr_ptr = 0;

  ret->inbound_read_thread = thread;
  if (!ret->inbound_read_thread) {
    LOG_ERROR("%s unable to make reading thread.", __func__);
    goto error;
  }

  ret->inbound_read_object = reactor_register(
      thread_get_reactor(ret->inbound_read_thread),
      fd_to_read,
      ret,
      read_cb,
      NULL
      );

  return ret;

error:;
  hci_reader_free(ret);
  return NULL;
}

size_t hci_reader_read(hci_reader_t *reader, uint8_t *buffer, size_t req_size) {
  int bytes_read = 0;
  assert(reader != NULL);
  assert(buffer != NULL);

  // If the caller wants nonblocking behavior, poll to see if we have
  // any bytes available before reading.
  if (reader->rd_ptr < reader->wr_ptr) {
    bytes_read = reader->wr_ptr - reader->rd_ptr;
    if ((size_t) bytes_read > req_size)
      bytes_read = req_size;
    memcpy(buffer, reader->data_buffer+reader->rd_ptr, bytes_read);
    reader->rd_ptr += bytes_read;
  } else {
    bytes_read = read(reader->inbound_fd, buffer, req_size);
    if(bytes_read == -1)
      bytes_read = 0;
  }

  return bytes_read;
}
#endif
