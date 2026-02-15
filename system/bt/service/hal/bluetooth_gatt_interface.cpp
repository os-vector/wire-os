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

#include "service/hal/bluetooth_gatt_interface.h"

#include <mutex>
#define _LIBCPP_BUILDING_SHARED_MUTEX
#include <shared_mutex>
#undef _LIBCPP_BUILDING_SHARED_MUTEX

#include <base/logging.h>
#include <base/observer_list.h>

#include "service/hal/bluetooth_interface.h"
#include "service/logging_helpers.h"

using std::lock_guard;
using std::unique_lock;
using std::shared_lock;
using std::mutex;
using std::shared_timed_mutex;

namespace bluetooth {
namespace hal {

namespace {

// The global BluetoothGattInterface instance.
BluetoothGattInterface* g_interface = nullptr;

// Mutex used by callbacks to access |g_interface|. If we initialize or clean it
// use unique_lock. If only accessing |g_interface| use shared lock.
//TODO(jpawlowski): this should be just shared_mutex, as we currently don't use
// timed methods. Change to shared_mutex when we upgrade to C++14
shared_timed_mutex g_instance_lock;

// Helper for obtaining the observer lists. This is forward declared here
// and defined below since it depends on BluetoothInterfaceImpl.
base::ObserverList<BluetoothGattInterface::ClientObserver>*
    GetClientObservers();
base::ObserverList<BluetoothGattInterface::ServerObserver>*
    GetServerObservers();

#define FOR_EACH_CLIENT_OBSERVER(func) \
  FOR_EACH_OBSERVER(BluetoothGattInterface::ClientObserver, \
                    *GetClientObservers(), func)

#define FOR_EACH_SERVER_OBSERVER(func) \
  FOR_EACH_OBSERVER(BluetoothGattInterface::ServerObserver, \
                    *GetServerObservers(), func)

#define VERIFY_INTERFACE_OR_RETURN() \
  do { \
    if (!g_interface) { \
      LOG(WARNING) << "Callback received while |g_interface| is NULL"; \
      return; \
    } \
  } while (0)

void RegisterClientCallback(int status, int client_if, bt_uuid_t* app_uuid) {
  shared_lock<shared_timed_mutex> lock(g_instance_lock);
  VLOG(2) << __func__ << " - status: " << status << " client_if: " << client_if;
  VERIFY_INTERFACE_OR_RETURN();
  CHECK(app_uuid);

  FOR_EACH_CLIENT_OBSERVER(
      RegisterClientCallback(g_interface, status, client_if, *app_uuid));
}

void ScanResultCallback(bt_bdaddr_t* bda, int rssi, uint8_t* adv_data) {
  shared_lock<shared_timed_mutex> lock(g_instance_lock);
  VERIFY_INTERFACE_OR_RETURN();
  CHECK(bda);
  CHECK(adv_data);

  VLOG(2) << __func__ << " - BD_ADDR: " << BtAddrString(bda)
          << " RSSI: " << rssi;
  FOR_EACH_CLIENT_OBSERVER(
    ScanResultCallback(g_interface, *bda, rssi, adv_data));
}

void ConnectCallback(int conn_id, int status, int client_if, bt_bdaddr_t* bda) {
  shared_lock<shared_timed_mutex> lock(g_instance_lock);
  VERIFY_INTERFACE_OR_RETURN();
  CHECK(bda);

  VLOG(2) << __func__ << " - status: " << status
          << " client_if: " << client_if
          << " - BD_ADDR: " << BtAddrString(bda)
          << " - conn_id: " << conn_id;

  FOR_EACH_CLIENT_OBSERVER(
    ConnectCallback(g_interface, conn_id, status, client_if, *bda));
}

void DisconnectCallback(int conn_id, int status, int client_if,
                        bt_bdaddr_t* bda) {
  shared_lock<shared_timed_mutex> lock(g_instance_lock);
  VERIFY_INTERFACE_OR_RETURN();
  CHECK(bda);

  VLOG(2) << __func__ << " - conn_id: " << conn_id
             << " - status: " << status
             << " client_if: " << client_if
             << " - BD_ADDR: " << BtAddrString(bda);
  FOR_EACH_CLIENT_OBSERVER(
    DisconnectCallback(g_interface, conn_id, status, client_if, *bda));
}

void SearchCompleteCallback(int conn_id, int status) {
  shared_lock<shared_timed_mutex> lock(g_instance_lock);
  VERIFY_INTERFACE_OR_RETURN();

  VLOG(2) << __func__ << " - conn_id: " << conn_id
          << " - status: " << status;
  FOR_EACH_CLIENT_OBSERVER(
    SearchCompleteCallback(g_interface, conn_id, status));
}

void RegisterForNotificationCallback(int conn_id, int registered, int status, uint16_t handle) {
  shared_lock<shared_timed_mutex> lock(g_instance_lock);
  VERIFY_INTERFACE_OR_RETURN();

  VLOG(2) << __func__ << " - conn_id: " << conn_id
          << " - status: " << status
          << " - registered: " << registered
          << " - handle: " << handle;
  FOR_EACH_CLIENT_OBSERVER(
    RegisterForNotificationCallback(g_interface, conn_id, status, registered, handle));
}

void NotifyCallback(int conn_id, btgatt_notify_params_t *p_data) {
  shared_lock<shared_timed_mutex> lock(g_instance_lock);
  VERIFY_INTERFACE_OR_RETURN();

  VLOG(2) << __func__ << " - conn_id: " << conn_id
          << " - address: " << BtAddrString(&p_data->bda)
          << " - handle: " << p_data->handle
          << " - len: " << p_data->len
          << " - is_notify: " << (int) p_data->is_notify;

  FOR_EACH_CLIENT_OBSERVER(
    NotifyCallback(g_interface, conn_id, p_data));
}

void WriteCharacteristicCallback(int conn_id, int status, uint16_t handle) {
  shared_lock<shared_timed_mutex> lock(g_instance_lock);
  VERIFY_INTERFACE_OR_RETURN();

  VLOG(2) << __func__ << " - conn_id: " << conn_id
          << " - status: " << status;

  FOR_EACH_CLIENT_OBSERVER(
    WriteCharacteristicCallback(g_interface, conn_id, status, handle));
}

void WriteDescriptorCallback(int conn_id, int status,
      uint16_t handle) {
  shared_lock<shared_timed_mutex> lock(g_instance_lock);
  VERIFY_INTERFACE_OR_RETURN();

  VLOG(2) << __func__ << " - conn_id: " << conn_id
          << " - status: " << status;

  FOR_EACH_CLIENT_OBSERVER(
    WriteDescriptorCallback(g_interface, conn_id, status, handle));
}

void ListenCallback(int status, int client_if) {
  shared_lock<shared_timed_mutex> lock(g_instance_lock);
  VLOG(2) << __func__ << " - status: " << status << " client_if: " << client_if;
  VERIFY_INTERFACE_OR_RETURN();

  FOR_EACH_CLIENT_OBSERVER(ListenCallback(g_interface, status, client_if));
}

void MtuChangedCallback(int conn_id, int status, int mtu) {
  shared_lock<shared_timed_mutex> lock(g_instance_lock);
  VERIFY_INTERFACE_OR_RETURN();

  VLOG(2) << __func__ << " - conn_id: " << conn_id
          << " status: " << status
          << " mtu: " << mtu;

  FOR_EACH_CLIENT_OBSERVER(MtuChangedCallback(g_interface, conn_id, status, mtu));
}

void MultiAdvEnableCallback(int client_if, int status) {
  shared_lock<shared_timed_mutex> lock(g_instance_lock);
  VLOG(2) << __func__ << " - status: " << status << " client_if: " << client_if;
  VERIFY_INTERFACE_OR_RETURN();

  FOR_EACH_CLIENT_OBSERVER(
      MultiAdvEnableCallback(g_interface, client_if, status));
}

void MultiAdvUpdateCallback(int client_if, int status) {
  shared_lock<shared_timed_mutex> lock(g_instance_lock);
  VLOG(2) << __func__ << " - status: " << status << " client_if: " << client_if;
  VERIFY_INTERFACE_OR_RETURN();

  FOR_EACH_CLIENT_OBSERVER(
      MultiAdvUpdateCallback(g_interface, client_if, status));
}

void MultiAdvDataCallback(int client_if, int status) {
  shared_lock<shared_timed_mutex> lock(g_instance_lock);
  VLOG(2) << __func__ << " - status: " << status << " client_if: " << client_if;
  VERIFY_INTERFACE_OR_RETURN();

  FOR_EACH_CLIENT_OBSERVER(
      MultiAdvDataCallback(g_interface, client_if, status));
}

void MultiAdvDisableCallback(int client_if, int status) {
  shared_lock<shared_timed_mutex> lock(g_instance_lock);
  VLOG(2) << __func__ << " - status: " << status << " client_if: " << client_if;
  VERIFY_INTERFACE_OR_RETURN();

  FOR_EACH_CLIENT_OBSERVER(
      MultiAdvDisableCallback(g_interface, client_if, status));
}

void GetGattDbCallback(int conn_id, btgatt_db_element_t *db, int size) {
  shared_lock<shared_timed_mutex> lock(g_instance_lock);
  VLOG(2) << __func__ << " - conn_id: " << conn_id << " size: " << size;
  VERIFY_INTERFACE_OR_RETURN();

  FOR_EACH_CLIENT_OBSERVER(
      GetGattDbCallback(g_interface, conn_id, db, size));
}

void ReadCharacteristicCallback(int conn_id, int status, btgatt_read_params_t *data) {
  shared_lock<shared_timed_mutex> lock(g_instance_lock);
  VLOG(2) << __func__ << " - conn_id: " << conn_id << " status: " << status;
  VERIFY_INTERFACE_OR_RETURN();

  FOR_EACH_CLIENT_OBSERVER(
      ReadCharacteristicCallback(g_interface, conn_id, status, data));
}

void ServicesRemovedCallback(int conn_id, uint16_t start_handle, uint16_t end_handle) {
  shared_lock<shared_timed_mutex> lock(g_instance_lock);
  VLOG(2) << __func__ << " - conn_id: " << conn_id
          << " start_handle: " << start_handle
          << " end_handle: " << end_handle;
  VERIFY_INTERFACE_OR_RETURN();

  FOR_EACH_CLIENT_OBSERVER(
      ServicesRemovedCallback(g_interface, conn_id, start_handle, end_handle));
}

void ServicesAddedCallback(int conn_id, btgatt_db_element_t *added, int added_count) {
  shared_lock<shared_timed_mutex> lock(g_instance_lock);
  VLOG(2) << __func__ << " - conn_id: " << conn_id
          << " added_count: " << added_count;
  VERIFY_INTERFACE_OR_RETURN();

  FOR_EACH_CLIENT_OBSERVER(
      ServicesAddedCallback(g_interface, conn_id, added, added_count));
}

void RegisterServerCallback(int status, int server_if, bt_uuid_t* app_uuid) {
  shared_lock<shared_timed_mutex> lock(g_instance_lock);
  VLOG(2) << __func__ << " - status: " << status << " server_if: " << server_if;
  VERIFY_INTERFACE_OR_RETURN();
  CHECK(app_uuid);

  FOR_EACH_SERVER_OBSERVER(
      RegisterServerCallback(g_interface, status, server_if, *app_uuid));
}

void ConnectionCallback(int conn_id, int server_if, int connected,
                        bt_bdaddr_t* bda) {
  shared_lock<shared_timed_mutex> lock(g_instance_lock);
  VLOG(2) << __func__ << " - conn_id: " << conn_id
          << " server_if: " << server_if << " connected: " << connected;
  VERIFY_INTERFACE_OR_RETURN();
  CHECK(bda);

  FOR_EACH_SERVER_OBSERVER(
      ConnectionCallback(g_interface, conn_id, server_if, connected, *bda));
}

void ServiceAddedCallback(
    int status,
    int server_if,
    btgatt_srvc_id_t* srvc_id,
    int srvc_handle) {
  shared_lock<shared_timed_mutex> lock(g_instance_lock);
  VLOG(2) << __func__ << " - status: " << status << " server_if: " << server_if
          << " handle: " << srvc_handle;
  VERIFY_INTERFACE_OR_RETURN();
  CHECK(srvc_id);

  FOR_EACH_SERVER_OBSERVER(ServiceAddedCallback(
      g_interface, status, server_if, *srvc_id, srvc_handle));
}

void CharacteristicAddedCallback(
    int status, int server_if,
    bt_uuid_t* uuid,
    int srvc_handle,
    int char_handle) {
  shared_lock<shared_timed_mutex> lock(g_instance_lock);
  VLOG(2) << __func__ << " - status: " << status << " server_if: " << server_if
          << " srvc_handle: " << srvc_handle << " char_handle: " << char_handle;
  VERIFY_INTERFACE_OR_RETURN();
  CHECK(uuid);

  FOR_EACH_SERVER_OBSERVER(CharacteristicAddedCallback(
      g_interface, status, server_if, *uuid, srvc_handle, char_handle));
}

void DescriptorAddedCallback(
    int status, int server_if,
    bt_uuid_t* uuid,
    int srvc_handle,
    int desc_handle) {
  shared_lock<shared_timed_mutex> lock(g_instance_lock);
  VLOG(2) << __func__ << " - status: " << status << " server_if: " << server_if
          << " srvc_handle: " << srvc_handle << " desc_handle: " << desc_handle;
  VERIFY_INTERFACE_OR_RETURN();
  CHECK(uuid);

  FOR_EACH_SERVER_OBSERVER(DescriptorAddedCallback(
      g_interface, status, server_if, *uuid, srvc_handle, desc_handle));
}

void ServiceStartedCallback(int status, int server_if, int srvc_handle) {
  shared_lock<shared_timed_mutex> lock(g_instance_lock);
  VLOG(2) << __func__ << " - status: " << status << " server_if: " << server_if
          << " handle: " << srvc_handle;
  VERIFY_INTERFACE_OR_RETURN();

  FOR_EACH_SERVER_OBSERVER(ServiceStartedCallback(
      g_interface, status, server_if, srvc_handle));
}

void ServiceStoppedCallback(int status, int server_if, int srvc_handle) {
  shared_lock<shared_timed_mutex> lock(g_instance_lock);
  VLOG(2) << __func__ << " - status: " << status << " server_if: " << server_if
          << " handle: " << srvc_handle;
  VERIFY_INTERFACE_OR_RETURN();

  FOR_EACH_SERVER_OBSERVER(ServiceStoppedCallback(
      g_interface, status, server_if, srvc_handle));
}

void ServiceDeletedCallback(int status, int server_if, int srvc_handle) {
  shared_lock<shared_timed_mutex> lock(g_instance_lock);
  VLOG(2) << __func__ << " - status: " << status << " server_if: " << server_if
          << " handle: " << srvc_handle;
  VERIFY_INTERFACE_OR_RETURN();

  FOR_EACH_SERVER_OBSERVER(ServiceDeletedCallback(
      g_interface, status, server_if, srvc_handle));
}

void RequestReadCallback(int conn_id, int trans_id, bt_bdaddr_t* bda,
                         int attr_handle, int offset, bool is_long) {
  shared_lock<shared_timed_mutex> lock(g_instance_lock);
  VLOG(2) << __func__ << " - conn_id: " << conn_id << " trans_id: " << trans_id
          << " attr_handle: " << attr_handle << " offset: " << offset
          << " is_long: " << is_long;
  VERIFY_INTERFACE_OR_RETURN();
  CHECK(bda);

  FOR_EACH_SERVER_OBSERVER(RequestReadCallback(
      g_interface, conn_id, trans_id, *bda, attr_handle, offset, is_long));
}

void RequestWriteCallback(int conn_id, int trans_id,
                          bt_bdaddr_t* bda,
                          int attr_handle, int offset, int length,
                          bool need_rsp, bool is_prep, uint8_t* value) {
  shared_lock<shared_timed_mutex> lock(g_instance_lock);
  VLOG(2) << __func__ << " - conn_id: " << conn_id << " trans_id: " << trans_id
          << " attr_handle: " << attr_handle << " offset: " << offset
          << " length: " << length << " need_rsp: " << need_rsp
          << " is_prep: " << is_prep;
  VERIFY_INTERFACE_OR_RETURN();
  CHECK(bda);

  FOR_EACH_SERVER_OBSERVER(RequestWriteCallback(
      g_interface, conn_id, trans_id, *bda, attr_handle, offset, length,
      need_rsp, is_prep, value));
}

void RequestExecWriteCallback(int conn_id, int trans_id,
                              bt_bdaddr_t* bda, int exec_write) {
  shared_lock<shared_timed_mutex> lock(g_instance_lock);
  VLOG(2) << __func__ << " - conn_id: " << conn_id << " trans_id: " << trans_id
          << " exec_write: " << exec_write;
  VERIFY_INTERFACE_OR_RETURN();
  CHECK(bda);

  FOR_EACH_SERVER_OBSERVER(RequestExecWriteCallback(
      g_interface, conn_id, trans_id, *bda, exec_write));
}

void ResponseConfirmationCallback(int status, int handle) {
  shared_lock<shared_timed_mutex> lock(g_instance_lock);
  VLOG(2) << __func__ << " - status: " << status << " handle: " << handle;
  VERIFY_INTERFACE_OR_RETURN();

  FOR_EACH_SERVER_OBSERVER(ResponseConfirmationCallback(
      g_interface, status, handle));
}

void IndicationSentCallback(int conn_id, int status) {
  shared_lock<shared_timed_mutex> lock(g_instance_lock);
  VLOG(2) << __func__ << " - conn_id: " << conn_id << " status: " << status;
  VERIFY_INTERFACE_OR_RETURN();

  FOR_EACH_SERVER_OBSERVER(IndicationSentCallback(
      g_interface, conn_id, status));
}

void MtuChangedCallback(int conn_id, int mtu) {
  shared_lock<shared_timed_mutex> lock(g_instance_lock);
  VLOG(2) << __func__ << " - conn_id: " << conn_id << " mtu: " << mtu;
  VERIFY_INTERFACE_OR_RETURN();

  FOR_EACH_SERVER_OBSERVER(MtuChangedCallback(g_interface, conn_id, mtu));
}

// The HAL Bluetooth GATT client interface callbacks. These signal a mixture of
// GATT client-role and GAP events.
const btgatt_client_callbacks_t gatt_client_callbacks = {
    RegisterClientCallback,
    ScanResultCallback,
    ConnectCallback,
    DisconnectCallback,
    SearchCompleteCallback,
    RegisterForNotificationCallback,
    NotifyCallback,
    ReadCharacteristicCallback,  // read_characteristic_cb
    WriteCharacteristicCallback,
    nullptr,  // read_descriptor_cb
    WriteDescriptorCallback,
    nullptr,  // execute_write_cb
    nullptr,  // read_remote_rssi_cb
    ListenCallback,
    MtuChangedCallback,
    nullptr,  // scan_filter_cfg_cb
    nullptr,  // scan_filter_param_cb
    nullptr,  // scan_filter_status_cb
    MultiAdvEnableCallback,
    MultiAdvUpdateCallback,
    MultiAdvDataCallback,
    MultiAdvDisableCallback,
    nullptr,  // congestion_cb
    nullptr,  // batchscan_cfg_storage_cb
    nullptr,  // batchscan_enb_disable_cb
    nullptr,  // batchscan_reports_cb
    nullptr,  // batchscan_threshold_cb
    nullptr,  // track_adv_event_cb
    nullptr,  // scan_parameter_setup_completed_cb
    GetGattDbCallback,
    ServicesRemovedCallback,
    ServicesAddedCallback,
};

const btgatt_server_callbacks_t gatt_server_callbacks = {
    RegisterServerCallback,
    ConnectionCallback,
    ServiceAddedCallback,
    nullptr,  // included_service_added_cb
    CharacteristicAddedCallback,
    DescriptorAddedCallback,
    ServiceStartedCallback,
    ServiceStoppedCallback,
    ServiceDeletedCallback,
    RequestReadCallback,
    RequestWriteCallback,
    RequestExecWriteCallback,
    ResponseConfirmationCallback,
    IndicationSentCallback,
    nullptr,  // congestion_cb
    MtuChangedCallback,
};

const btgatt_callbacks_t gatt_callbacks = {
  sizeof(btgatt_callbacks_t),
  &gatt_client_callbacks,
  &gatt_server_callbacks
};

}  // namespace

// BluetoothGattInterface implementation for production.
class BluetoothGattInterfaceImpl : public BluetoothGattInterface {
 public:
  BluetoothGattInterfaceImpl() : hal_iface_(nullptr) {
  }

