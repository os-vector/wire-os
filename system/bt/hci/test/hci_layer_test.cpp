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

#include "AlarmTestHarness.h"

extern "C" {
#include <stdint.h>

#include "device/include/controller.h"
#include "osi/include/allocation_tracker.h"
#include "osi/include/allocator.h"
#include "osi/include/osi.h"
#include "osi/include/semaphore.h"
#include "btsnoop.h"
#include "hcimsgs.h"
#include "hci_hal.h"
#include "hci_inject.h"
#include "hci_layer.h"
#include "low_power_manager.h"
#include "module.h"
#include "packet_fragmenter.h"
#include "test_stubs.h"
#include "vendor.h"

extern const module_t hci_module;
}

DECLARE_TEST_MODES(
  start_up_async,
  shut_down,
  postload,
  transmit_simple,
  receive_simple,
  transmit_command_no_callbacks,
  transmit_command_command_status,
  transmit_command_command_complete,
  ignoring_packets_ignored_packet,
  ignoring_packets_following_packet
);

static const char *small_sample_data = "\"It is easy to see,\" replied Don Quixote";
static const char *command_sample_data = "that thou art not used to this business of adventures; those are giants";
static const char *ignored_data = "and if thou art afraid, away with thee out of this and betake thyself to prayer";
static const char *unignored_data = "while I engage them in fierce and unequal combat";

static const hci_t *hci;
static const hci_hal_callbacks_t *hal_callbacks;
static thread_t *internal_thread;
static vendor_cb firmware_config_callback;
static vendor_cb sco_config_callback;
static vendor_cb epilog_callback;
static semaphore_t *done;
static const uint16_t test_handle = (0x1992 & 0xCFFF);
static const uint16_t test_handle_continuation = (0x1992 & 0xCFFF) | 0x1000;
static int packet_index;
static unsigned int data_size_sum;
static BT_HDR *data_to_receive;

static void signal_work_item(UNUSED_ATTR void *context) {
  semaphore_post(done);
}

static void flush_thread(thread_t *thread) {
  // Double flush to ensure we get the next reactor cycle
  thread_post(thread, signal_work_item, NULL);
  semaphore_wait(done);
  thread_post(thread, signal_work_item, NULL);
  semaphore_wait(done);
}

// TODO move this to a common packet testing helper
static BT_HDR *manufacture_packet(uint16_t event, const char *data) {
  uint16_t data_length = strlen(data);
  uint16_t size = data_length;
  if (event == MSG_STACK_TO_HC_HCI_ACL) {
    size += 4; // 2 for the handle, 2 for the length;
  }

  BT_HDR *packet = (BT_HDR *)osi_malloc(size + sizeof(BT_HDR));
  packet->len = size;
  packet->offset = 0;
  packet->layer_specific = 0;

  // The command transmit interface adds the event type automatically.
  // Make sure it works but omitting it here.
  if (event != MSG_STACK_TO_HC_HCI_CMD)
    packet->event = event;

  uint8_t *packet_data = packet->data;

  if (event == MSG_STACK_TO_HC_HCI_ACL) {
    UINT16_TO_STREAM(packet_data, test_handle);
    UINT16_TO_STREAM(packet_data, data_length);
  }

  for (int i = 0; i < data_length; i++) {
    packet_data[i] = data[i];
  }

  if (event == MSG_STACK_TO_HC_HCI_CMD) {
    STREAM_SKIP_UINT16(packet_data);
    UINT8_TO_STREAM(packet_data, data_length - 3);
  } else if (event == MSG_HC_TO_STACK_HCI_EVT) {
    STREAM_SKIP_UINT8(packet_data);
    UINT8_TO_STREAM(packet_data, data_length - 2);
  }

  return packet;
}

