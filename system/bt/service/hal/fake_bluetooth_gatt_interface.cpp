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

#include "service/hal/fake_bluetooth_gatt_interface.h"

namespace bluetooth {
namespace hal {
namespace {

// The global test handler instances. We have to have globals since the HAL
// interface methods all have to be global and their signatures don't allow us
// to pass in user_data.
std::shared_ptr<FakeBluetoothGattInterface::TestClientHandler> g_client_handler;
std::shared_ptr<FakeBluetoothGattInterface::TestServerHandler> g_server_handler;

bt_status_t FakeRegisterClient(bt_uuid_t* app_uuid) {
  if (g_client_handler)
    return g_client_handler->RegisterClient(app_uuid);

  return BT_STATUS_FAIL;
}

bt_status_t FakeUnregisterClient(int client_if) {
  if (g_client_handler)
    return g_client_handler->UnregisterClient(client_if);

  return BT_STATUS_FAIL;
}

bt_status_t FakeScan(bool start) {
  if (g_client_handler)
    return g_client_handler->Scan(start);

  return BT_STATUS_FAIL;
}

bt_status_t FakeConnect(int client_if, const bt_bdaddr_t *bd_addr,
                        bool is_direct, int transport) {
  if (g_client_handler)
    return g_client_handler->Connect(client_if, bd_addr, is_direct, transport);

  return BT_STATUS_FAIL;
}

bt_status_t FakeDisconnect(int client_if, const bt_bdaddr_t *bd_addr,
                           int conn_id) {
  if (g_client_handler)
    return g_client_handler->Disconnect(client_if, bd_addr, conn_id);

  return BT_STATUS_FAIL;
}

bt_status_t FakeMultiAdvEnable(
    int client_if, int min_interval, int max_interval, int adv_type,
    int chnl_map, int tx_power, int timeout_s) {
  if (g_client_handler)
    return g_client_handler->MultiAdvEnable(client_if, min_interval, max_interval,
                                     adv_type, chnl_map, tx_power, timeout_s);

  return BT_STATUS_FAIL;
}

bt_status_t FakeMultiAdvSetInstData(
    int client_if, bool set_scan_rsp, bool include_name,
    bool incl_txpower, int appearance,
    int manufacturer_len, char* manufacturer_data,
    int service_data_len, char* service_data,
    int service_uuid_len, char* service_uuid) {
  if (g_client_handler)
    return g_client_handler->MultiAdvSetInstData(
        client_if, set_scan_rsp, include_name,
        incl_txpower, appearance,
        manufacturer_len, manufacturer_data,
        service_data_len, service_data,
        service_uuid_len, service_uuid);

  return BT_STATUS_FAIL;
}

bt_status_t FakeMultiAdvDisable(int client_if) {
  if (g_client_handler)
    return g_client_handler->MultiAdvDisable(client_if);

  return BT_STATUS_FAIL;
}

bt_status_t FakeRegisterServer(bt_uuid_t* app_uuid) {
  if (g_server_handler)
    return g_server_handler->RegisterServer(app_uuid);

  return BT_STATUS_FAIL;
}

bt_status_t FakeUnregisterServer(int server_if) {
  if (g_server_handler)
    return g_server_handler->UnregisterServer(server_if);

  return BT_STATUS_FAIL;
}

bt_status_t FakeAddService(
    int server_if, btgatt_srvc_id_t* srvc_id, int num_handles) {
  if (g_server_handler)
    return g_server_handler->AddService(server_if, srvc_id, num_handles);

  return BT_STATUS_FAIL;
}

bt_status_t FakeAddCharacteristic(int server_if, int srvc_handle,
                                  bt_uuid_t *uuid,
                                  int properties, int permissions) {
  if (g_server_handler)
    return g_server_handler->AddCharacteristic(server_if, srvc_handle, uuid,
                                               properties, permissions);

  return BT_STATUS_FAIL;
}

bt_status_t FakeAddDescriptor(int server_if, int srvc_handle,
                              bt_uuid_t* uuid,
                              int permissions) {
  if (g_server_handler)
    return g_server_handler->AddDescriptor(
        server_if, srvc_handle, uuid, permissions);

  return BT_STATUS_FAIL;
}

bt_status_t FakeStartService(
    int server_if, int srvc_handle, int transport) {
  if (g_server_handler)
    return g_server_handler->StartService(server_if, srvc_handle, transport);

  return BT_STATUS_FAIL;
}

bt_status_t FakeDeleteService(int server_if, int srvc_handle) {
  if (g_server_handler)
    return g_server_handler->DeleteService(server_if, srvc_handle);

  return BT_STATUS_FAIL;
}

bt_status_t FakeSendIndication(int server_if, int attribute_handle,
                               int conn_id, int len, int confirm,
                               char* value) {
  if (g_server_handler)
    return g_server_handler->SendIndication(server_if, attribute_handle,
                                            conn_id, len, confirm, value);

  return BT_STATUS_FAIL;
}

bt_status_t FakeSendResponse(int conn_id, int trans_id, int status,
                             btgatt_response_t* response) {
  if (g_server_handler)
    return g_server_handler->SendResponse(conn_id, trans_id, status, response);

  return BT_STATUS_FAIL;
}

btgatt_client_interface_t fake_btgattc_iface = {
  FakeRegisterClient,
  FakeUnregisterClient,
  FakeScan,
  FakeConnect,
  FakeDisconnect,
  nullptr,  // listen
  nullptr,  // refresh
  nullptr,  // search_service
  nullptr,  // read_characteristic
  nullptr,  // write_characteristic
  nullptr,  // read_descriptor
  nullptr,  // write_descriptor
  nullptr,  // execute_write
  nullptr,  // register_for_notification
  nullptr,  // deregister_for_notification
  nullptr,  // read_remote_rssi
  nullptr,  // scan_filter_param_setup
  nullptr,  // scan_filter_add_remove
  nullptr,  // scan_filter_clear
  nullptr,  // scan_filter_enable
  nullptr,  // get_device_type
  nullptr,  // set_adv_data
  nullptr,  // configure_mtu
  nullptr,  // conn_parameter_update
  nullptr,  // set_scan_parameters
  FakeMultiAdvEnable,
  nullptr,  // multi_adv_update
  FakeMultiAdvSetInstData,
  FakeMultiAdvDisable,
  nullptr,  // batchscan_cfg_storate
  nullptr,  // batchscan_enb_batch_scan
  nullptr,  // batchscan_dis_batch_scan
  nullptr,  // batchscan_read_reports
  nullptr,  // test_command
  nullptr,  // get_gatt_db
};

btgatt_server_interface_t fake_btgatts_iface = {
  FakeRegisterServer,
  FakeUnregisterServer,
  nullptr,  // connect
  nullptr,  // disconnect
  FakeAddService,
  nullptr,  // add_included_service
  FakeAddCharacteristic,
  FakeAddDescriptor,
  FakeStartService,
  nullptr,  // stop_service
  FakeDeleteService,
  FakeSendIndication,
  FakeSendResponse,
};

}  // namespace

FakeBluetoothGattInterface::FakeBluetoothGattInterface(
    std::shared_ptr<TestClientHandler> client_handler,
    std::shared_ptr<TestServerHandler> server_handler)
    : client_handler_(client_handler) {
  CHECK(!g_client_handler);
  CHECK(!g_server_handler);

  // We allow passing NULL. In this case all calls we fail by default.
  if (client_handler)
    g_client_handler = client_handler;

  if (server_handler)
    g_server_handler = server_handler;
}

FakeBluetoothGattInterface::~FakeBluetoothGattInterface() {
  if (g_client_handler)
    g_client_handler = nullptr;

  if (g_server_handler)
    g_server_handler = nullptr;
}

// The methods below can be used to notify observers with certain events and
// given parameters.
void FakeBluetoothGattInterface::NotifyRegisterClientCallback(
    int status, int client_if,
    const bt_uuid_t& app_uuid) {
  FOR_EACH_OBSERVER(ClientObserver, client_observers_,
                    RegisterClientCallback(this, status, client_if, app_uuid));
}

void FakeBluetoothGattInterface::NotifyConnectCallback(
    int conn_id, int status, int client_if, const bt_bdaddr_t& bda) {
  FOR_EACH_OBSERVER(ClientObserver, client_observers_,
                    ConnectCallback(this, conn_id, status, client_if, bda));
}

void FakeBluetoothGattInterface::NotifyDisconnectCallback(
    int conn_id, int status, int client_if, const bt_bdaddr_t& bda) {
  FOR_EACH_OBSERVER(ClientObserver, client_observers_,
                    DisconnectCallback(this, conn_id, status, client_if, bda));
}

void FakeBluetoothGattInterface::NotifyScanResultCallback(
    const bt_bdaddr_t& bda, int rssi, uint8_t* adv_data) {
  FOR_EACH_OBSERVER(ClientObserver, client_observers_,
                    ScanResultCallback(this, bda, rssi, adv_data));
}

void FakeBluetoothGattInterface::NotifyMultiAdvEnableCallback(
    int client_if, int status) {
  FOR_EACH_OBSERVER(ClientObserver, client_observers_,
                    MultiAdvEnableCallback(this, client_if, status));
}

void FakeBluetoothGattInterface::NotifyMultiAdvDataCallback(
    int client_if, int status) {
  FOR_EACH_OBSERVER(ClientObserver, client_observers_,
                    MultiAdvDataCallback(this, client_if, status));
}

void FakeBluetoothGattInterface::NotifyMultiAdvDisableCallback(
    int client_if, int status) {
  FOR_EACH_OBSERVER(ClientObserver, client_observers_,
                    MultiAdvDisableCallback(this, client_if, status));
}

void FakeBluetoothGattInterface::NotifyRegisterServerCallback(
    int status, int server_if,
    const bt_uuid_t& app_uuid) {
  FOR_EACH_OBSERVER(ServerObserver, server_observers_,
                    RegisterServerCallback(this, status, server_if, app_uuid));
}

void FakeBluetoothGattInterface::NotifyServerConnectionCallback(
    int conn_id, int server_if, int connected, const bt_bdaddr_t& bda) {
  FOR_EACH_OBSERVER(
      ServerObserver, server_observers_,
      ConnectionCallback(this, conn_id, server_if, connected, bda));
}

void FakeBluetoothGattInterface::NotifyServiceAddedCallback(
    int status, int server_if,
    const btgatt_srvc_id_t& srvc_id,
    int srvc_handle) {
  FOR_EACH_OBSERVER(
      ServerObserver, server_observers_,
      ServiceAddedCallback(this, status, server_if, srvc_id, srvc_handle));
}

void FakeBluetoothGattInterface::NotifyCharacteristicAddedCallback(
    int status, int server_if,
    const bt_uuid_t& uuid,
    int srvc_handle, int char_handle) {
  FOR_EACH_OBSERVER(
      ServerObserver, server_observers_,
      CharacteristicAddedCallback(
          this, status, server_if, uuid, srvc_handle, char_handle));
}

void FakeBluetoothGattInterface::NotifyDescriptorAddedCallback(
    int status, int server_if,
    const bt_uuid_t& uuid,
    int srvc_handle, int desc_handle) {
  FOR_EACH_OBSERVER(
      ServerObserver, server_observers_,
      DescriptorAddedCallback(
          this, status, server_if, uuid, srvc_handle, desc_handle));
}

void FakeBluetoothGattInterface::NotifyServiceStartedCallback(
    int status, int server_if, int srvc_handle) {
  FOR_EACH_OBSERVER(
      ServerObserver, server_observers_,
      ServiceStartedCallback(this, status, server_if, srvc_handle));
}

void FakeBluetoothGattInterface::NotifyRequestReadCallback(
    int conn_id, int trans_id, const bt_bdaddr_t& bda, int attr_handle,
    int offset, bool is_long) {
  FOR_EACH_OBSERVER(
      ServerObserver, server_observers_,
      RequestReadCallback(
          this, conn_id, trans_id, bda, attr_handle, offset, is_long));
}

void FakeBluetoothGattInterface::NotifyRequestWriteCallback(
    int conn_id, int trans_id,
    const bt_bdaddr_t& bda, int attr_handle,
    int offset, int length,
    bool need_rsp, bool is_prep, uint8_t* value) {
  FOR_EACH_OBSERVER(
      ServerObserver, server_observers_,
      RequestWriteCallback(
          this, conn_id, trans_id, bda, attr_handle, offset, length, need_rsp,
          is_prep, value));
}

void FakeBluetoothGattInterface::NotifyRequestExecWriteCallback(
    int conn_id, int trans_id, const bt_bdaddr_t& bda, int exec_write) {
  FOR_EACH_OBSERVER(
      ServerObserver, server_observers_,
      RequestExecWriteCallback(this, conn_id, trans_id, bda, exec_write));
}

void FakeBluetoothGattInterface::NotifyIndicationSentCallback(
    int conn_id, int status) {
  FOR_EACH_OBSERVER(ServerObserver, server_observers_,
                    IndicationSentCallback(this, conn_id, status));
}

void FakeBluetoothGattInterface::AddClientObserver(ClientObserver* observer) {
  CHECK(observer);
  client_observers_.AddObserver(observer);
}

void FakeBluetoothGattInterface::RemoveClientObserver(
    ClientObserver* observer) {
  CHECK(observer);
  client_observers_.RemoveObserver(observer);
}

void FakeBluetoothGattInterface::AddServerObserver(ServerObserver* observer) {
  CHECK(observer);
  server_observers_.AddObserver(observer);
}

void FakeBluetoothGattInterface::RemoveServerObserver(
    ServerObserver* observer) {
  CHECK(observer);
  server_observers_.RemoveObserver(observer);
}

const btgatt_client_interface_t*
FakeBluetoothGattInterface::GetClientHALInterface() const {
  return &fake_btgattc_iface;
}

const btgatt_server_interface_t*
FakeBluetoothGattInterface::GetServerHALInterface() const {
  return &fake_btgatts_iface;
}

}  // namespace hal
}  // namespace bluetooth
