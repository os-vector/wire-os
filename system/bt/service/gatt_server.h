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

#include <deque>
#include <functional>
#include <mutex>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <base/macros.h>

#include "service/bluetooth_instance.h"
#include "service/common/bluetooth/gatt_identifier.h"
#include "service/common/bluetooth/uuid.h"
#include "service/hal/bluetooth_gatt_interface.h"

namespace bluetooth {

// A GattServer instance represents an application's handle to perform GATT
// server-role operations. Instances cannot be created directly and should be
// obtained through the factory.
class GattServer : public BluetoothInstance,
                   private hal::BluetoothGattInterface::ServerObserver {
 public:
  // Delegate interface is used to handle incoming requests and confirmations
  // for a GATT service.
  class Delegate {
   public:
    Delegate() = default;
    virtual ~Delegate() = default;

    // Called when there is an incoming read request for the characteristic with
    // ID |characteristic_id| from a remote device with address
    // |device_address|. |request_id| can be used to respond to this request by
    // calling SendResponse below.
    virtual void OnCharacteristicReadRequest(
        GattServer* gatt_server,
        const std::string& device_address,
        int request_id, int offset, bool is_long,
        const bluetooth::GattIdentifier& characteristic_id) = 0;

    // Called when there is an incoming read request for the descriptor with
    // ID |descriptor_id| from a remote device with address |device_address|.
    // |request_id| can be used to respond to this request by
    // calling SendResponse below.
    virtual void OnDescriptorReadRequest(
        GattServer* gatt_server,
        const std::string& device_address,
        int request_id, int offset, bool is_long,
        const bluetooth::GattIdentifier& descriptor_id) = 0;

    // Called when there is an incoming write request for the characteristic
    // with ID |characteristic_id| from a remote device with address
    // |device_address|. |request_id| can be used to respond to this request by
    // calling SendResponse, if the |need_response| parameter is true. Otherwise
    // this is a "Write Without Reponse" procedure and SendResponse will fail.
    // If |is_prepare_write| is true, then the write should not be committed
    // immediately as this is a "Prepared Write Request". Instead, the Delegate
    // should hold on to the value and either discard it or complete the write
    // when it receives the OnExecuteWriteRequest event.
    virtual void OnCharacteristicWriteRequest(
        GattServer* gatt_server,
        const std::string& device_address,
        int request_id, int offset, bool is_prepare_write, bool need_response,
        const std::vector<uint8_t>& value,
        const bluetooth::GattIdentifier& characteristic_id) = 0;

    // Called when there is an incoming write request for the descriptor
    // with ID |descriptor_id| from a remote device with address
    // |device_address|. |request_id| can be used to respond to this request by
    // calling SendResponse, if the |need_response| parameter is true. Otherwise
    // this is a "Write Without Response" procedure and SendResponse will fail.
    // If |is_prepare_write| is true, then the write should not be committed
    // immediately as this is a "Prepared Write Request". Instead, the Delegate
    // should hold on to the value and either discard it or complete the write
    // when it receives the OnExecuteWriteRequest event.
    virtual void OnDescriptorWriteRequest(
        GattServer* gatt_server,
        const std::string& device_address,
        int request_id, int offset, bool is_prepare_write, bool need_response,
        const std::vector<uint8_t>& value,
        const bluetooth::GattIdentifier& descriptor_id) = 0;

    // Called when there is an incoming "Execute Write Request". If |is_execute|
    // is true, then the Delegate should commit all previously prepared writes.
    // Otherwise, all prepared writes should be aborted. The Delegate should
    // call "SendResponse" to complete the procedure.
    virtual void OnExecuteWriteRequest(
        GattServer* gatt_server,
        const std::string& device_address,
        int request_id, bool is_execute) = 0;

   private:
    DISALLOW_COPY_AND_ASSIGN(Delegate);
  };

  // The desctructor automatically unregisters this instance from the stack.
  ~GattServer() override;

  // Assigns a delegate to this instance. |delegate| must out-live this
  // GattServer instance.
  void SetDelegate(Delegate* delegate);

  // BluetoothClientInstace overrides:
  const UUID& GetAppIdentifier() const override;
  int GetInstanceId() const override;

  // Callback type used to report the status of an asynchronous GATT server
  // operation.
  using ResultCallback =
      std::function<void(BLEStatus status, const GattIdentifier& id)>;
  using GattCallback = std::function<void(GATTError error)>;

  // Starts a new GATT service declaration for the service with the given
  // parameters. In the case of an error, for example If a service declaration
  // is already in progress, then this method returns a NULL pointer. Otherwise,
  // this returns an identifier that uniquely identifies the added service.
  //
  // TODO(armansito): In the framework code, the "min_handles" parameter is
  // annotated to be used for "conformance testing only". I don't fully see the
  // point of this and suggest getting rid of this parameter entirely. For now
  // this code doesn't declare or use it.
  std::unique_ptr<GattIdentifier> BeginServiceDeclaration(
      const UUID& uuid, bool is_primary);

  // Inserts a new characteristic definition into a previously begun service
  // declaration. Returns the assigned identifier for the characteristic, or
  // nullptr if a service declaration wasn't begun or a call to
  // EndServiceDeclaration is still in progress.
  std::unique_ptr<GattIdentifier> AddCharacteristic(
      const UUID& uuid, int properties, int permissions);

  // Inserts a new descriptor definition into a previous begun service
  // declaration. Returns the assigned identifier for the descriptor, or
  // nullptr if a service declaration wasn't begun, a call to
  // EndServiceDeclaration is still in progress, or a characteristic definition
  // doesn't properly precede this definition.
  std::unique_ptr<GattIdentifier> AddDescriptor(
      const UUID& uuid, int permissions);

  // Ends a previously started service declaration. This method immediately
  // returns false if a service declaration hasn't been started. Otherwise,
  // |callback| will be called asynchronously with the result of the operation.
  //
  // TODO(armansito): It is unclear to me what it means for this function to
  // fail. What is the state that we're in? Is the service declaration over so
  // we can add other services to this server instance? Do we need to clean up
  // all the entries or does the upper-layer need to remove the service? Or are
  // we in a stuck-state where the service declaration hasn't ended?
  bool EndServiceDeclaration(const ResultCallback& callback);

  // Sends a response for a pending notification. |request_id| and
  // |device_address| should match those that were received through one of the
  // Delegate callbacks. |value| and |offset| are used for read requests and
  // prepare write requests and should match the value of the attribute. Returns
  // false if the pending request could not be resolved using the given
  // parameters or if the call to the underlying stack fails.
  bool SendResponse(const std::string& device_address, int request_id,
                    GATTError error, int offset,
                    const std::vector<uint8_t>& value);

  // Sends an ATT Handle-Value Notification to the device with BD_ADDR
  // |device_address| for the characteristic with ID |characteristic_id| and
  // value |value|. If |confirm| is true, then an ATT Handle-Value Indication
  // will be sent instead, which requires the remote to confirm receipt. Returns
  // false if there was an immediate error in initiating the notification
  // procedure. Otherwise, returns true and reports the asynchronous result of
  // the operation in |callback|.
  //
  // If |confirm| is true, then |callback| will be run when the remote device
  // sends a ATT Handle-Value Confirmation packet. Otherwise, it will be run as
  // soon as the notification has been sent out.
  bool SendNotification(const std::string& device_address,
                        const GattIdentifier& characteristic_id,
                        bool confirm, const std::vector<uint8_t>& value,
                        const GattCallback& callback);

 private:
  friend class GattServerFactory;

  // Internal representation of an attribute entry as part of a service
  // declaration.
  struct AttributeEntry {
    AttributeEntry(const GattIdentifier& id,
                   int char_properties,
                   int permissions)
        : id(id), char_properties(char_properties), permissions(permissions) {}

    GattIdentifier id;
    int char_properties;
    int permissions;
  };

  // Internal representation of a GATT service declaration before it has been
  // sent to the stack.
  struct ServiceDeclaration {
    ServiceDeclaration() : num_handles(0), service_handle(-1) {}

    size_t num_handles;
    GattIdentifier service_id;
    int service_handle;
    std::deque<AttributeEntry> attributes;
  };

  // Used for the internal remote connection tracking. Keeps track of the
  // request ID and the device address for the connection. If |request_id| is -1
  // then no ATT read/write request is currently pending.
  struct Connection {
    Connection(int conn_id, const bt_bdaddr_t& bdaddr)
        : conn_id(conn_id), bdaddr(bdaddr) {}
    Connection() : conn_id(-1) {
      memset(&bdaddr, 0, sizeof(bdaddr));
    }

    int conn_id;
    std::unordered_map<int, int> request_id_to_handle;
    bt_bdaddr_t bdaddr;
  };

  // Used to keep track of a pending Handle-Value indication.
  struct PendingIndication {
    PendingIndication(const GattCallback& callback)
        : has_success(false), callback(callback) {}

    bool has_success;
    GattCallback callback;
  };

  // Constructor shouldn't be called directly as instances are meant to be
  // obtained from the factory.
  GattServer(const UUID& uuid, int server_id);

  // Returns a GattIdentifier for the attribute with the given UUID within the
  // current pending service declaration.
  std::unique_ptr<GattIdentifier> GetIdForService(const UUID& uuid,
                                                  bool is_primary);
  std::unique_ptr<GattIdentifier> GetIdForCharacteristic(const UUID& uuid);
  std::unique_ptr<GattIdentifier> GetIdForDescriptor(const UUID& uuid);

  // hal::BluetoothGattInterface::ServerObserver overrides:
  void ConnectionCallback(
      hal::BluetoothGattInterface* gatt_iface,
      int conn_id, int server_id,
      int connected,
      const bt_bdaddr_t& bda) override;
  void ServiceAddedCallback(
      hal::BluetoothGattInterface* gatt_iface,
      int status, int server_id,
      const btgatt_srvc_id_t& srvc_id,
      int service_handle) override;
  void CharacteristicAddedCallback(
      hal::BluetoothGattInterface* gatt_iface,
      int status, int server_id,
      const bt_uuid_t& uuid,
      int service_handle,
      int char_handle) override;
  void DescriptorAddedCallback(
      hal::BluetoothGattInterface* gatt_iface,
      int status, int server_id,
      const bt_uuid_t& uuid,
      int service_handle,
      int desc_handle) override;
  void ServiceStartedCallback(
      hal::BluetoothGattInterface* gatt_iface,
      int status, int server_id,
      int service_handle) override;
  void ServiceStoppedCallback(
      hal::BluetoothGattInterface* gatt_iface,
      int status, int server_id,
      int service_handle) override;
  void RequestReadCallback(
      hal::BluetoothGattInterface* gatt_iface,
      int conn_id, int trans_id,
      const bt_bdaddr_t& bda,
      int attribute_handle, int offset,
      bool is_long) override;
  void RequestWriteCallback(
      hal::BluetoothGattInterface* gatt_iface,
      int conn_id, int trans_id,
      const bt_bdaddr_t& bda,
      int attr_handle, int offset, int length,
      bool need_rsp, bool is_prep, uint8_t* value) override;
  void RequestExecWriteCallback(
      hal::BluetoothGattInterface* gatt_iface,
      int conn_id, int trans_id,
      const bt_bdaddr_t& bda, int exec_write) override;
  void IndicationSentCallback(
      hal::BluetoothGattInterface* gatt_iface,
      int conn_id, int status) override;

  // Helper function that notifies and clears the pending callback.
  void NotifyEndCallbackAndClearData(BLEStatus status,
                                     const GattIdentifier& id);
  void CleanUpPendingData();

  // Handles the next attribute entry in the pending service declaration.
  void HandleNextEntry(hal::BluetoothGattInterface* gatt_iface);

  // Helper method that returns a pointer to an internal Connection instance
  // that matches the given parameters.
  std::shared_ptr<Connection> GetConnection(int conn_id, const bt_bdaddr_t& bda,
                                            int request_id);

  // Pops the next GATT ID or entry from the pending service declaration's
  // attribute list.
  std::unique_ptr<AttributeEntry> PopNextEntry();
  std::unique_ptr<GattIdentifier> PopNextId();

  // See getters for documentation.
  UUID app_identifier_;
  int server_id_;

  // Mutex that synchronizes access to the entries below.
  std::mutex mutex_;
  std::unique_ptr<GattIdentifier> pending_id_;
  std::unique_ptr<ServiceDeclaration> pending_decl_;
  ResultCallback pending_end_decl_cb_;
  std::unordered_map<GattIdentifier, int> pending_handle_map_;

  // Mapping of handles and GATT identifiers for started services.
  std::unordered_map<GattIdentifier, int> id_to_handle_map_;
  std::unordered_map<int, GattIdentifier> handle_to_id_map_;

  // GATT connection mappings from stack-provided "conn_id" IDs and remote
  // device addresses to Connection structures. The conn_id map is one-to-one
  // while the conn_addr map is one to many, as a remote device may support
  // multiple transports (BR/EDR & LE) and use the same device address for both.
  std::unordered_map<int, std::shared_ptr<Connection>> conn_id_map_;
  std::unordered_map<std::string, std::vector<std::shared_ptr<Connection>>>
      conn_addr_map_;

  // Connections for which a Handle-Value indication is pending. Since there can
  // be multiple indications to the same device (in the case of a dual-mode
  // device with simulatenous BR/EDR & LE GATT connections), we also keep track
  // of whether there has been at least one successful confirmation.
  std::unordered_map<int, std::shared_ptr<PendingIndication>>
      pending_indications_;

  // Raw handle to the Delegate, which must outlive this GattServer instance.
  Delegate* delegate_;

  DISALLOW_COPY_AND_ASSIGN(GattServer);
};

// GattServerFactory is used to register and obtain a per-application GattServer
// instance. Users should call RegisterClient to obtain their own unique
// GattServer instance that has been registered with the Bluetooth stack.
class GattServerFactory : public BluetoothInstanceFactory,
                          private hal::BluetoothGattInterface::ServerObserver {
 public:
  // Don't construct/destruct directly except in tests. Instead, obtain a handle
  // from an Adapter instance.
  GattServerFactory();
  ~GattServerFactory() override;

  // BluetoothInstanceFactory override:
  bool RegisterInstance(const UUID& uuid,
                        const RegisterCallback& callback) override;

 private:
  // hal::BluetoothGattInterface::ServerObserver override:
  void RegisterServerCallback(
      hal::BluetoothGattInterface* gatt_iface,
      int status, int server_id,
      const bt_uuid_t& app_uuid) override;

  // Map of pending calls to register.
  std::mutex pending_calls_lock_;
  std::unordered_map<UUID, RegisterCallback> pending_calls_;

  DISALLOW_COPY_AND_ASSIGN(GattServerFactory);
};

}  // namespace bluetooth