static void expect_packet(uint16_t event, int max_acl_data_size, const uint8_t *data, uint16_t data_length, const char *expected_data) {
  int expected_data_offset;
  int length_to_check;

  if (event == MSG_STACK_TO_HC_HCI_ACL) {
    uint16_t handle;
    uint16_t length;
    STREAM_TO_UINT16(handle, data);
    STREAM_TO_UINT16(length, data);

    if (packet_index == 0)
      EXPECT_EQ(test_handle, handle);
    else
      EXPECT_EQ(test_handle_continuation, handle);

    int length_remaining = strlen(expected_data) - data_size_sum;
    int packet_data_length = data_length - HCI_ACL_PREAMBLE_SIZE;
    EXPECT_EQ(length_remaining, length);

    if (length_remaining < max_acl_data_size)
      EXPECT_EQ(length, packet_data_length);
    else
      EXPECT_EQ(max_acl_data_size, packet_data_length);

    length_to_check = packet_data_length;
    expected_data_offset = packet_index * max_acl_data_size;
    packet_index++;
  } else {
    length_to_check = strlen(expected_data);
    expected_data_offset = 0;
  }

  for (int i = 0; i < length_to_check; i++) {
    if (event == MSG_STACK_TO_HC_HCI_CMD && (i == 2))
      EXPECT_EQ(data_length - 3, data[i]);
    else
      EXPECT_EQ(expected_data[expected_data_offset + i], data[i]);

    data_size_sum++;
  }
}

STUB_FUNCTION(bool, hal_init, (const hci_hal_callbacks_t *callbacks, thread_t *working_thread))
  DURING(start_up_async) AT_CALL(0) {
    hal_callbacks = callbacks;
    internal_thread = working_thread;
    return true;
  }

  UNEXPECTED_CALL;
  return false;
}

STUB_FUNCTION(bool, hal_open, ())
  DURING(start_up_async) AT_CALL(0) return true;
  UNEXPECTED_CALL;
  return false;
}

STUB_FUNCTION(void, hal_close, ())
  DURING(shut_down) AT_CALL(0) return;
  UNEXPECTED_CALL;
}

STUB_FUNCTION(uint16_t, hal_transmit_data, (serial_data_type_t type, uint8_t *data, uint16_t length))
  DURING(transmit_simple) AT_CALL(0) {
    EXPECT_EQ(DATA_TYPE_ACL, type);
    expect_packet(MSG_STACK_TO_HC_HCI_ACL, 1021, data, length, small_sample_data);
    return length;
  }

  DURING(
      transmit_command_no_callbacks,
      transmit_command_command_status,
      transmit_command_command_complete
    ) AT_CALL(0) {
    EXPECT_EQ(DATA_TYPE_COMMAND, type);
    expect_packet(MSG_STACK_TO_HC_HCI_CMD, 1021, data, length, command_sample_data);
    return length;
  }

  UNEXPECTED_CALL;
  return 0;
}

static size_t replay_data_to_receive(size_t max_size, uint8_t *buffer) {
  for (size_t i = 0; i < max_size; i++) {
    if (data_to_receive->offset >= data_to_receive->len)
      break;

    buffer[i] = data_to_receive->data[data_to_receive->offset++];

    if (i == (max_size - 1))
      return i + 1; // We return the length, not the index;
  }

  return 0;
}

STUB_FUNCTION(size_t, hal_read_data, (serial_data_type_t type, uint8_t *buffer, size_t max_size))
  DURING(receive_simple, ignoring_packets_following_packet) {
    EXPECT_EQ(DATA_TYPE_ACL, type);
    return replay_data_to_receive(max_size, buffer);
  }

  DURING(ignoring_packets_ignored_packet) {
    EXPECT_EQ(DATA_TYPE_EVENT, type);
    return replay_data_to_receive(max_size, buffer);
  }

  DURING(
      transmit_command_no_callbacks,
      transmit_command_command_status,
      transmit_command_command_complete) {
    EXPECT_EQ(DATA_TYPE_EVENT, type);
    return replay_data_to_receive(max_size, buffer);
  }

  UNEXPECTED_CALL;
  return 0;
}

