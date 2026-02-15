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

#include "low_power_manager.h"
#include "osi/include/osi.h"
#include "osi/include/semaphore.h"
#include "osi/include/thread.h"
#include "test_stubs.h"
#include "vendor.h"
}

DECLARE_TEST_MODES(
  init,
  cleanup,
  enable_disable
);

static const low_power_manager_t *manager;
static thread_t *thread;
static semaphore_t *done;

static vendor_cb low_power_state_callback;

static void flush_work_queue_item(UNUSED_ATTR void *context) {
  semaphore_post(done);
}

STUB_FUNCTION(int, vendor_send_command, (vendor_opcode_t opcode, void *param))
  DURING(enable_disable) AT_CALL(0) {
    EXPECT_EQ(VENDOR_GET_LPM_IDLE_TIMEOUT, opcode);
    *((uint32_t *)param) = 100;
    return 0;
  }

  UNEXPECTED_CALL;
  return 0;
}

STUB_FUNCTION(int, vendor_send_async_command, (vendor_async_opcode_t opcode, void *param))
  DURING(enable_disable) {
    AT_CALL(0) {
      EXPECT_EQ(VENDOR_SET_LPM_MODE, opcode);
      EXPECT_EQ(BT_VND_LPM_ENABLE, *(uint8_t *)param);
      low_power_state_callback(true);
      thread_post(thread, flush_work_queue_item, NULL);
      return 0;
    }
    AT_CALL(1) {
      EXPECT_EQ(VENDOR_SET_LPM_MODE, opcode);
      EXPECT_EQ(BT_VND_LPM_DISABLE, *(uint8_t *)param);
      low_power_state_callback(true);
      thread_post(thread, flush_work_queue_item, NULL);
      return 0;
    }
  }

  UNEXPECTED_CALL;
  return 0;
}

STUB_FUNCTION(void, vendor_set_callback, (vendor_async_opcode_t opcode, vendor_cb callback))
  DURING(init) AT_CALL(0) {
    EXPECT_EQ(VENDOR_SET_LPM_MODE, opcode);
    low_power_state_callback = callback;
    return;
  }

  UNEXPECTED_CALL;
}

static void reset_for(TEST_MODES_T next) {
  RESET_CALL_COUNT(vendor_send_command);
  RESET_CALL_COUNT(vendor_send_async_command);
  RESET_CALL_COUNT(vendor_set_callback);
  CURRENT_TEST_MODE = next;
}

class LowPowerManagerTest : public AlarmTestHarness {
  protected:
    virtual void SetUp() {
      AlarmTestHarness::SetUp();
      low_power_state_callback = NULL;
      vendor.send_command = vendor_send_command;
      vendor.send_async_command = vendor_send_async_command;
      vendor.set_callback = vendor_set_callback;
      manager = low_power_manager_get_test_interface(&vendor);
      thread = thread_new("test_thread");
      done = semaphore_new(0);

      reset_for(init);
      manager->init(thread);

      EXPECT_CALL_COUNT(vendor_set_callback, 1);
    }

    virtual void TearDown() {
      reset_for(cleanup);
      manager->cleanup();

      semaphore_free(done);
      thread_free(thread);
      AlarmTestHarness::TearDown();
    }

    vendor_t vendor;
};

TEST_F(LowPowerManagerTest, test_enable_disable) {
  reset_for(enable_disable);
  manager->post_command(LPM_ENABLE);
  semaphore_wait(done);

  manager->post_command(LPM_DISABLE);
  semaphore_wait(done);

  EXPECT_CALL_COUNT(vendor_send_command, 1);
  EXPECT_CALL_COUNT(vendor_send_async_command, 2);
}
