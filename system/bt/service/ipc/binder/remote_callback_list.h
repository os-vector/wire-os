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

#include <functional>
#include <mutex>
#include <unordered_map>

#include <base/logging.h>
#include <base/macros.h>
#include <binder/IBinder.h>
#include <binder/IInterface.h>

namespace ipc {
namespace binder {

// Takes care of the grunt work of maintaining a list of remote interfaces,
// typically for the use of performing registered callbacks from a remote
// service. This is a native equivalent of the the android.os.RemoteCallbackList
// Java class. The type "T" must inherit from android::IInterface.
//
// TODO(armansito): We need to unit test this class. Right now it's defined as a
// simple template interface over the native libbinder types directly and we
// can't compile libbinder for host unless the Binder kernel module is enabled
// on the system. Figure out whether to:
//    1) write the Binder test-code in a new target-only executable;
//    2) conditionally compile into the host-native test suite if the Binder
//       module is built;
//    3) provide a test-only static library that re-defines the libbinder
//       classes as mocks.
// (See http://b/23386387)
//
// TODO(armansito): We should make this class non-final and the template
// interface abstract, so that code that depends on this can be unit tested
// against a mock version of this class.
//
// TODO(armansito): Consider submitting this class to frameworks/native/binder.
template<typename T>
class RemoteCallbackList final {
 public:
  RemoteCallbackList() = default;
  ~RemoteCallbackList();

  // Register and unregister a callback interface. Registering will
  // automatically start tracking for death notifications in case the remote
  // process hosting the Binder dies. In such a case, the Binder is
  // automatically removed from the list.
  bool Register(const android::sp<T>& callback);
  bool Unregister(const android::sp<T>& callback);

  // Calls the given function on each of the contained callbacks. The internal
  // mutex is locked for the duration of the iteration.
  void ForEach(const std::function<void(T*)>& callback);

 private:
  class CallbackDeathRecipient : public android::IBinder::DeathRecipient {
   public:
    CallbackDeathRecipient(const android::sp<T>& callback,
                           RemoteCallbackList* owner);

    android::sp<T> get_callback() const { return callback_; }

    // android::IBinder::DeathRecipient override:
    void binderDied(const android::wp<android::IBinder>& who) override;

   private:
    android::sp<T> callback_;
    RemoteCallbackList<T>* owner_;  // weak
  };

  // Typedef for internal map container. This keeps track of a given Binder and
  // a death receiver associated with it.
  using CallbackMap = std::unordered_map<android::IBinder*,
                                         android::sp<CallbackDeathRecipient>>;

  bool UnregisterInternal(typename CallbackMap::iterator iter);

  std::mutex map_lock_;
  CallbackMap callbacks_;

  DISALLOW_COPY_AND_ASSIGN(RemoteCallbackList);
};

// Template Implementation details below
// ========================================================

using android::IBinder;
using android::IInterface;
using android::sp;
using android::wp;

template<typename T>
RemoteCallbackList<T>::~RemoteCallbackList() {
  std::lock_guard<std::mutex> lock(map_lock_);
  for (auto iter = callbacks_.begin(); iter != callbacks_.end(); ++iter)
    UnregisterInternal(iter);
}

template<typename T>
bool RemoteCallbackList<T>::Register(const sp<T>& callback) {
  std::lock_guard<std::mutex> lock(map_lock_);

  sp<IBinder> binder = IInterface::asBinder(callback.get());
  if (callbacks_.find(binder.get()) != callbacks_.end()) {
    VLOG(1) << "Callback list already contains given callback";
    return false;
  }

  sp<CallbackDeathRecipient> dr(new CallbackDeathRecipient(callback, this));

  if (binder->linkToDeath(dr) != android::NO_ERROR) {
    LOG(ERROR) << "Failed to link death recipient to binder";
    return false;
  }

  callbacks_[binder.get()] = dr;

  VLOG(2) << "Callback successfully registered with list";

  return true;
}

template<typename T>
bool RemoteCallbackList<T>::Unregister(const sp<T>& callback) {
  std::lock_guard<std::mutex> lock(map_lock_);

  sp<IBinder> binder = IInterface::asBinder(callback.get());
  auto iter = callbacks_.find(binder.get());
  if (iter == callbacks_.end()) {
    VLOG(2) << "Given callback not registered with this list";
    return false;
  }

  return UnregisterInternal(iter);
}

template<typename T>
void RemoteCallbackList<T>::ForEach(const std::function<void(T*)>& callback) {
  std::lock_guard<std::mutex> lock(map_lock_);
  for (const auto& iter : callbacks_)
    callback(iter.second->get_callback().get());
}

template<typename T>
bool RemoteCallbackList<T>::UnregisterInternal(
    typename CallbackMap::iterator iter) {
  sp<CallbackDeathRecipient> dr = iter->second;
  callbacks_.erase(iter);

  if (IInterface::asBinder(dr->get_callback().get())->unlinkToDeath(dr) !=
      android::NO_ERROR) {
    LOG(ERROR) << "Failed to unlink death recipient from binder";

    // We removed the entry from |map_| but unlinkToDeath failed. There isn't
    // really much we can do here other than deleting the binder and returning
    // an error.
    return false;
  }

  VLOG(2) << "Callback successfully removed from list";

  return true;
}

template<typename T>
RemoteCallbackList<T>::CallbackDeathRecipient::CallbackDeathRecipient(
    const sp<T>& callback,
    RemoteCallbackList<T>* owner)
    : callback_(callback),
      owner_(owner) {
  CHECK(callback_.get());
  CHECK(owner_);
}

template<typename T>
void RemoteCallbackList<T>::CallbackDeathRecipient::binderDied(
    const wp<IBinder>& who) {
  VLOG(1) << "Received binderDied";

  sp<IBinder> binder = IInterface::asBinder(callback_.get());
  CHECK(who.unsafe_get() == binder.get());

  // Remove the callback but no need to call unlinkToDeath.
  std::lock_guard<std::mutex> lock(owner_->map_lock_);
  auto iter = owner_->callbacks_.find(binder.get());
  CHECK(iter != owner_->callbacks_.end());
  owner_->callbacks_.erase(iter);

  VLOG(1) << "Callback from dead process unregistered";
}

}  // namespace binder
}  // namespace ipc