STUB_FUNCTION(void, hal_packet_finished, (serial_data_type_t type))
  DURING(receive_simple, ignoring_packets_following_packet) AT_CALL(0) {
    EXPECT_EQ(DATA_TYPE_ACL, type);
    return;
  }

  DURING(ignoring_packets_ignored_packet) AT_CALL(0) {
    EXPECT_EQ(DATA_TYPE_EVENT, type);
    return;
  }

  DURING(
      transmit_command_no_callbacks,
      transmit_command_command_status,
      transmit_command_command_complete
    ) AT_CALL(0) {
    EXPECT_EQ(DATA_TYPE_EVENT, type);
    return;
  }

  UNEXPECTED_CALL;
}

STUB_FUNCTION(bool, hci_inject_open, (
    UNUSED_ATTR const hci_t *hci_interface))
  DURING(start_up_async) AT_CALL(0) return true;
  UNEXPECTED_CALL;
  return false;
}

STUB_FUNCTION(void, hci_inject_close, ())
  DURING(shut_down) AT_CALL(0) return;
  UNEXPECTED_CALL;
}

STUB_FUNCTION(void, btsnoop_capture, (const BT_HDR *buffer, bool is_received))
  DURING(transmit_simple) AT_CALL(0) {
    EXPECT_FALSE(is_received);
    expect_packet(MSG_STACK_TO_HC_HCI_ACL, 1021, buffer->data + buffer->offset, buffer->len, small_sample_data);
    packet_index = 0;
    data_size_sum = 0;
    return;
  }


  DURING(
      transmit_command_no_callbacks,
      transmit_command_command_status,
      transmit_command_command_complete) {
    AT_CALL(0) {
      EXPECT_FALSE(is_received);
      expect_packet(MSG_STACK_TO_HC_HCI_CMD, 1021, buffer->data + buffer->offset, buffer->len, command_sample_data);
      packet_index = 0;
      data_size_sum = 0;
      return;
    }
    AT_CALL(1) {
      EXPECT_TRUE(is_received);
      // not super important to verify the contents right now
      return;
    }
  }

  DURING(
      receive_simple,
      ignoring_packets_following_packet
    ) AT_CALL(0) {
    EXPECT_TRUE(is_received);
    EXPECT_TRUE(buffer->len == data_to_receive->len);
    const uint8_t *buffer_base = buffer->data + buffer->offset;
    const uint8_t *expected_base = data_to_receive->data;
    for (int i = 0; i < buffer->len; i++) {
      EXPECT_EQ(expected_base[i], buffer_base[i]);
    }

    return;
  }

  UNEXPECTED_CALL;
}

STUB_FUNCTION(void, low_power_init, (UNUSED_ATTR thread_t *thread))
  DURING(start_up_async) AT_CALL(0) return;
  UNEXPECTED_CALL;
}

STUB_FUNCTION(void, low_power_cleanup, ())
  DURING(shut_down) AT_CALL(0) return;
  UNEXPECTED_CALL;
}

STUB_FUNCTION(void, low_power_wake_assert, ())
  DURING(
      transmit_simple,
      transmit_command_no_callbacks,
      transmit_command_command_status,
      transmit_command_command_complete) {
    AT_CALL(0) return;
  }

  UNEXPECTED_CALL;
}

STUB_FUNCTION(void, low_power_transmit_done, ())
  DURING(
      transmit_simple,
      transmit_command_no_callbacks,
      transmit_command_command_status,
      transmit_command_command_complete) {
    AT_CALL(0) return;
  }

  UNEXPECTED_CALL;
}

STUB_FUNCTION(bool, vendor_open, (UNUSED_ATTR const uint8_t *addr, const hci_t *hci_interface))
  DURING(start_up_async) AT_CALL(0) {
    // TODO(zachoverflow): check address value when it gets put into a module
    EXPECT_EQ(hci, hci_interface);
    return true;
  }

  UNEXPECTED_CALL;
  return true;
}