  ~BluetoothGattInterfaceImpl() override {
    if (hal_iface_)
        hal_iface_->cleanup();
  }

  // BluetoothGattInterface overrides:
  void AddClientObserver(ClientObserver* observer) override {
    client_observers_.AddObserver(observer);
  }

  void RemoveClientObserver(ClientObserver* observer) override {
    client_observers_.RemoveObserver(observer);
  }

  void AddServerObserver(ServerObserver* observer) override {
    server_observers_.AddObserver(observer);
  }

  void RemoveServerObserver(ServerObserver* observer) override {
    server_observers_.RemoveObserver(observer);
  }

  const btgatt_client_interface_t* GetClientHALInterface() const override {
    return hal_iface_->client;
  }

  const btgatt_server_interface_t* GetServerHALInterface() const override {
    return hal_iface_->server;
  }

  // Initialize the interface.
  bool Initialize() {
    const bt_interface_t* bt_iface =
        BluetoothInterface::Get()->GetHALInterface();
    CHECK(bt_iface);

    const btgatt_interface_t* gatt_iface =
        reinterpret_cast<const btgatt_interface_t*>(
            bt_iface->get_profile_interface(BT_PROFILE_GATT_ID));
    if (!gatt_iface) {
      LOG(ERROR) << "Failed to obtain HAL GATT interface handle";
      return false;
    }

    bt_status_t status = gatt_iface->init(&gatt_callbacks);
    if (status != BT_STATUS_SUCCESS) {
      LOG(ERROR) << "Failed to initialize HAL GATT interface";
      return false;
    }

    hal_iface_ = gatt_iface;

    return true;
  }

