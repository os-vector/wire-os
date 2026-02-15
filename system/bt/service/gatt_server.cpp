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

#include "service/gatt_server.h"

#include "service/common/bluetooth/util/address_helper.h"
#include "service/hal/gatt_helpers.h"
#include "service/logging_helpers.h"

using std::lock_guard;
using std::mutex;

namespace bluetooth {

namespace {

bool operator==(const bt_bdaddr_t& lhs, const bt_bdaddr_t& rhs) {
  return memcmp(&lhs, &rhs, sizeof(lhs)) == 0;
}

bool operator!=(const bt_bdaddr_t& lhs, const bt_bdaddr_t& rhs) {
  return !(lhs == rhs);
}

}  // namespace

// GattServer implementation
// ========================================================

GattServer::GattServer(const UUID& uuid, int server_id)
    : app_identifier_(uuid),
      server_id_(server_id),
      delegate_(nullptr) {
}

GattServer::~GattServer() {
  // Automatically unregister the server.
  VLOG(1) << "GattServer unregistering: " << server_id_;

  // Unregister as observer so we no longer receive any callbacks.
  hal::BluetoothGattInterface::Get()->RemoveServerObserver(this);

  // Unregister this server, stop all services, and ignore the result.
  // TODO(armansito): stop and remove all services here? unregister_server
  // should really take care of that.
  hal::BluetoothGattInterface::Get()->
      GetServerHALInterface()->unregister_server(server_id_);
}

void GattServer::SetDelegate(Delegate* delegate) {
  lock_guard<mutex> lock(mutex_);
  delegate_ = delegate;
}

const UUID& GattServer::GetAppIdentifier() const {
  return app_identifier_;
}

int GattServer::GetInstanceId() const {
  return server_id_;
}

std::unique_ptr<GattIdentifier> GattServer::BeginServiceDeclaration(
    const UUID& uuid, bool is_primary) {
  VLOG(1) << __func__ << " server_id: " << server_id_
          << " - UUID: " << uuid.ToString()
          << ", is_primary: " << is_primary;
  lock_guard<mutex> lock(mutex_);

  if (pending_decl_) {
    LOG(ERROR) << "Already began service declaration";
    return nullptr;
  }

  CHECK(!pending_id_);
  CHECK(!pending_decl_);
  CHECK(!pending_end_decl_cb_);

  auto service_id = GetIdForService(uuid, is_primary);
  CHECK(service_id);

  // Pass 0 for permissions and properties as this is a service decl.
  AttributeEntry entry(
      *service_id, kCharacteristicPropertyNone, kAttributePermissionNone);

  pending_decl_.reset(new ServiceDeclaration());
  pending_decl_->num_handles++;  // 1 handle for the service decl. attribute
  pending_decl_->service_id = *service_id;
  pending_decl_->attributes.push_back(entry);

  return service_id;
}

std::unique_ptr<GattIdentifier> GattServer::AddCharacteristic(
      const UUID& uuid, int properties, int permissions) {
  VLOG(1) << __func__ << " server_id: " << server_id_
          << " - UUID: " << uuid.ToString()
          << ", properties: " << properties
          << ", permissions: " << permissions;
  lock_guard<mutex> lock(mutex_);

  if (!pending_decl_) {
    LOG(ERROR) << "Service declaration not begun";
    return nullptr;
  }

  if (pending_end_decl_cb_) {
    LOG(ERROR) << "EndServiceDeclaration in progress, cannot modify service";
    return nullptr;
  }

  auto char_id = GetIdForCharacteristic(uuid);
  CHECK(char_id);
  AttributeEntry entry(*char_id, properties, permissions);

  // 2 handles for the characteristic declaration and the value attributes.
  pending_decl_->num_handles += 2;
  pending_decl_->attributes.push_back(entry);

  return char_id;
}

std::unique_ptr<GattIdentifier> GattServer::AddDescriptor(
    const UUID& uuid, int permissions) {
  VLOG(1) << __func__ << " server_id: " << server_id_
          << " - UUID: " << uuid.ToString()
          << ", permissions: " << permissions;
  lock_guard<mutex> lock(mutex_);

  if (!pending_decl_) {
    LOG(ERROR) << "Service declaration not begun";
    return nullptr;
  }

  if (pending_end_decl_cb_) {
    LOG(ERROR) << "EndServiceDeclaration in progress, cannot modify service";
    return nullptr;
  }

  auto desc_id = GetIdForDescriptor(uuid);
  if (!desc_id)
    return nullptr;

  AttributeEntry entry(*desc_id, kCharacteristicPropertyNone, permissions);

  // 1 handle for the descriptor attribute.
  pending_decl_->num_handles += 1;
  pending_decl_->attributes.push_back(entry);

  return desc_id;
}

bool GattServer::EndServiceDeclaration(const ResultCallback& callback) {
  VLOG(1) << __func__ << " server_id: " << server_id_;
  lock_guard<mutex> lock(mutex_);

  if (!callback) {
    LOG(ERROR) << "|callback| cannot be NULL";
    return false;
  }

  if (!pending_decl_) {
    LOG(ERROR) << "Service declaration not begun";
    return false;
  }

  if (pending_end_decl_cb_) {
    LOG(ERROR) << "EndServiceDeclaration already in progress";
    return false;
  }

  CHECK(!pending_id_);

  // There has to be at least one entry here for the service declaration
  // attribute.
  CHECK(pending_decl_->num_handles > 0);
  CHECK(!pending_decl_->attributes.empty());

  std::unique_ptr<GattIdentifier> service_id = PopNextId();
  CHECK(service_id->IsService());
  CHECK(*service_id == pending_decl_->service_id);

  btgatt_srvc_id_t hal_id;
  hal::GetHALServiceId(*service_id, &hal_id);

  bt_status_t status = hal::BluetoothGattInterface::Get()->
      GetServerHALInterface()->add_service(
          server_id_, &hal_id, pending_decl_->num_handles);
  if (status != BT_STATUS_SUCCESS) {
    LOG(ERROR) << "Failed to initiate call to populate GATT service";
    CleanUpPendingData();
    return false;
  }

  pending_id_ = std::move(service_id);
  pending_end_decl_cb_ = callback;

  return true;
}

std::unique_ptr<GattIdentifier> GattServer::GetIdForService(
    const UUID& uuid, bool is_primary) {
  // Calculate the instance ID for this service by searching through the handle
  // map to see how many occurrences of the same service UUID we find.
  int inst_id = 0;
  for (const auto& iter : id_to_handle_map_) {
    const GattIdentifier* gatt_id = &iter.first;

    if (!gatt_id->IsService())
      continue;

    if (gatt_id->service_uuid() == uuid)
      ++inst_id;
  }

  // Pass empty string for the address as this is a local service.
  return GattIdentifier::CreateServiceId("", inst_id, uuid, is_primary);
}

std::unique_ptr<GattIdentifier> GattServer::GetIdForCharacteristic(
    const UUID& uuid) {
  CHECK(pending_decl_);

  // Calculate the instance ID for this characteristic by searching through the
  // pending entries.
  int inst_id = 0;
  for (const auto& entry : pending_decl_->attributes) {
    const GattIdentifier& gatt_id = entry.id;

    if (!gatt_id.IsCharacteristic())
      continue;

    if (gatt_id.characteristic_uuid() == uuid)
      ++inst_id;
  }

  CHECK(pending_decl_->service_id.IsService());

  return GattIdentifier::CreateCharacteristicId(
      inst_id, uuid, pending_decl_->service_id);
}

std::unique_ptr<GattIdentifier> GattServer::GetIdForDescriptor(
    const UUID& uuid) {
  CHECK(pending_decl_);

  // Calculate the instance ID for this descriptor by searching through the
  // pending entries. We iterate in reverse until we find a characteristic
  // entry.
  CHECK(!pending_decl_->attributes.empty());
  int inst_id = 0;
  bool char_found = false;
  GattIdentifier char_id;
  for (auto iter = pending_decl_->attributes.end() - 1;
       iter != pending_decl_->attributes.begin();  // Begin is always a service
       --iter) {
    const GattIdentifier& gatt_id = iter->id;

    if (gatt_id.IsCharacteristic()) {
      // Found the owning characteristic.
      char_found = true;
      char_id = gatt_id;
      break;
    }

    if (!gatt_id.IsDescriptor()) {
      // A descriptor must be preceded by a descriptor or a characteristic.
      LOG(ERROR) << "Descriptors must come directly after a characteristic or "
                 << "another descriptor.";
      return nullptr;
    }

    if (gatt_id.descriptor_uuid() == uuid)
      ++inst_id;
  }

  if (!char_found) {
    LOG(ERROR) << "No characteristic found to add the descriptor to.";
    return nullptr;
  }

  return GattIdentifier::CreateDescriptorId(inst_id, uuid, char_id);
}

bool GattServer::SendResponse(
    const std::string& device_address, int request_id,
    GATTError error, int offset,
    const std::vector<uint8_t>& value) {
  VLOG(1) << __func__ << " - server_id: " << server_id_
          << " device_address: " << device_address
          << " request_id: " << request_id
          << " error: " << error
          << " offset: " << offset;
  lock_guard<mutex> lock(mutex_);

  bt_bdaddr_t addr;
  if (!util::BdAddrFromString(device_address, &addr)) {
    LOG(ERROR) << "Invalid device address given: " << device_address;
    return false;
  }

  if (value.size() + offset > BTGATT_MAX_ATTR_LEN) {
    LOG(ERROR) << "Value is too large";
    return false;
  }

  // Find the correct connection ID for |device_address| and |request_id|.
  auto iter = conn_addr_map_.find(device_address);
  if (iter == conn_addr_map_.end()) {
    LOG(ERROR) << "No known connections for device address: " << device_address;
    return false;
  }

  std::shared_ptr<Connection> connection;
  for (auto tmp : iter->second) {
    if (tmp->request_id_to_handle.find(request_id) ==
        tmp->request_id_to_handle.end())
      continue;

    connection = tmp;
  }

  if (!connection) {
    LOG(ERROR) << "Pending request with ID " << request_id
               << " not found for device with BD_ADDR: " << device_address;
    return false;
  }

  btgatt_response_t response;
  memset(&response, 0, sizeof(response));

  // We keep -1 as the handle for "Execute Write Request". In that case,
  // there is no need to populate the response data. Just send zeros back.
  int handle = connection->request_id_to_handle[request_id];
  response.handle = handle;
  response.attr_value.handle = handle;
  if (handle != -1) {
    memcpy(response.attr_value.value, value.data(), value.size());
    response.attr_value.offset = offset;
    response.attr_value.len = value.size();
  }

  bt_status_t result = hal::BluetoothGattInterface::Get()->
      GetServerHALInterface()->send_response(
          connection->conn_id, request_id, error, &response);
  if (result != BT_STATUS_SUCCESS) {
    LOG(ERROR) << "Failed to initiate call to send GATT response";
    return false;
  }

  connection->request_id_to_handle.erase(request_id);

  return true;
}

bool GattServer::SendNotification(
    const std::string& device_address,
    const GattIdentifier& characteristic_id,
    bool confirm,
    const std::vector<uint8_t>& value,
    const GattCallback& callback) {
  VLOG(1) << " - server_id: " << server_id_
          << " device_address: " << device_address
          << " confirm: " << confirm;
  lock_guard<mutex> lock(mutex_);

  bt_bdaddr_t addr;
  if (!util::BdAddrFromString(device_address, &addr)) {
    LOG(ERROR) << "Invalid device address given: " << device_address;
    return false;
  }

  // Get the connection IDs for which we will send this notification.
  auto conn_iter = conn_addr_map_.find(device_address);
  if (conn_iter == conn_addr_map_.end()) {
    LOG(ERROR) << "No known connections for device with address: "
               << device_address;
    return false;
  }

  // Make sure that |characteristic_id| matches a valid attribute handle.
  auto handle_iter = id_to_handle_map_.find(characteristic_id);
  if (handle_iter == id_to_handle_map_.end()) {
    LOG(ERROR) << "Unknown characteristic";
    return false;
  }

  std::shared_ptr<PendingIndication> pending_ind(
      new PendingIndication(callback));

  // Send the notification/indication on all matching connections.
  int send_count = 0;
  for (auto conn : conn_iter->second) {
    // Make sure that one isn't already pending for this connection.
    if (pending_indications_.find(conn->conn_id) !=
        pending_indications_.end()) {
      VLOG(1) << "A" << (confirm ? "n indication" : " notification")
              << " is already pending for connection: " << conn->conn_id;
      continue;
    }

    // The HAL API takes char* rather const char* for |value|, so we have to
    // cast away the const.
    // TODO(armansito): Make HAL accept const char*.
    bt_status_t status = hal::BluetoothGattInterface::Get()->
        GetServerHALInterface()->send_indication(
            server_id_,
            handle_iter->second,
            conn->conn_id,
            value.size(),
            confirm,
            reinterpret_cast<char*>(const_cast<uint8_t*>(value.data())));

    // Increment the send count if this was successful. We don't immediately
    // fail if the HAL returned an error. It's better to report success as long
    // as we sent out at least one notification to this device as
    // multi-transport GATT connections from the same BD_ADDR will be rare
    // enough already.
    if (status != BT_STATUS_SUCCESS)
      continue;

    send_count++;
    pending_indications_[conn->conn_id] = pending_ind;
  }

  if (send_count == 0) {
    LOG(ERROR) << "Failed to send notifications/indications to device: "
               << device_address;
    return false;
  }

  return true;
}

void GattServer::ConnectionCallback(
    hal::BluetoothGattInterface* /* gatt_iface */,
    int conn_id, int server_id,
    int connected,
    const bt_bdaddr_t& bda) {
  lock_guard<mutex> lock(mutex_);

  if (server_id != server_id_)
    return;

  std::string device_address = BtAddrString(&bda);

  VLOG(1) << __func__ << " conn_id: " << conn_id << " connected: " << connected
          << " BD_ADDR: " << device_address;

  if (!connected) {
    // Erase the entry if we were connected to it.
    VLOG(1) << "No longer connected: " << device_address;
    conn_id_map_.erase(conn_id);
    auto iter = conn_addr_map_.find(device_address);
    if (iter == conn_addr_map_.end())
      return;

    // Remove the appropriate connection objects in the address.
    for (auto conn_iter = iter->second.begin(); conn_iter != iter->second.end();
         ++conn_iter) {
      if ((*conn_iter)->conn_id != conn_id)
        continue;

      iter->second.erase(conn_iter);
      break;
    }

    return;
  }

  if (conn_id_map_.find(conn_id) != conn_id_map_.end()) {
    LOG(WARNING) << "Connection entry already exists; "
                 << "ignoring ConnectionCallback";
    return;
  }

  LOG(INFO) << "Added connection entry for conn_id: " << conn_id
            << " device address: " << device_address;
  std::shared_ptr<Connection> connection(new Connection(conn_id, bda));
  conn_id_map_[conn_id] = connection;
  conn_addr_map_[device_address].push_back(connection);
}

void GattServer::ServiceAddedCallback(
    hal::BluetoothGattInterface* gatt_iface,
    int status, int server_id,
    const btgatt_srvc_id_t& srvc_id,
    int service_handle) {
  lock_guard<mutex> lock(mutex_);

  if (server_id != server_id_)
    return;

  // Construct a GATT identifier.
  auto gatt_id = hal::GetServiceIdFromHAL(srvc_id);
  CHECK(pending_id_);
  CHECK(*gatt_id == *pending_id_);
  CHECK(*gatt_id == pending_decl_->service_id);
  CHECK(pending_id_->IsService());

  VLOG(1) << __func__ << " - status: " << status
          << " server_id: " << server_id
          << " handle: " << service_handle
          << " UUID: " << gatt_id->service_uuid().ToString();

  if (status != BT_STATUS_SUCCESS) {
    NotifyEndCallbackAndClearData(static_cast<BLEStatus>(status), *gatt_id);
    return;
  }

  // Add this to the handle map.
  pending_handle_map_[*gatt_id] = service_handle;
  CHECK(-1 == pending_decl_->service_handle);
  pending_decl_->service_handle = service_handle;

  HandleNextEntry(gatt_iface);
}

void GattServer::CharacteristicAddedCallback(
    hal::BluetoothGattInterface* gatt_iface,
    int status, int server_id,
    const bt_uuid_t& uuid,
    int service_handle,
    int char_handle) {
  lock_guard<mutex> lock(mutex_);

  if (server_id != server_id_)
    return;

  CHECK(pending_decl_);
  CHECK(pending_decl_->service_handle == service_handle);
  CHECK(pending_id_);
  CHECK(pending_id_->IsCharacteristic());
  CHECK(pending_id_->characteristic_uuid() == UUID(uuid));

  VLOG(1) << __func__ << " - status: " << status
          << " server_id: " << server_id
          << " service_handle: " << service_handle
          << " char_handle: " << char_handle;

  if (status != BT_STATUS_SUCCESS) {
    NotifyEndCallbackAndClearData(static_cast<BLEStatus>(status),
                                  pending_decl_->service_id);
    return;
  }

  // Add this to the handle map and continue.
  pending_handle_map_[*pending_id_] = char_handle;
  HandleNextEntry(gatt_iface);
}

void GattServer::DescriptorAddedCallback(
    hal::BluetoothGattInterface* gatt_iface,
    int status, int server_id,
    const bt_uuid_t& uuid,
    int service_handle,
    int desc_handle) {
  lock_guard<mutex> lock(mutex_);

  if (server_id != server_id_)
    return;

  CHECK(pending_decl_);
  CHECK(pending_decl_->service_handle == service_handle);
  CHECK(pending_id_);
  CHECK(pending_id_->IsDescriptor());
  CHECK(pending_id_->descriptor_uuid() == UUID(uuid));

  VLOG(1) << __func__ << " - status: " << status
          << " server_id: " << server_id
          << " service_handle: " << service_handle
          << " desc_handle: " << desc_handle;

  if (status != BT_STATUS_SUCCESS) {
    NotifyEndCallbackAndClearData(static_cast<BLEStatus>(status),
                                  pending_decl_->service_id);
    return;
  }

  // Add this to the handle map and contiue.
  pending_handle_map_[*pending_id_] = desc_handle;
  HandleNextEntry(gatt_iface);
}

void GattServer::ServiceStartedCallback(
    hal::BluetoothGattInterface* gatt_iface,
    int status, int server_id,
    int service_handle) {
  lock_guard<mutex> lock(mutex_);

  if (server_id != server_id_)
    return;

  CHECK(pending_id_);
  CHECK(pending_decl_);
  CHECK(pending_decl_->service_handle == service_handle);

  VLOG(1) << __func__ << " - server_id: " << server_id
          << " handle: " << service_handle;

  // If we failed to start the service, remove it from the database and ignore
  // the result.
  if (status != BT_STATUS_SUCCESS) {
    gatt_iface->GetServerHALInterface()->delete_service(
        server_id_, service_handle);
  }

  // Complete the operation.
  NotifyEndCallbackAndClearData(static_cast<BLEStatus>(status),
                                pending_decl_->service_id);
}

void GattServer::ServiceStoppedCallback(
    hal::BluetoothGattInterface* /* gatt_iface */,
    int /* status */,
    int /* server_id */,
    int /* service_handle */) {
  // TODO(armansito): Support stopping a service.
}

void GattServer::RequestReadCallback(
    hal::BluetoothGattInterface* /* gatt_iface */,
    int conn_id, int trans_id,
    const bt_bdaddr_t& bda,
    int attribute_handle, int offset,
    bool is_long) {
  lock_guard<mutex> lock(mutex_);

  // Check to see if we know about this connection. Otherwise ignore the
  // request.
  auto conn = GetConnection(conn_id, bda, trans_id);
  if (!conn)
    return;

  std::string device_address = BtAddrString(&bda);

  VLOG(1) << __func__ << " - conn_id: " << conn_id << " trans_id: " << trans_id
          << " BD_ADDR: " << device_address
          << " attribute_handle: " << attribute_handle << " offset: " << offset
          << " is_long: " << is_long;

  // Make sure that the handle is valid.
  auto iter = handle_to_id_map_.find(attribute_handle);
  if (iter == handle_to_id_map_.end()) {
    LOG(ERROR) << "Request received for unknown handle: " << attribute_handle;
    return;
  }

  conn->request_id_to_handle[trans_id] = attribute_handle;

  // If there is no delegate then there is nobody to handle request. The request
  // will eventually timeout and we should get a connection update that
  // terminates the connection.
  if (!delegate_) {
    // TODO(armansito): Require a delegate at server registration so that this
    // is never possible.
    LOG(WARNING) << "No delegate was assigned to GattServer. Incoming request "
                 << "will time out.";
    return;
  }

  if (iter->second.IsCharacteristic()) {
    delegate_->OnCharacteristicReadRequest(
        this, device_address, trans_id, offset, is_long, iter->second);
  } else if (iter->second.IsDescriptor()) {
    delegate_->OnDescriptorReadRequest(
        this, device_address, trans_id, offset, is_long, iter->second);
  } else {
    // Our API only delegates to applications those read requests for
    // characteristic value and descriptor attributes. Everything else should be
    // handled by the stack.
    LOG(WARNING) << "Read request received for unsupported attribute";
  }
}

void GattServer::RequestWriteCallback(
    hal::BluetoothGattInterface* /* gatt_iface */,
    int conn_id, int trans_id,
    const bt_bdaddr_t& bda,
    int attr_handle, int offset, int length,
    bool need_rsp, bool is_prep, uint8_t* value) {
  lock_guard<mutex> lock(mutex_);

  if (length < 0) {
    LOG(WARNING) << "Negative length value received";
    return;
  }

  // Check to see if we know about this connection. Otherwise ignore the
  // request.
  auto conn = GetConnection(conn_id, bda, trans_id);
  if (!conn)
    return;

  std::string device_address = BtAddrString(&bda);

  VLOG(1) << __func__ << " - conn_id: " << conn_id << " trans_id: " << trans_id
          << " BD_ADDR: " << device_address
          << " attr_handle: " << attr_handle << " offset: " << offset
          << " length: " << length << " need_rsp: " << need_rsp
          << " is_prep: " << is_prep;

  // Make sure that the handle is valid.
  auto iter = handle_to_id_map_.find(attr_handle);
  if (iter == handle_to_id_map_.end()) {
    LOG(ERROR) << "Request received for unknown handle: " << attr_handle;
    return;
  }

  // Store the request ID only if this is not a write-without-response. If
  // another request occurs after this with the same request ID, then we'll
  // simply process it normally, though that shouldn't ever happen.
  if (need_rsp)
    conn->request_id_to_handle[trans_id] = attr_handle;

  // If there is no delegate then there is nobody to handle request. The request
  // will eventually timeout and we should get a connection update that
  // terminates the connection.
  if (!delegate_) {
    // TODO(armansito): Require a delegate at server registration so that this
    // is never possible.
    LOG(WARNING) << "No delegate was assigned to GattServer. Incoming request "
                 << "will time out.";
    return;
  }

  std::vector<uint8_t> value_vec(value, value + length);

  if (iter->second.IsCharacteristic()) {
    delegate_->OnCharacteristicWriteRequest(
        this, device_address, trans_id, offset, is_prep, need_rsp,
        value_vec, iter->second);
  } else if (iter->second.IsDescriptor()) {
    delegate_->OnDescriptorWriteRequest(
        this, device_address, trans_id, offset, is_prep, need_rsp,
        value_vec, iter->second);
  } else {
    // Our API only delegates to applications those read requests for
    // characteristic value and descriptor attributes. Everything else should be
    // handled by the stack.
    LOG(WARNING) << "Write request received for unsupported attribute";
  }
}

void GattServer::RequestExecWriteCallback(
    hal::BluetoothGattInterface* /* gatt_iface */,
    int conn_id, int trans_id,
    const bt_bdaddr_t& bda, int exec_write) {
  lock_guard<mutex> lock(mutex_);

  // Check to see if we know about this connection. Otherwise ignore the
  // request.
  auto conn = GetConnection(conn_id, bda, trans_id);
  if (!conn)
    return;

  std::string device_address = BtAddrString(&bda);

  VLOG(1) << __func__ << " - conn_id: " << conn_id << " trans_id: " << trans_id
          << " BD_ADDR: " << device_address << " exec_write: " << exec_write;

  // Just store a dummy invalid handle as this request doesn't apply to a
  // specific handle.
  conn->request_id_to_handle[trans_id] = -1;

  // If there is no delegate then there is nobody to handle request. The request
  // will eventually timeout and we should get a connection update that
  // terminates the connection.
  if (!delegate_) {
    // TODO(armansito): Require a delegate at server registration so that this
    // is never possible.
    LOG(WARNING) << "No delegate was assigned to GattServer. Incoming request "
                 << "will time out.";
    return;
  }

  delegate_->OnExecuteWriteRequest(this, device_address, trans_id, exec_write);
}

void GattServer::IndicationSentCallback(
    hal::BluetoothGattInterface* /* gatt_iface */,
    int conn_id, int status) {
  VLOG(1) << __func__ << " conn_id: " << conn_id << " status: " << status;
  lock_guard<mutex> lock(mutex_);

  const auto& pending_ind_iter = pending_indications_.find(conn_id);
  if (pending_ind_iter == pending_indications_.end()) {
    VLOG(1) << "Unknown connection: " << conn_id;
    return;
  }

  std::shared_ptr<PendingIndication> pending_ind = pending_ind_iter->second;
  pending_indications_.erase(pending_ind_iter);

  if (status == BT_STATUS_SUCCESS)
    pending_ind->has_success = true;

  // Invoke it if this was the last reference to the confirmation callback.
  if (pending_ind.unique() && pending_ind->callback) {
    pending_ind->callback(
        pending_ind->has_success ?
        GATT_ERROR_NONE : static_cast<GATTError>(status));
  }
}

void GattServer::NotifyEndCallbackAndClearData(
    BLEStatus status, const GattIdentifier& id) {
  VLOG(1) << __func__ << " status: " << status;
  CHECK(pending_end_decl_cb_);

  if (status == BLE_STATUS_SUCCESS) {
    id_to_handle_map_.insert(pending_handle_map_.begin(),
                             pending_handle_map_.end());
    for (auto& iter : pending_handle_map_)
      handle_to_id_map_[iter.second] = iter.first;
  }

  pending_end_decl_cb_(status, id);

  CleanUpPendingData();
}

void GattServer::CleanUpPendingData() {
  pending_id_ = nullptr;
  pending_decl_ = nullptr;
  pending_end_decl_cb_ = ResultCallback();
  pending_handle_map_.clear();
}

void GattServer::HandleNextEntry(hal::BluetoothGattInterface* gatt_iface) {
  CHECK(pending_decl_);
  CHECK(gatt_iface);

  auto next_entry = PopNextEntry();
  if (!next_entry) {
    // No more entries. Call start_service to finish up.
    bt_status_t status = gatt_iface->GetServerHALInterface()->start_service(
        server_id_,
        pending_decl_->service_handle,
        TRANSPORT_BREDR | TRANSPORT_LE);

    // Terminate the procedure in the case of an error.
    if (status != BT_STATUS_SUCCESS) {
      NotifyEndCallbackAndClearData(static_cast<BLEStatus>(status),
                                    pending_decl_->service_id);
    }

    return;
  }

  if (next_entry->id.IsCharacteristic()) {
    bt_uuid_t char_uuid = next_entry->id.characteristic_uuid().GetBlueDroid();
    bt_status_t status = gatt_iface->GetServerHALInterface()->
        add_characteristic(
            server_id_,
            pending_decl_->service_handle,
            &char_uuid,
            next_entry->char_properties,
            next_entry->permissions);

    // Terminate the procedure in the case of an error.
    if (status != BT_STATUS_SUCCESS) {
      NotifyEndCallbackAndClearData(static_cast<BLEStatus>(status),
                                    pending_decl_->service_id);
      return;
    }

    pending_id_.reset(new GattIdentifier(next_entry->id));
    return;
  }

  if (next_entry->id.IsDescriptor()) {
    bt_uuid_t desc_uuid = next_entry->id.descriptor_uuid().GetBlueDroid();
    bt_status_t status = gatt_iface->GetServerHALInterface()->
        add_descriptor(
            server_id_,
            pending_decl_->service_handle,
            &desc_uuid,
            next_entry->permissions);

    // Terminate the procedure in the case of an error.
    if (status != BT_STATUS_SUCCESS) {
      NotifyEndCallbackAndClearData(static_cast<BLEStatus>(status),
                                    pending_decl_->service_id);
      return;
    }

    pending_id_.reset(new GattIdentifier(next_entry->id));
    return;
  }

  NOTREACHED() << "Unexpected entry type";
}

std::shared_ptr<GattServer::Connection> GattServer::GetConnection(
    int conn_id, const bt_bdaddr_t& bda, int request_id) {
  auto iter = conn_id_map_.find(conn_id);
  if (iter == conn_id_map_.end()) {
    VLOG(1) << "Connection doesn't belong to this server";
    return nullptr;
  }

  auto conn = iter->second;
  if (conn->bdaddr != bda) {
    LOG(WARNING) << "BD_ADDR: " << BtAddrString(&bda) << " doesn't match "
                 << "connection ID: " << conn_id;
    return nullptr;
  }

  if (conn->request_id_to_handle.find(request_id) !=
      conn->request_id_to_handle.end()) {
    VLOG(1) << "Request with ID: " << request_id << " already exists for "
            << " connection: " << conn_id;
    return nullptr;
  }

  return conn;
}

std::unique_ptr<GattServer::AttributeEntry> GattServer::PopNextEntry() {
  CHECK(pending_decl_);

  if (pending_decl_->attributes.empty())
    return nullptr;

  const auto& next = pending_decl_->attributes.front();
  std::unique_ptr<AttributeEntry> entry(new AttributeEntry(next));

  pending_decl_->attributes.pop_front();

  return entry;
}

std::unique_ptr<GattIdentifier> GattServer::PopNextId() {
  auto entry = PopNextEntry();
  if (!entry)
    return nullptr;

  return std::unique_ptr<GattIdentifier>(new GattIdentifier(entry->id));
}

// GattServerFactory implementation
// ========================================================

GattServerFactory::GattServerFactory() {
  hal::BluetoothGattInterface::Get()->AddServerObserver(this);
}

GattServerFactory::~GattServerFactory() {
  hal::BluetoothGattInterface::Get()->RemoveServerObserver(this);
}

bool GattServerFactory::RegisterInstance(
    const UUID& uuid,
    const RegisterCallback& callback) {
  VLOG(1) << __func__ << " - UUID: " << uuid.ToString();
  lock_guard<mutex> lock(pending_calls_lock_);

  if (pending_calls_.find(uuid) != pending_calls_.end()) {
    LOG(ERROR) << "GATT-server client with given UUID already being registered "
               << " - UUID: " << uuid.ToString();
    return false;
  }

  const btgatt_server_interface_t* hal_iface =
      hal::BluetoothGattInterface::Get()->GetServerHALInterface();
  bt_uuid_t app_uuid = uuid.GetBlueDroid();

  if (hal_iface->register_server(&app_uuid) != BT_STATUS_SUCCESS)
    return false;

  pending_calls_[uuid] = callback;

  return true;
}

void GattServerFactory::RegisterServerCallback(
    hal::BluetoothGattInterface* gatt_iface,
    int status, int server_id,
    const bt_uuid_t& app_uuid) {
  UUID uuid(app_uuid);

  VLOG(1) << __func__ << " - UUID: " << uuid.ToString();
  lock_guard<mutex> lock(pending_calls_lock_);

  auto iter = pending_calls_.find(uuid);
  if (iter == pending_calls_.end()) {
    VLOG(1) << "Ignoring callback for unknown app_id: " << uuid.ToString();
    return;
  }

  // No need to construct a server if the call wasn't successful.
  std::unique_ptr<GattServer> server;
  BLEStatus result = BLE_STATUS_FAILURE;
  if (status == BT_STATUS_SUCCESS) {
    server.reset(new GattServer(uuid, server_id));

    gatt_iface->AddServerObserver(server.get());

    result = BLE_STATUS_SUCCESS;
  }

  // Notify the result via the result callback.
  iter->second(result, uuid, std::move(server));

  pending_calls_.erase(iter);
}

}  // namespace bluetooth