STUB_FUNCTION(void, vendor_close, ())
  DURING(shut_down) AT_CALL(0) return;
  UNEXPECTED_CALL;
}

STUB_FUNCTION(void, vendor_set_callback, (vendor_async_opcode_t opcode, UNUSED_ATTR vendor_cb callback))
  DURING(start_up_async) {
    AT_CALL(0) {
      EXPECT_EQ(VENDOR_CONFIGURE_FIRMWARE, opcode);
      firmware_config_callback = callback;
      return;
    }
    AT_CALL(1) {
      EXPECT_EQ(VENDOR_CONFIGURE_SCO, opcode);
      sco_config_callback = callback;
      return;
    }
    AT_CALL(2) {
      EXPECT_EQ(VENDOR_DO_EPILOG, opcode);
      epilog_callback = callback;
      return;
    }
  }

  UNEXPECTED_CALL;
}

STUB_FUNCTION(int, vendor_send_command, (vendor_opcode_t opcode, void *param))
  DURING(start_up_async) {
#if (defined (BT_CLEAN_TURN_ON_DISABLED) && BT_CLEAN_TURN_ON_DISABLED == TRUE)
    AT_CALL(0) {
      EXPECT_EQ(VENDOR_CHIP_POWER_CONTROL, opcode);
      EXPECT_EQ(BT_VND_PWR_ON, *(int *)param);
      return 0;
    }
#else
    AT_CALL(0) {
      EXPECT_EQ(VENDOR_CHIP_POWER_CONTROL, opcode);
      EXPECT_EQ(BT_VND_PWR_OFF, *(int *)param);
      return 0;
    }
    AT_CALL(1) {
      EXPECT_EQ(VENDOR_CHIP_POWER_CONTROL, opcode);
      EXPECT_EQ(BT_VND_PWR_ON, *(int *)param);
      return 0;
    }
#endif
  }

  DURING(shut_down) AT_CALL(0) {
    EXPECT_EQ(VENDOR_CHIP_POWER_CONTROL, opcode);
    EXPECT_EQ(BT_VND_PWR_OFF, *(int *)param);
    return 0;
  }

  UNEXPECTED_CALL;
  return 0;
}

STUB_FUNCTION(int, vendor_send_async_command, (UNUSED_ATTR vendor_async_opcode_t opcode, UNUSED_ATTR void *param))
  DURING(start_up_async) AT_CALL(0) {
    EXPECT_EQ(VENDOR_CONFIGURE_FIRMWARE, opcode);
    firmware_config_callback(true);
    return 0;
  }

  DURING(postload) AT_CALL(0) {
    EXPECT_EQ(VENDOR_CONFIGURE_SCO, opcode);
    sco_config_callback(true);
    return 0;
  }

  DURING(shut_down) AT_CALL(0) {
    EXPECT_EQ(VENDOR_DO_EPILOG, opcode);
    epilog_callback(true);
    return 0;
  }

  UNEXPECTED_CALL;
  return 0;
}

STUB_FUNCTION(void, command_complete_callback, (BT_HDR *response, UNUSED_ATTR void *context))
  DURING(transmit_command_command_complete) AT_CALL(0) {
    osi_free(response);
    return;
  }

  UNEXPECTED_CALL;
}

STUB_FUNCTION(void, command_status_callback, (UNUSED_ATTR uint8_t status, BT_HDR *command, UNUSED_ATTR void *context))
  DURING(transmit_command_command_status) AT_CALL(0) {
    osi_free(command);
    return;
  }

  UNEXPECTED_CALL;
}

STUB_FUNCTION(uint16_t, controller_get_acl_data_size_classic, (void))
  return 2048;
}

STUB_FUNCTION(uint16_t, controller_get_acl_data_size_ble, (void))
  return 2048;
}

STUB_FUNCTION(void *, buffer_allocator_alloc, (size_t size))
  DURING(ignoring_packets_ignored_packet) {
    AT_CALL(0)
      return NULL;

    UNEXPECTED_CALL;
  }

  return allocator_malloc.alloc(size);
}

