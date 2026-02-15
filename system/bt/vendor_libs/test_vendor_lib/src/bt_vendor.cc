//
// Copyright 2015 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#define LOG_TAG "bt_vendor"

#include "vendor_manager.h"

#include "base/logging.h"

extern "C" {
#include "osi/include/log.h"

#include <unistd.h>
}  // extern "C"

namespace test_vendor_lib {

// Initializes vendor manager for test controller. |p_cb| are the callbacks to
// be in TestVendorOp(). |local_bdaddr| points to the address of the Bluetooth
// device. Returns 0 on success, -1 on error.
static int TestVendorInitialize(const bt_vendor_callbacks_t* p_cb,
                                unsigned char* /* local_bdaddr */) {
  LOG_INFO(LOG_TAG, "Initializing test controller.");
  CHECK(p_cb);

  VendorManager::Initialize();
  VendorManager* manager = VendorManager::Get();
  manager->SetVendorCallbacks(*(const_cast<bt_vendor_callbacks_t*>(p_cb)));
  return manager->Run() ? 0 : -1;
}

// Vendor specific operations. |opcode| is the opcode for Bluedroid's vendor op
// definitions. |param| points to operation specific arguments. Return value is
// dependent on the operation invoked, or -1 on error.
static int TestVendorOp(bt_vendor_opcode_t opcode, void* param) {
  LOG_INFO(LOG_TAG, "Opcode received in vendor library: %d", opcode);

  VendorManager* manager = VendorManager::Get();
  CHECK(manager);

  switch (opcode) {
    case BT_VND_OP_POWER_CTRL: {
      LOG_INFO(LOG_TAG, "Doing op: BT_VND_OP_POWER_CTRL");
      int* state = static_cast<int*>(param);
      if (*state == BT_VND_PWR_OFF)
        LOG_INFO(LOG_TAG, "Turning Bluetooth off.");
      else if (*state == BT_VND_PWR_ON)
        LOG_INFO(LOG_TAG, "Turning Bluetooth on.");
      return 0;
    }

    // Give the HCI its fd to communicate with the HciTransport.
    case BT_VND_OP_USERIAL_OPEN: {
      LOG_INFO(LOG_TAG, "Doing op: BT_VND_OP_USERIAL_OPEN");
      int* fd_list = static_cast<int*>(param);
      fd_list[0] = manager->GetHciFd();
      LOG_INFO(LOG_TAG, "Setting HCI's fd to: %d", fd_list[0]);
      return 1;
    }

    // Close the HCI's file descriptor.
    case BT_VND_OP_USERIAL_CLOSE:
      LOG_INFO(LOG_TAG, "Doing op: BT_VND_OP_USERIAL_CLOSE");
      LOG_INFO(LOG_TAG, "Closing HCI's fd (fd: %d)", manager->GetHciFd());
      manager->CloseHciFd();
      return 1;

    case BT_VND_OP_FW_CFG:
      LOG_INFO(LOG_TAG, "Unsupported op: BT_VND_OP_FW_CFG");
      manager->GetVendorCallbacks().fwcfg_cb(BT_VND_OP_RESULT_FAIL);
      return -1;

    default:
      LOG_INFO(LOG_TAG, "Op not recognized.");
      return -1;
  }
  return 0;
}

// Closes the vendor interface and cleans up the global vendor manager object.
static void TestVendorCleanUp(void) {
  LOG_INFO(LOG_TAG, "Cleaning up vendor library.");
  VendorManager::CleanUp();
}

}  // namespace test_vendor_lib

// Entry point of DLib.
const bt_vendor_interface_t BLUETOOTH_VENDOR_LIB_INTERFACE = {
  sizeof(bt_vendor_interface_t),
  test_vendor_lib::TestVendorInitialize,
  test_vendor_lib::TestVendorOp,
  test_vendor_lib::TestVendorCleanUp
};
