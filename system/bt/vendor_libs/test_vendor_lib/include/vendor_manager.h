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

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread.h"
#include "base/time/time.h"
#include "hci/include/bt_vendor_lib.h"
#include "vendor_libs/test_vendor_lib/include/dual_mode_controller.h"
#include "vendor_libs/test_vendor_lib/include/event_packet.h"
#include "vendor_libs/test_vendor_lib/include/hci_transport.h"
#include "vendor_libs/test_vendor_lib/include/test_channel_transport.h"

#include <memory>

namespace test_vendor_lib {

// Contains the three core objects that make up the test vendor library: the
// HciTransport for communication, the HciHandler for processing commands, and
// the Controller for actual command implementations. The VendorManager shall
// operate as a global singleton and be used in bt_vendor.cc to perform vendor
// specific operations, via |vendor_callbacks_|, and to provide access to the
// test controller by setting up a message loop (on another thread) that the HCI
// will talk to and controller methods will execute on.
class VendorManager {
 public:
  // Functions that operate on the global manager instance. Initialize()
  // is called by the vendor library's TestVendorInitialize() function to create
  // the global manager and must be called before Get() and CleanUp().
  // CleanUp() should be called when a call to TestVendorCleanUp() is made
  // since the global manager should live throughout the entire time the test
  // vendor library is in use.
  static void CleanUp();

  static VendorManager* Get();

  static void Initialize();

  void CloseHciFd();

  int GetHciFd() const;

  const bt_vendor_callbacks_t& GetVendorCallbacks() const;

  // Stores a copy of the vendor specific configuration callbacks passed into
  // the vendor library from the HCI in TestVendorInit().
  void SetVendorCallbacks(const bt_vendor_callbacks_t& callbacks);

  // Returns true if |thread_| is able to be started and the
  // StartingWatchingOnThread() task has been posted to the task runner.
  bool Run();

 private:
  VendorManager();

  ~VendorManager() = default;

  // Posts a callback to |thread_|'s task runner. Equivalent to calling
  // |PostDelayedTask| with a delay of 0.
  bool PostTask(const base::Closure& task);

  // Posts a callback to be run after |delay| ms (or longer) have passed.
  bool PostDelayedTask(const base::Closure& task, base::TimeDelta delay);

  // Starts watching for incoming data from the HCI and the test hook.
  void StartWatchingOnThread();

  // Creates the HCI's communication channel and overrides IO callbacks to
  // receive and send packets.
  HciTransport transport_;

  // The controller object that provides implementations of Bluetooth commands.
  DualModeController controller_;

  // The two test channel objects that perform functions corresponding to the
  // HciTransport and HciHandler.
  TestChannelTransport test_channel_transport_;

  // Configuration callbacks provided by the HCI for use in TestVendorOp().
  bt_vendor_callbacks_t vendor_callbacks_;

  // True if the underlying message loop (in |thread_|) is running.
  bool running_;

  // Dedicated thread for managing the message loop to receive and send packets
  // from the HCI and to receive additional parameters from the test hook file
  // descriptor.
  base::Thread thread_;

  // Used to handle further watching of the vendor's/test channel's file
  // descriptor after WatchFileDescriptor() is called.
  base::MessageLoopForIO::FileDescriptorWatcher hci_watcher_;

  // Used to handle further watching of the test channel's file descriptor after
  // WatchFileDescriptor() is called.
  base::MessageLoopForIO::FileDescriptorWatcher test_channel_watcher_;

  // This should remain the last member so it'll be destroyed and invalidate
  // its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<VendorManager> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(VendorManager);
};

}  // namespace test_vendor_lib