STUB_FUNCTION(void, buffer_allocator_free, (void *ptr))
  allocator_malloc.free(ptr);
}

static void reset_for(TEST_MODES_T next) {
  RESET_CALL_COUNT(vendor_open);
  RESET_CALL_COUNT(vendor_close);
  RESET_CALL_COUNT(vendor_set_callback);
  RESET_CALL_COUNT(vendor_send_command);
  RESET_CALL_COUNT(vendor_send_async_command);
  RESET_CALL_COUNT(hal_init);
  RESET_CALL_COUNT(hal_open);
  RESET_CALL_COUNT(hal_close);
  RESET_CALL_COUNT(hal_read_data);
  RESET_CALL_COUNT(hal_packet_finished);
  RESET_CALL_COUNT(hal_transmit_data);
  RESET_CALL_COUNT(btsnoop_capture);
  RESET_CALL_COUNT(hci_inject_open);
  RESET_CALL_COUNT(hci_inject_close);
  RESET_CALL_COUNT(low_power_init);
  RESET_CALL_COUNT(low_power_cleanup);
  RESET_CALL_COUNT(low_power_wake_assert);
  RESET_CALL_COUNT(low_power_transmit_done);
  RESET_CALL_COUNT(command_complete_callback);
  RESET_CALL_COUNT(command_status_callback);
  RESET_CALL_COUNT(controller_get_acl_data_size_classic);
  RESET_CALL_COUNT(controller_get_acl_data_size_ble);
  RESET_CALL_COUNT(buffer_allocator_alloc);
  RESET_CALL_COUNT(buffer_allocator_free);
  CURRENT_TEST_MODE = next;
}

class HciLayerTest : public AlarmTestHarness {
  protected:
    virtual void SetUp() {
      AlarmTestHarness::SetUp();
      module_management_start();

      hci = hci_layer_get_test_interface(
        &buffer_allocator,
        &hal,
        &btsnoop,
        &hci_inject,
        packet_fragmenter_get_test_interface(&controller, &allocator_malloc),
        &vendor,
        &low_power_manager
      );

      packet_index = 0;
      data_size_sum = 0;

      vendor.open = vendor_open;
      vendor.close = vendor_close;
      vendor.set_callback = vendor_set_callback;
      vendor.send_command = vendor_send_command;
      vendor.send_async_command = vendor_send_async_command;
      hal.init = hal_init;
      hal.open = hal_open;
      hal.close = hal_close;
      hal.read_data = hal_read_data;
      hal.packet_finished = hal_packet_finished;
      hal.transmit_data = hal_transmit_data;
      btsnoop.capture = btsnoop_capture;
      hci_inject.open = hci_inject_open;
      hci_inject.close = hci_inject_close;
      low_power_manager.init = low_power_init;
      low_power_manager.cleanup = low_power_cleanup;
      low_power_manager.wake_assert = low_power_wake_assert;
      low_power_manager.transmit_done = low_power_transmit_done;
      controller.get_acl_data_size_classic = controller_get_acl_data_size_classic;
      controller.get_acl_data_size_ble = controller_get_acl_data_size_ble;
      buffer_allocator.alloc = buffer_allocator_alloc;
      buffer_allocator.free = buffer_allocator_free;

      done = semaphore_new(0);

      reset_for(start_up_async);
      EXPECT_TRUE(module_start_up(&hci_module));

      EXPECT_CALL_COUNT(vendor_open, 1);
      EXPECT_CALL_COUNT(hal_init, 1);
      EXPECT_CALL_COUNT(low_power_init, 1);
      EXPECT_CALL_COUNT(vendor_set_callback, 3);
      EXPECT_CALL_COUNT(hal_open, 1);
      EXPECT_CALL_COUNT(vendor_send_async_command, 1);
    }

