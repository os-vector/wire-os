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

#include <mutex>
#include <unordered_map>

#include <base/logging.h>
#include <base/macros.h>
#include <binder/IBinder.h>
#include <binder/IInterface.h>

namespace ipc {
namespace binder {

// A map of remote interfaces where the value type "V" must inherit from
// android::IInterface. This is just like RemoteCallbackList except it provides
// a key-value mapping.
//
// TODO(armansito): We should make this class non-final and the template
// interface abstract, so that code that depends on this can be unit tested
// against a mock version of this class.
template<typename K, typename V>
class RemoteCallbackMap final {
 public:
  RemoteCallbackMap() = default;
  ~RemoteCallbackMap();

  // The Delegate interface is used to notify when a registered callback is
  // removed from the map as a result of the death of the remote process that
  // owns the registered callback.
  class Delegate {
   public:
    virtual ~Delegate() = default;

    // Called when a remote callback associated with the key |key| has been
    // removed. This won't get called if the callback was removed as a result of
    // a call to RemoteCallbackMap::Unregister.
    virtual void OnRemoteCallbackRemoved(const K& key) = 0;
  };

  // Register a callback interface and associate it with the given key.
  // Registering will automatically start tracking for death notifications in
  // case the remote process hosting the Binder dies. In such a case, the Binder
  // is automatically removed from the map.
  //
  // An optional |delegate| can be passed which will be assocated with the given
  // key/value pair. |delegate| must outlive this map.
  bool Register(const K& key,
                const android::sp<V>& callback,
                Delegate* delegate = nullptr);

  // Unregisters a previously registered callback with the given key. Returns
  // false if |key| doesn't exist.
  bool Unregister(const K& key);

  // Returns the callback associated with the given key. Returns NULL if |key|
  // doesn't exist.
  android::sp<V> Get(const K& key);

  // Removes the callback associated with the given key from the map and returns
  // the value. Returns NULL if the key doesn't exists.
  android::sp<V> Remove(const K& key);

  // Clear the contents of the map.
  void Clear();

 private:
  class CallbackDeathRecipient : public android::IBinder::DeathRecipient {
   public:
    CallbackDeathRecipient(
        const K& key,
        const android::sp<V>& callback,
        RemoteCallbackMap<K, V>* owner,
        Delegate* delegate);

    android::sp<V> get_callback() const { return callback_; }

    // android::IBinder::DeathRecipient override:
    void binderDied(const android::wp<android::IBinder>& who) override;

   private:
    K key_;
    android::sp<V> callback_;
    RemoteCallbackMap<K, V>* owner_;  // weak
    Delegate* delegate_;  // weak
  };

  // Typedef for internal map container.
  using CallbackMap =
      std::unordered_map<K, android::sp<CallbackDeathRecipient>>;

  bool UnregisterInternal(typename CallbackMap::iterator iter);

  std::mutex map_lock_;
  CallbackMap map_;

  DISALLOW_COPY_AND_ASSIGN(RemoteCallbackMap);
};

// Template Implementation details below
// ========================================================

using android::IBinder;
using android::IInterface;
using android::sp;
using android::wp;
using std::lock_guard;
using std::mutex;

template<typename K, typename V>
RemoteCallbackMap<K, V>::~RemoteCallbackMap() {
  Clear();
}

template<typename K, typename V>
bool RemoteCallbackMap<K, V>::Register(
    const K& key,
    const sp<V>& callback,
    Delegate* delegate) {
  lock_guard<mutex> lock(map_lock_);

  if (map_.find(key) != map_.end()) {
    VLOG(1) << "Callback map already contains key";
    return false;
  }

  sp<CallbackDeathRecipient> dr(
      new CallbackDeathRecipient(key, callback, this, delegate));
  sp<IBinder> binder = IInterface::asBinder(callback.get());
  if (binder->linkToDeath(dr) != android::NO_ERROR) {
    LOG(ERROR) << "Failed to link death recipient to binder";
    return false;
  }

  map_[key] = dr;

  VLOG(2) << "Callback successfully registered with map";

  return true;
}

template<typename K, typename V>
bool RemoteCallbackMap<K, V>::Unregister(const K& key) {
  lock_guard<mutex> lock(map_lock_);

  auto iter = map_.find(key);
  if (iter == map_.end()) {
    VLOG(1) << "Callback with given key not found";
    return false;
  }

  return UnregisterInternal(iter);
}

template<typename K, typename V>
sp<V> RemoteCallbackMap<K, V>::Get(const K& key) {
  lock_guard<mutex> lock(map_lock_);

  auto iter = map_.find(key);
  if (iter == map_.end())
    return nullptr;

  return iter->second->get_callback();
}

template<typename K, typename V>
sp<V> RemoteCallbackMap<K, V>::Remove(const K& key) {
  lock_guard<mutex> lock(map_lock_);

  auto iter = map_.find(key);
  if (iter == map_.end())
    return nullptr;

  sp<V> val = iter->second->get_callback();
  UnregisterInternal(iter);

  return val;
}
template<typename K, typename V>
void RemoteCallbackMap<K, V>::Clear() {
  lock_guard<mutex> lock(map_lock_);

  for (auto iter = map_.begin(); iter != map_.end();)
    UnregisterInternal(iter++);
}

template<typename K, typename V>
bool RemoteCallbackMap<K, V>::UnregisterInternal(
    typename CallbackMap::iterator iter) {
  sp<CallbackDeathRecipient> dr = iter->second;
  map_.erase(iter);

  if (IInterface::asBinder(dr->get_callback().get())->unlinkToDeath(dr) !=
      android::NO_ERROR) {
    LOG(ERROR) << "Failed to unlink death recipient from binder";

    // We removed the entry from |map_| but unlinkToDeath failed. There isn't
    // really much we can do here other than deleting the binder and returning
    // an error.
    return false;
  }

  VLOG(2) << "Callback successfully removed from map";

  return true;
}

template<typename K, typename V>
RemoteCallbackMap<K, V>::CallbackDeathRecipient::CallbackDeathRecipient(
    const K& key,
    const sp<V>& callback,
    RemoteCallbackMap<K, V>* owner,
    Delegate* delegate)
    : key_(key),
      callback_(callback),
      owner_(owner),
      delegate_(delegate) {
  CHECK(callback_.get());
}

template<typename K, typename V>
void RemoteCallbackMap<K, V>::CallbackDeathRecipient::binderDied(
    const wp<IBinder>& who) {
  VLOG(1) << "Received binderDied";

  sp<IBinder> binder = IInterface::asBinder(callback_.get());
  CHECK(who.unsafe_get() == binder.get());

  // Remove the callback but no need to call unlinkToDeath.
  {
    lock_guard<mutex> lock(owner_->map_lock_);
    auto iter = owner_->map_.find(key_);
    CHECK(iter != owner_->map_.end());
    owner_->map_.erase(iter);
  }

  VLOG(1) << "Callback from dead process unregistered; notifying delegate";

  // Notify delegate.
  if (delegate_)
    delegate_->OnRemoteCallbackRemoved(key_);
}

}  // namespace binder
}  // namespace ipc