  base::ObserverList<ClientObserver>* client_observers() {
    return &client_observers_;
  }

  base::ObserverList<ServerObserver>* server_observers() {
    return &server_observers_;
  }

 private:
  // List of observers that are interested in notifications from us.
  // We're not using a base::ObserverListThreadSafe, which it posts observer
  // events automatically on the origin threads, as we want to avoid that
  // overhead and simply forward the events to the upper layer.
  base::ObserverList<ClientObserver> client_observers_;
  base::ObserverList<ServerObserver> server_observers_;

  // The HAL handle obtained from the shared library. We hold a weak reference
  // to this since the actual data resides in the shared Bluetooth library.
  const btgatt_interface_t* hal_iface_;

  DISALLOW_COPY_AND_ASSIGN(BluetoothGattInterfaceImpl);
};

namespace {

base::ObserverList<BluetoothGattInterface::ClientObserver>*
GetClientObservers() {
  CHECK(g_interface);
  return static_cast<BluetoothGattInterfaceImpl*>(
      g_interface)->client_observers();
}

base::ObserverList<BluetoothGattInterface::ServerObserver>*
GetServerObservers() {
  CHECK(g_interface);
  return static_cast<BluetoothGattInterfaceImpl*>(
      g_interface)->server_observers();
}

}  // namespace

// Default observer implementations. These are provided so that the methods
// themselves are optional.
void BluetoothGattInterface::ClientObserver::RegisterClientCallback(
    BluetoothGattInterface* /* gatt_iface */,
    int /* status */,
    int /* client_if */,
    const bt_uuid_t& /* app_uuid */) {
  // Do nothing.
}
void BluetoothGattInterface::ClientObserver::ScanResultCallback(
    BluetoothGattInterface* /* gatt_iface */,
    const bt_bdaddr_t& /* bda */,
    int /* rssi */,
    uint8_t* /* adv_data */) {
  // Do Nothing.
}

void BluetoothGattInterface::ClientObserver::ConnectCallback(
    BluetoothGattInterface* /* gatt_iface */,
    int /* conn_id */,
    int /* status */,
    int /* client_if */,
    const bt_bdaddr_t& /* bda */) {
  // Do nothing
}

void BluetoothGattInterface::ClientObserver::DisconnectCallback(
    BluetoothGattInterface* /* gatt_iface */,
    int /* conn_id */,
    int /* status */,
    int /* client_if */,
    const bt_bdaddr_t& /* bda */) {
  // Do nothing
}

void BluetoothGattInterface::ClientObserver::SearchCompleteCallback(
    BluetoothGattInterface* /* gatt_iface */,
    int /* conn_id */,
    int /* status */) {
  // Do nothing
}

void BluetoothGattInterface::ClientObserver::RegisterForNotificationCallback(
    BluetoothGattInterface* /* gatt_iface */,
    int /* conn_id */,
    int /* status */,
    int /* registered */,
    uint16_t /* handle */) {
  // Do nothing
}

void BluetoothGattInterface::ClientObserver::NotifyCallback(
    BluetoothGattInterface* /* gatt_iface */,
    int /* conn_id */,
    btgatt_notify_params_t* /* p_data */) {
  // Do nothing
}

void BluetoothGattInterface::ClientObserver::WriteCharacteristicCallback(
    BluetoothGattInterface* /* gatt_iface */,
    int /* conn_id */,
    int /* status */,
    uint16_t /* handle */) {
  // Do nothing
}

void BluetoothGattInterface::ClientObserver::WriteDescriptorCallback(
    BluetoothGattInterface* /* gatt_iface */,
    int /* conn_id */,
    int /* status */,
    uint16_t /* handle */) {
  // Do nothing
}

void BluetoothGattInterface::ClientObserver::ListenCallback(
    BluetoothGattInterface* /* gatt_iface */,
    int /* status */,
    int /* client_if */) {
  // Do nothing.
}

void BluetoothGattInterface::ClientObserver::MtuChangedCallback(
    BluetoothGattInterface* /* gatt_iface */,
    int /* conn_id */,
    int /* statis*/,
    int /* mtu */) {
  // Do nothing.
}

void BluetoothGattInterface::ClientObserver::MultiAdvEnableCallback(
    BluetoothGattInterface* /* gatt_iface */,
    int /* status */,
    int /* client_if */) {
  // Do nothing.
}
void BluetoothGattInterface::ClientObserver::MultiAdvUpdateCallback(
    BluetoothGattInterface* /* gatt_iface */,
    int /* status */,
    int /* client_if */) {
  // Do nothing.
}
void BluetoothGattInterface::ClientObserver::MultiAdvDataCallback(
    BluetoothGattInterface* /* gatt_iface */,
    int /* status */,
    int /* client_if */) {
  // Do nothing.
}
void BluetoothGattInterface::ClientObserver::MultiAdvDisableCallback(
    BluetoothGattInterface* /* gatt_iface */,
    int /* status */,
    int /* client_if */) {
  // Do nothing.
}

void BluetoothGattInterface::ClientObserver::GetGattDbCallback(
    BluetoothGattInterface* /* gatt_iface */,
    int /* conn_id */,
    btgatt_db_element_t* /* gatt_db */,
    int /* size */) {
  // Do nothing.
}

void BluetoothGattInterface::ClientObserver::ReadCharacteristicCallback(
    BluetoothGattInterface* /* gatt_iface */,
    int /* conn_id */,
    int /* status */,
    btgatt_read_params_t* /* data */) {
  // Do nothing.
}

void BluetoothGattInterface::ClientObserver::ServicesRemovedCallback(
    BluetoothGattInterface* /* gatt_iface */,
    int /* conn_id */,
    uint16_t /* start_handle */,
    uint16_t /* end_handle */) {
  // Do nothing.
}

void BluetoothGattInterface::ClientObserver::ServicesAddedCallback(
    BluetoothGattInterface* /* gatt_iface */,
    int /* conn_id */,
    btgatt_db_element_t* /* added */,
    int /* added_count */) {
  // Do nothing.
}

void BluetoothGattInterface::ServerObserver::RegisterServerCallback(
    BluetoothGattInterface* /* gatt_iface */,
    int /* status */,
    int /* server_if */,
    const bt_uuid_t& /* app_uuid */) {
  // Do nothing.
}

void BluetoothGattInterface::ServerObserver::ConnectionCallback(
    BluetoothGattInterface* /* gatt_iface */,
    int /* conn_id */,
    int /* server_if */,
    int /* connected */,
    const bt_bdaddr_t& /* bda */) {
  // Do nothing.
}

void BluetoothGattInterface::ServerObserver::ServiceAddedCallback(
    BluetoothGattInterface* /* gatt_iface */,
    int /* status */,
    int /* server_if */,
    const btgatt_srvc_id_t& /* srvc_id */,
    int /* srvc_handle */) {
  // Do nothing.
}

void BluetoothGattInterface::ServerObserver::CharacteristicAddedCallback(
    BluetoothGattInterface* /* gatt_iface */,
    int /* status */,
    int /* server_if */,
    const bt_uuid_t& /* uuid */,
    int /* srvc_handle */,
    int /* char_handle */) {
  // Do nothing.
}

void BluetoothGattInterface::ServerObserver::DescriptorAddedCallback(
    BluetoothGattInterface* /* gatt_iface */,
    int /* status */,
    int /* server_if */,
    const bt_uuid_t& /* uuid */,
    int /* srvc_handle */,
    int /* desc_handle */) {
  // Do nothing.
}

void BluetoothGattInterface::ServerObserver::ServiceStartedCallback(
    BluetoothGattInterface* /* gatt_iface */,
    int /* status */,
    int /* server_if */,
    int /* srvc_handle */) {
  // Do nothing.
}

void BluetoothGattInterface::ServerObserver::ServiceStoppedCallback(
    BluetoothGattInterface* /* gatt_iface */,
    int /* status */,
    int /* server_if */,
    int /* srvc_handle */) {
  // Do nothing.
}

void BluetoothGattInterface::ServerObserver::ServiceDeletedCallback(
    BluetoothGattInterface* /* gatt_iface */,
    int /* status */,
    int /* server_if */,
    int /* srvc_handle */) {
  // Do nothing.
}

void BluetoothGattInterface::ServerObserver::RequestReadCallback(
    BluetoothGattInterface* /* gatt_iface */,
    int /* conn_id */,
    int /* trans_id */,
    const bt_bdaddr_t& /* bda */,
    int /* attr_handle */,
    int /* offset */,
    bool /* is_long */) {
  // Do nothing.
}

void BluetoothGattInterface::ServerObserver::RequestWriteCallback(
    BluetoothGattInterface* /* gatt_iface */,
    int /* conn_id */,
    int /* trans_id */,
    const bt_bdaddr_t& /* bda */,
    int /* attr_handle */,
    int /* offset */,
    int /* length */,
    bool /* need_rsp */,
    bool /* is_prep */,
    uint8_t* /* value */) {
  // Do nothing.
}

void BluetoothGattInterface::ServerObserver::RequestExecWriteCallback(
    BluetoothGattInterface* /* gatt_iface */,
    int /* conn_id */,
    int /* trans_id */,
    const bt_bdaddr_t& /* bda */,
    int /* exec_write */) {
  // Do nothing.
}

void BluetoothGattInterface::ServerObserver::ResponseConfirmationCallback(
    BluetoothGattInterface* /* gatt_iface */,
    int /* status */,
    int /* handle */) {
  // Do nothing
}

void BluetoothGattInterface::ServerObserver::IndicationSentCallback(
    BluetoothGattInterface* /* gatt_iface */,
    int /* conn_id */,
    int /* status */) {
  // Do nothing.
}

void BluetoothGattInterface::ServerObserver::MtuChangedCallback(
    BluetoothGattInterface* /* gatt_iface */,
    int /* conn_id */,
    int /* mtu */) {
  // Do nothing.
}

// static
bool BluetoothGattInterface::Initialize() {
  unique_lock<shared_timed_mutex> lock(g_instance_lock);
  CHECK(!g_interface);

  std::unique_ptr<BluetoothGattInterfaceImpl> impl(
      new BluetoothGattInterfaceImpl());
  if (!impl->Initialize()) {
    LOG(ERROR) << "Failed to initialize BluetoothGattInterface";
    return false;
  }

  g_interface = impl.release();

  return true;
}

// static
void BluetoothGattInterface::CleanUp() {
  unique_lock<shared_timed_mutex> lock(g_instance_lock);
  CHECK(g_interface);

  delete g_interface;
  g_interface = nullptr;
}

// static
bool BluetoothGattInterface::IsInitialized() {
  shared_lock<shared_timed_mutex> lock(g_instance_lock);

  return g_interface != nullptr;
}

// static
BluetoothGattInterface* BluetoothGattInterface::Get() {
  shared_lock<shared_timed_mutex> lock(g_instance_lock);
  CHECK(g_interface);
  return g_interface;
}

// static
void BluetoothGattInterface::InitializeForTesting(
    BluetoothGattInterface* test_instance) {
  unique_lock<shared_timed_mutex> lock(g_instance_lock);
  CHECK(test_instance);
  CHECK(!g_interface);

  g_interface = test_instance;
}

bt_status_t BluetoothGattInterface::StartScan(int client_id) {
  lock_guard<mutex> lock(scan_clients_lock_);

  // Scan already initiated for this client.
  if (scan_client_set_.find(client_id) != scan_client_set_.end()) {
    // Assume starting scan multiple times is not error, but warn user.
    LOG(WARNING) << "Scan already initiated for client";
    return BT_STATUS_SUCCESS;
  }

  // If this is the first scan client, then make a call into the stack. We
  // only do this when the reference count changes to or from 0.
  if (scan_client_set_.empty()) {
    bt_status_t status = GetClientHALInterface()->scan(true);
    if (status != BT_STATUS_SUCCESS) {
      LOG(ERROR) << "HAL call to scan failed";
      return status;
    }
  }

  scan_client_set_.insert(client_id);

  return BT_STATUS_SUCCESS;
}

bt_status_t BluetoothGattInterface::StopScan(int client_id) {
  lock_guard<mutex> lock(scan_clients_lock_);

  // Scan not initiated for this client.
  auto iter = scan_client_set_.find(client_id);
  if (iter == scan_client_set_.end()) {
    // Assume stopping scan multiple times is not error, but warn user.
    LOG(WARNING) << "Scan already stopped or not initiated for client";
    return BT_STATUS_SUCCESS;
  }

  if (scan_client_set_.size() == 1) {
    bt_status_t status = GetClientHALInterface()->scan(false);
    if (status != BT_STATUS_SUCCESS) {
      LOG(ERROR) << "HAL call to stop scan failed";
      return status;
    }
  }

  scan_client_set_.erase(iter);
  return BT_STATUS_SUCCESS;
}

}  // namespace hal
}  // namespace bluetooth
