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
  read_async_reentry
);

static char sample_data1[100] = "A point is that which has no part.";
static char sample_data2[100] = "A line is breadthless length.";
static char sample_data3[100] = "The ends of a line are points.";
static char acl_data[100] =     "A straight line is a line which lies evenly with the points on itself.";
static char event_data[100] =   "The edges of a surface are lines.";

static const hci_hal_t *hal;
static int command_out_fd;
static int acl_out_fd;
static int acl_in_fd;
static int event_in_fd;
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
    ((int *)param)[CH_CMD] = command_out_fd;
    ((int *)param)[CH_ACL_OUT] = acl_out_fd;
    ((int *)param)[CH_ACL_IN] = acl_in_fd;
    ((int *)param)[CH_EVT] = event_in_fd;
    return 4;
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
      semaphore_post(done);
      return;
    }
    AT_CALL(1) {
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

class HciHalMctTest : public AllocationTestHarness {
  protected:
    virtual void SetUp() {
      AllocationTestHarness::SetUp();
      hal = hci_hal_mct_get_test_interface(&vendor);
      vendor.send_command = vendor_send_command;
      callbacks.data_ready = data_ready_callback;

      socketpair(AF_LOCAL, SOCK_STREAM, 0, command_sockfd);
      socketpair(AF_LOCAL, SOCK_STREAM, 0, event_sockfd);
      socketpair(AF_LOCAL, SOCK_STREAM, 0, acl_in_sockfd);
      socketpair(AF_LOCAL, SOCK_STREAM, 0, acl_out_sockfd);
      command_out_fd = command_sockfd[0];
      acl_out_fd = acl_out_sockfd[0];
      acl_in_fd = acl_in_sockfd[0];
      event_in_fd = event_sockfd[0];

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

    int command_sockfd[2];
    int event_sockfd[2];
    int acl_in_sockfd[2];
    int acl_out_sockfd[2];
    vendor_t vendor;
    thread_t *thread;
    hci_hal_callbacks_t callbacks;
};

static void expect_socket_data(int fd, char *data) {
  int length = strlen(data);
  int i;

  for (i = 0; i < length; i++) {
    fd_set read_fds;
    FD_ZERO(&read_fds);
    FD_SET(fd, &read_fds);
    select(fd + 1, &read_fds, NULL, NULL, NULL);

    char byte;
    read(fd, &byte, 1);

    EXPECT_EQ(data[i], byte);
  }
}

static void write_packet(int fd, char *data) {
  write(fd, data, strlen(data));
}

static void write_packet_reentry(int fd, char *data) {
  int length = strlen(data);
  for (int i = 0; i < length; i++) {
    write(fd, &data[i], 1);
    semaphore_wait(reentry_semaphore);
  }
}

TEST_F(HciHalMctTest, test_transmit) {
  reset_for(transmit);

  // Send a command packet
  hal->transmit_data(DATA_TYPE_COMMAND, (uint8_t *)(sample_data1), strlen(sample_data1));
  expect_socket_data(command_sockfd[1], sample_data1);

  // Send an acl packet
  hal->transmit_data(DATA_TYPE_ACL, (uint8_t *)(sample_data2), strlen(sample_data2));
  expect_socket_data(acl_out_sockfd[1], sample_data2);
}

TEST_F(HciHalMctTest, test_read_synchronous) {
  reset_for(read_synchronous);

  write_packet(acl_in_sockfd[1], acl_data);
  semaphore_wait(done);

  write_packet(event_sockfd[1], event_data);
  semaphore_wait(done);

  EXPECT_CALL_COUNT(data_ready_callback, 2);
}

TEST_F(HciHalMctTest, test_read_async_reentry) {
  reset_for(read_async_reentry);

  reentry_semaphore = semaphore_new(0);
  reentry_i = 0;

  write_packet_reentry(acl_in_sockfd[1], sample_data3);

  // write_packet_reentry ensures the data has been received
  semaphore_free(reentry_semaphore);
}
