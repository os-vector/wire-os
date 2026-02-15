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

#pragma once

#define REMOVE_EAGER_THREADS TRUE

#if (defined(REMOVE_EAGER_THREADS) && (REMOVE_EAGER_THREADS == TRUE))
#include "osi/include/allocator.h"
#include "osi/include/thread.h"
#include "osi/include/reactor.h"
#endif

// 2 bytes for opcode, 1 byte for parameter length (Volume 2, Part E, 5.4.1)
#define HCI_COMMAND_PREAMBLE_SIZE 3
// 2 bytes for handle, 2 bytes for data length (Volume 2, Part E, 5.4.2)
#define HCI_ACL_PREAMBLE_SIZE 4
// 2 bytes for handle, 1 byte for data length (Volume 2, Part E, 5.4.3)
#define HCI_SCO_PREAMBLE_SIZE 3
// 1 byte for event code, 1 byte for parameter length (Volume 2, Part E, 5.4.4)
#define HCI_EVENT_PREAMBLE_SIZE 2

#if (defined(REMOVE_EAGER_THREADS) && (REMOVE_EAGER_THREADS == TRUE))
struct hci_reader_t {
  int inbound_fd;

  const allocator_t *allocator;
  size_t buffer_size;
  uint8_t *data_buffer;
  int rd_ptr;
  int wr_ptr;

  thread_t *inbound_read_thread;
  reactor_object_t *inbound_read_object;
};
typedef  struct hci_reader_t hci_reader_t;
typedef void (*hci_reader_cb)(void *context);

hci_reader_t *hci_reader_new(
    int fd_to_read,
    size_t buffer_size,
    size_t max_buffer_count,
    thread_t *thread,
    hci_reader_cb read_cb
    );

size_t hci_reader_read(hci_reader_t *reader, uint8_t *buffer, size_t max_size);
void hci_reader_free(hci_reader_t *reader);
#endif