    virtual void TearDown() {
      reset_for(shut_down);
      module_shut_down(&hci_module);

      EXPECT_CALL_COUNT(low_power_cleanup, 1);
      EXPECT_CALL_COUNT(hal_close, 1);
      EXPECT_CALL_COUNT(vendor_send_command, 1);
      EXPECT_CALL_COUNT(vendor_close, 1);

      semaphore_free(done);
      hci_layer_cleanup_interface();
      module_management_stop();
      AlarmTestHarness::TearDown();
    }

    hci_hal_t hal;
    btsnoop_t btsnoop;
    controller_t controller;
    hci_inject_t hci_inject;
    vendor_t vendor;
    low_power_manager_t low_power_manager;
    allocator_t buffer_allocator;
};

TEST_F(HciLayerTest, test_postload) {
  reset_for(postload);
  hci->do_postload();

  flush_thread(internal_thread);
  EXPECT_CALL_COUNT(vendor_send_async_command, 1);
}

TEST_F(HciLayerTest, test_transmit_simple) {
  reset_for(transmit_simple);
  BT_HDR *packet = manufacture_packet(MSG_STACK_TO_HC_HCI_ACL, small_sample_data);
  hci->transmit_downward(MSG_STACK_TO_HC_HCI_ACL, packet);

  flush_thread(internal_thread);
  EXPECT_CALL_COUNT(hal_transmit_data, 1);
  EXPECT_CALL_COUNT(btsnoop_capture, 1);
  EXPECT_CALL_COUNT(low_power_transmit_done, 1);
  EXPECT_CALL_COUNT(low_power_wake_assert, 1);
}

TEST_F(HciLayerTest, test_receive_simple) {
  reset_for(receive_simple);
  data_to_receive = manufacture_packet(MSG_STACK_TO_HC_HCI_ACL, small_sample_data);

  // Not running on the internal thread, unlike the real hal
  hal_callbacks->data_ready(DATA_TYPE_ACL);
  EXPECT_CALL_COUNT(hal_packet_finished, 1);
  EXPECT_CALL_COUNT(btsnoop_capture, 1);

  osi_free(data_to_receive);
}

static BT_HDR *manufacture_command_complete(command_opcode_t opcode) {
  BT_HDR *ret = (BT_HDR *)osi_calloc(sizeof(BT_HDR) + 5);
  uint8_t *stream = ret->data;
  UINT8_TO_STREAM(stream, HCI_COMMAND_COMPLETE_EVT);
  UINT8_TO_STREAM(stream, 3); // length of the event parameters
  UINT8_TO_STREAM(stream, 1); // the number of commands that can be sent
  UINT16_TO_STREAM(stream, opcode);
  ret->len = 5;

  return ret;
}

static BT_HDR *manufacture_command_status(command_opcode_t opcode) {
  BT_HDR *ret = (BT_HDR *)osi_calloc(sizeof(BT_HDR) + 6);
  uint8_t *stream = ret->data;
  UINT8_TO_STREAM(stream, HCI_COMMAND_STATUS_EVT);
  UINT8_TO_STREAM(stream, 4); // length of the event parameters
  UINT8_TO_STREAM(stream, HCI_PENDING); // status
  UINT8_TO_STREAM(stream, 1); // the number of commands that can be sent
  UINT16_TO_STREAM(stream, opcode);
  ret->len = 6;

  return ret;
}

TEST_F(HciLayerTest, test_transmit_command_no_callbacks) {
  // Send a test command
  reset_for(transmit_command_no_callbacks);
  data_to_receive = manufacture_packet(MSG_STACK_TO_HC_HCI_CMD, command_sample_data);
  hci->transmit_command(data_to_receive, NULL, NULL, NULL);

  flush_thread(internal_thread);
  EXPECT_CALL_COUNT(hal_transmit_data, 1);
  EXPECT_CALL_COUNT(btsnoop_capture, 1);
  EXPECT_CALL_COUNT(low_power_transmit_done, 1);
  EXPECT_CALL_COUNT(low_power_wake_assert, 1);

  // Send a response
  command_opcode_t opcode = *((uint16_t *)command_sample_data);
  data_to_receive = manufacture_command_complete(opcode);

  hal_callbacks->data_ready(DATA_TYPE_EVENT);
  EXPECT_CALL_COUNT(hal_packet_finished, 1);
  EXPECT_CALL_COUNT(btsnoop_capture, 2);

  osi_free(data_to_receive);
}

