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

#include <gtest/gtest.h>

#include "AllocationTestHarness.h"

extern "C" {
#include <stdint.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>

#include "osi/include/osi.h"
#include "osi/include/semaphore.h"
#include "hci_hal.h"
#include "test_stubs.h"
#include "vendor.h"
}

DECLARE_TEST_MODES(
  init,
  open,
  close_fn,
  transmit,
  read_synchronous,
  read_async_reentry,
  type_byte_only
);

// Use as packet type to test stream_corrupted_during_le_scan_workaround()
static const uint8_t HCI_BLE_EVENT = 0x3e;

static char sample_data1[100] = "A point is that which has no part.";
static char sample_data2[100] = "A line is breadthless length.";
static char sample_data3[100] = "The ends of a line are points.";
static char acl_data[100] =     "A straight line is a line which lies evenly with the points on itself.";
static char sco_data[100] =     "A surface is that which has length and breadth only.";
static char event_data[100] =   "The edges of a surface are lines.";

// Test data for stream_corrupted_during_le_scan_workaround()
static char corrupted_data[] = { 0x5 /* length of remaining data */, 'H', 'e', 'l', 'l', 'o' };

static const hci_hal_t *hal;
static int dummy_serial_fd;
static int reentry_i = 0;

static semaphore_t *done;
static semaphore_t *reentry_semaphore;

static void expect_packet_synchronous(serial_data_type_t type, char *packet_data) {
  int length = strlen(packet_data);
  for (int i = 0; i < length; i++) {
    uint8_t byte;
    EXPECT_EQ((size_t)1, hal->read_data(type, &byte, 1));
    EXPECT_EQ(packet_data[i], byte);
  }

  hal->packet_finished(type);
}

STUB_FUNCTION(int, vendor_send_command, (vendor_opcode_t opcode, void *param))
  DURING(open) AT_CALL(0) {
    EXPECT_EQ(VENDOR_OPEN_USERIAL, opcode);
    // Give back the dummy fd and the number 1 to say we opened 1 port
    ((int *)param)[0] = dummy_serial_fd;
    return 1;
  }

  DURING(close_fn) AT_CALL(0) {
    EXPECT_EQ(VENDOR_CLOSE_USERIAL, opcode);
    return 0;
  }

  UNEXPECTED_CALL;
  return 0;
}

STUB_FUNCTION(void, data_ready_callback, (serial_data_type_t type))
  DURING(read_synchronous) {
    AT_CALL(0) {
      EXPECT_EQ(DATA_TYPE_ACL, type);
      expect_packet_synchronous(type, acl_data);
      return;
    }
    AT_CALL(1) {
      EXPECT_EQ(DATA_TYPE_SCO, type);
      expect_packet_synchronous(type, sco_data);
      return;
    }
    AT_CALL(2) {
      EXPECT_EQ(DATA_TYPE_EVENT, type);
      expect_packet_synchronous(type, event_data);
      semaphore_post(done);
      return;
    }
  }

  DURING(read_async_reentry) {
    EXPECT_EQ(DATA_TYPE_ACL, type);

    uint8_t byte;
    size_t bytes_read;
    while ((bytes_read = hal->read_data(type, &byte, 1)) != 0) {
      EXPECT_EQ(sample_data3[reentry_i], byte);
      semaphore_post(reentry_semaphore);
      reentry_i++;
      if (reentry_i == (int)strlen(sample_data3)) {
        hal->packet_finished(type);
        return;
      }
    }

    return;
  }

  UNEXPECTED_CALL;
}

static void reset_for(TEST_MODES_T next) {
  RESET_CALL_COUNT(vendor_send_command);
  RESET_CALL_COUNT(data_ready_callback);
  CURRENT_TEST_MODE = next;
}

class HciHalH4Test : public AllocationTestHarness {
  protected:
    virtual void SetUp() {
      AllocationTestHarness::SetUp();
      hal = hci_hal_h4_get_test_interface(&vendor);
      vendor.send_command = vendor_send_command;
      callbacks.data_ready = data_ready_callback;

      socketpair(AF_LOCAL, SOCK_STREAM, 0, sockfd);
      dummy_serial_fd = sockfd[0];
      done = semaphore_new(0);
      thread = thread_new("hal_test");

      reset_for(init);
      EXPECT_TRUE(hal->init(&callbacks, thread));

      reset_for(open);
      EXPECT_TRUE(hal->open());
      EXPECT_CALL_COUNT(vendor_send_command, 1);
    }

