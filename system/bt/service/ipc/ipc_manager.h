//
//  Copyright (C) 2015 Google, Inc.
//
//  Licensed under the Apache License, Version 2.0 (the "License");
//  you may not use this file except in compliance with the License.
//  You may obtain a copy of the License at:
//
//  http://www.apache.org/licenses/LICENSE-2.0
//
//  Unless required by applicable law or agreed to in writing, software
//  distributed under the License is distributed on an "AS IS" BASIS,
//  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//  See the License for the specific language governing permissions and
//  limitations under the License.
//

#pragma once

#include <memory>

#include <base/macros.h>
#include <base/memory/ref_counted.h>

namespace bluetooth {
class Adapter;
}  // namespace bluetooth

namespace ipc {

class IPCHandler;

// IPCManager is a class for initializing and running supported IPC mechanisms.
// It manages the life-time of different IPC flavors that are available on the
// system. There are two flavors: a Linux sequential packet domain socket based
// system and one based on the Binder-based android.bluetooth framework.
class IPCManager {
 public:
  // Possible IPC types.
  enum Type {
    TYPE_LINUX,  // IPC based on a Linux sequential packet domain socket
    TYPE_BINDER  // IPC based on the Binder
  };

  // Interface for observing events from an IPC mechanism. These methods will be
  // called on the thread that started the particular IPC type.
  class Delegate {
   public:
    Delegate() = default;
    virtual ~Delegate() = default;

    // Called when an IPC mechanism has successfully started and is ready for
    // client connections.
    virtual void OnIPCHandlerStarted(Type type) = 0;

    // Called when an IPC mechanism has stopped. This may happen due to an error
    // in initialization or due to a regular shut down routine.
    virtual void OnIPCHandlerStopped(Type type) = 0;

   private:
    DISALLOW_COPY_AND_ASSIGN(Delegate);
  };

  explicit IPCManager(bluetooth::Adapter* adapter);
  ~IPCManager();

  // Initialize the underlying IPC handler based on |type|, if that type has not
  // yet been initialized and returns true on success. Returns false if that
  // type has already been initialized or an error occurs.
  //
  // If TYPE_LINUX is given, the file path to use for the domain socket will be
  // obtained from the global Settings object. Hence, the Settings object must
  // have been initialized before calling this method.
  //
  // |delegate| must out-live the IPCManager and the underlying handler. Users
  // can guarantee proper clean up by deallocating |delegate| when or after
  // Delegate::OnIPCHandlerStopped is called. It is safe to destroy |delegate|
  // after destroying the IPCManager instance, as the destructor will join and
  // clean up all underlying threads.
  bool Start(Type type, Delegate* delegate);

  // Returns true if an IPC type has been initialized.
  bool BinderStarted() const;
  bool LinuxStarted() const;

 private:
  IPCManager() = default;

  // Pointers to the different IPC handler classes. These are initialized and
  // owned by us.
  scoped_refptr<IPCHandler> binder_handler_;
  scoped_refptr<IPCHandler> linux_handler_;

  // The Bluetooth adapter instance. This is owned by Daemon so we keep a raw
  // pointer to it.
  bluetooth::Adapter* adapter_;

  DISALLOW_COPY_AND_ASSIGN(IPCManager);
};

}  // namespace ipc