TEST_F(HciLayerTest, test_transmit_command_command_status) {
  // Send a test command
  reset_for(transmit_command_command_status);
  data_to_receive = manufacture_packet(MSG_STACK_TO_HC_HCI_CMD, command_sample_data);
  hci->transmit_command(data_to_receive, command_complete_callback, command_status_callback, NULL);

  flush_thread(internal_thread);
  EXPECT_CALL_COUNT(hal_transmit_data, 1);
  EXPECT_CALL_COUNT(btsnoop_capture, 1);
  EXPECT_CALL_COUNT(low_power_transmit_done, 1);
  EXPECT_CALL_COUNT(low_power_wake_assert, 1);

  command_opcode_t opcode = *((uint16_t *)command_sample_data);

  // Send status event response
  data_to_receive = manufacture_command_status(opcode);

  hal_callbacks->data_ready(DATA_TYPE_EVENT);
  EXPECT_CALL_COUNT(hal_packet_finished, 1);
  EXPECT_CALL_COUNT(btsnoop_capture, 2);
  EXPECT_CALL_COUNT(command_status_callback, 1);

  osi_free(data_to_receive);
}

TEST_F(HciLayerTest, test_transmit_command_command_complete) {
  // Send a test command
  reset_for(transmit_command_command_complete);
  data_to_receive = manufacture_packet(MSG_STACK_TO_HC_HCI_CMD, command_sample_data);
  hci->transmit_command(data_to_receive, command_complete_callback, command_status_callback, NULL);

  flush_thread(internal_thread);
  EXPECT_CALL_COUNT(hal_transmit_data, 1);
  EXPECT_CALL_COUNT(btsnoop_capture, 1);
  EXPECT_CALL_COUNT(low_power_transmit_done, 1);
  EXPECT_CALL_COUNT(low_power_wake_assert, 1);

  command_opcode_t opcode = *((uint16_t *)command_sample_data);

  // Send complete event response
  data_to_receive = manufacture_command_complete(opcode);

  hal_callbacks->data_ready(DATA_TYPE_EVENT);
  EXPECT_CALL_COUNT(hal_packet_finished, 1);
  EXPECT_CALL_COUNT(btsnoop_capture, 2);
  EXPECT_CALL_COUNT(command_complete_callback, 1);

  osi_free(data_to_receive);
}

TEST_F(HciLayerTest, test_ignoring_packets) {
  reset_for(ignoring_packets_ignored_packet);
  data_to_receive = manufacture_packet(MSG_HC_TO_STACK_HCI_EVT, unignored_data);

  hal_callbacks->data_ready(DATA_TYPE_EVENT);
  EXPECT_CALL_COUNT(buffer_allocator_alloc, 1);
  EXPECT_CALL_COUNT(hal_packet_finished, 1);
  EXPECT_CALL_COUNT(btsnoop_capture, 0);
  osi_free(data_to_receive);

  reset_for(ignoring_packets_following_packet);
  data_to_receive = manufacture_packet(MSG_STACK_TO_HC_HCI_ACL, ignored_data);

  hal_callbacks->data_ready(DATA_TYPE_ACL);
  EXPECT_CALL_COUNT(buffer_allocator_alloc, 1);
  EXPECT_CALL_COUNT(hal_packet_finished, 1);
  EXPECT_CALL_COUNT(btsnoop_capture, 1);
  osi_free(data_to_receive);
}

// TODO(zachoverflow): test post-reassembly better, stub out fragmenter instead of using it