    virtual void TearDown() {
      reset_for(close_fn);
      hal->close();
      EXPECT_CALL_COUNT(vendor_send_command, 1);

      semaphore_free(done);
      thread_free(thread);
      AllocationTestHarness::TearDown();
    }

    int sockfd[2];
    vendor_t vendor;
    thread_t *thread;
    hci_hal_callbacks_t callbacks;
};

static void expect_socket_data(int fd, char first_byte, char *data) {
  int length = strlen(data) + 1; // + 1 for data type code
  int i;

  for (i = 0; i < length; i++) {
    fd_set read_fds;
    FD_ZERO(&read_fds);
    FD_SET(fd, &read_fds);
    select(fd + 1, &read_fds, NULL, NULL, NULL);

    char byte;
    read(fd, &byte, 1);

    EXPECT_EQ(i == 0 ? first_byte : data[i - 1], byte);
  }
}

static void write_packet(int fd, char first_byte, const void *data,
                         size_t datalen) {
  write(fd, &first_byte, 1);
  write(fd, data, datalen);
}

static void write_packet_reentry(int fd, char first_byte, const void *data,
                                 size_t datalen) {
  write(fd, &first_byte, 1);

  for (size_t i = 0; i < datalen; i++) {
    write(fd, static_cast<const uint8_t *>(data) + i, 1);
    semaphore_wait(reentry_semaphore);
  }
}

TEST_F(HciHalH4Test, test_transmit) {
  reset_for(transmit);

  // Send a command packet
  hal->transmit_data(DATA_TYPE_COMMAND, (uint8_t *)(sample_data1 + 1), strlen(sample_data1 + 1));
  expect_socket_data(sockfd[1], DATA_TYPE_COMMAND, sample_data1 + 1);

  // Send an acl packet
  hal->transmit_data(DATA_TYPE_ACL, (uint8_t *)(sample_data2 + 1), strlen(sample_data2 + 1));
  expect_socket_data(sockfd[1], DATA_TYPE_ACL, sample_data2 + 1);

  // Send an sco packet
  hal->transmit_data(DATA_TYPE_SCO, (uint8_t *)(sample_data3 + 1), strlen(sample_data3 + 1));
  expect_socket_data(sockfd[1], DATA_TYPE_SCO, sample_data3 + 1);
}

TEST_F(HciHalH4Test, test_read_synchronous) {
  reset_for(read_synchronous);

  write_packet(sockfd[1], DATA_TYPE_ACL, acl_data, strlen(acl_data));
  write_packet(sockfd[1], HCI_BLE_EVENT, corrupted_data,
               sizeof(corrupted_data));
  write_packet(sockfd[1], DATA_TYPE_SCO, sco_data, strlen(sco_data));
  write_packet(sockfd[1], DATA_TYPE_EVENT, event_data, strlen(event_data));

  // Wait for all data to be received before calling the test good
  semaphore_wait(done);
  EXPECT_CALL_COUNT(data_ready_callback, 3);
}

TEST_F(HciHalH4Test, test_read_async_reentry) {
  reset_for(read_async_reentry);

  reentry_semaphore = semaphore_new(0);
  reentry_i = 0;

  write_packet_reentry(sockfd[1], DATA_TYPE_ACL, sample_data3,
                       strlen(sample_data3));

  // write_packet_reentry ensures the data has been received
  semaphore_free(reentry_semaphore);
}

TEST_F(HciHalH4Test, test_type_byte_only_must_not_signal_data_ready) {
  reset_for(type_byte_only);

  char byte = DATA_TYPE_ACL;
  write(sockfd[1], &byte, 1);

  fd_set read_fds;

  // Wait until the byte we wrote was picked up
  do {
    FD_ZERO(&read_fds);
    FD_SET(sockfd[0], &read_fds);

    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 0;

    select(sockfd[0] + 1, &read_fds, NULL, NULL, &timeout);
  } while(FD_ISSET(sockfd[0], &read_fds));
}
