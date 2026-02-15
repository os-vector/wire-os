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

#include "service/example/heart_rate/heart_rate_server.h"

#include <base/bind.h>
#include <base/location.h>
#include <base/logging.h>
#include <base/rand_util.h>

#include <bluetooth/low_energy_constants.h>

#include "service/example/heart_rate/constants.h"

namespace heart_rate {

class CLIBluetoothLowEnergyCallback
    : public ipc::binder::BnBluetoothLowEnergyCallback {
 public:
  CLIBluetoothLowEnergyCallback(android::sp<ipc::binder::IBluetooth> bt)
      : bt_(bt) {}

  // IBluetoothLowEnergyCallback overrides:
  void OnConnectionState(int status, int client_id, const char* address,
                         bool connected) override {}
  void OnMtuChanged(int status, const char *address, int mtu) override {}

  void OnServicesDiscovered(int status, const char *address) override {}

  void OnGattDbUpdated(const char *address, btgatt_db_element_t* db, int size) override {}

  void OnCharacteristicRead(const char *address, int status, btgatt_read_params_t* data) override {}

  void OnCharacteristicWrite(const char *address, int status, uint16_t handle) override {}

  void OnDescriptorWrite(const char *address, int status, uint16_t handle) override {}

  void OnCharacteristicNotificationRegistration(const char* address, int registered,
                                                int status, uint16_t handle) override {}

  void OnCharacteristicChanged(const char* address, btgatt_notify_params_t* notification) override {}

  void OnScanResult(const bluetooth::ScanResult& scan_result) override {}

  void OnClientRegistered(int status, int client_id){
    if (status != bluetooth::BLE_STATUS_SUCCESS) {
      LOG(ERROR) << "Failed to register BLE client, will not start advertising";
      return;
    }

    LOG(INFO) << "Registered BLE client with ID: " << client_id;

    /* Advertising data: 16-bit Service UUID: Heart Rate Service */
    std::vector<uint8_t> data{0x03, 0x03, 0x0D, 0x18};
    base::TimeDelta timeout;

    bluetooth::AdvertiseSettings settings(
        bluetooth::AdvertiseSettings::MODE_LOW_POWER,
        timeout,
        bluetooth::AdvertiseSettings::TX_POWER_LEVEL_MEDIUM,
        true);

    bluetooth::AdvertiseData adv_data(data);
    adv_data.set_include_device_name(true);
    adv_data.set_include_tx_power_level(true);

    bluetooth::AdvertiseData scan_rsp;

    bt_->GetLowEnergyInterface()->
        StartMultiAdvertising(client_id, adv_data, scan_rsp, settings);
  }

  void OnMultiAdvertiseCallback(int status, bool is_start,
      const bluetooth::AdvertiseSettings& /* settings */) {
    LOG(INFO) << "Advertising" << (is_start?" started":" stopped");
  };

 private:
  android::sp<ipc::binder::IBluetooth> bt_;
  DISALLOW_COPY_AND_ASSIGN(CLIBluetoothLowEnergyCallback);
};


HeartRateServer::HeartRateServer(
    android::sp<ipc::binder::IBluetooth> bluetooth,
    scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
    bool advertise)
    : simulation_started_(false),
      bluetooth_(bluetooth),
      server_if_(-1),
      hr_notification_count_(0),
      energy_expended_(0),
      advertise_(advertise),
      main_task_runner_(main_task_runner),
      weak_ptr_factory_(this) {
  CHECK(bluetooth_.get());
}

HeartRateServer::~HeartRateServer() {
  std::lock_guard<std::mutex> lock(mutex_);
  if (!gatt_.get() || server_if_ == -1)
    return;

  if (!android::IInterface::asBinder(gatt_.get())->isBinderAlive())
    return;

  // Manually unregister ourselves from the daemon. It's good practice to do
  // this, even though the daemon will automatically unregister us if this
  // process exits.
  gatt_->UnregisterServer(server_if_);
}

bool HeartRateServer::Run(const RunCallback& callback) {
  std::lock_guard<std::mutex> lock(mutex_);

  if (pending_run_cb_) {
    LOG(ERROR) << "Already started";
    return false;
  }

  // Grab the IBluetoothGattServer binder from the Bluetooth daemon.
  gatt_ = bluetooth_->GetGattServerInterface();
  if (!gatt_.get()) {
    LOG(ERROR) << "Failed to obtain handle to IBluetoothGattServer interface";
    return false;
  }

  // Register this instance as a GATT server. If this call succeeds, we will
  // asynchronously receive a server ID via the OnServerRegistered callback.
  if (!gatt_->RegisterServer(this)) {
    LOG(ERROR) << "Failed to register with the server interface";
    return false;
  }

  pending_run_cb_ = callback;

  return true;
}

void HeartRateServer::ScheduleNextMeasurement() {
  main_task_runner_->PostDelayedTask(
      FROM_HERE,
      base::Bind(&HeartRateServer::SendHeartRateMeasurement,
                 weak_ptr_factory_.GetWeakPtr()),
      base::TimeDelta::FromSeconds(1));
}

void HeartRateServer::SendHeartRateMeasurement() {
  std::lock_guard<std::mutex> lock(mutex_);

  // Send a notification or indication to all enabled devices.
  bool found = false;
  for (const auto& iter : device_ccc_map_) {
    uint8_t ccc_val = iter.second;

    if (!ccc_val)
      continue;

    found = true;

    // Don't send a notification if one is already pending for this device.
    if (pending_notification_map_[iter.first])
      continue;

    std::vector<uint8_t> value;
    BuildHeartRateMeasurementValue(&value);

    if (gatt_->SendNotification(server_if_, iter.first, hr_measurement_id_,
                                false, value))
      pending_notification_map_[iter.first] = true;
  }

  // Still enabled!
  if (found) {
    ScheduleNextMeasurement();
    return;
  }

  // All clients disabled notifications.
  simulation_started_ = false;

  // TODO(armansito): We should keep track of closed connections here so that we
  // don't send notifications to uninterested clients.
}

void HeartRateServer::BuildHeartRateMeasurementValue(
    std::vector<uint8_t>* out_value) {
  CHECK(out_value);  // Assert that |out_value| is not nullptr.

  // Default flags field. Here is what we put in there:
  //   Bit 0: 0 - 8-bit Heart Rate value
  //   Bits 1 & 2: 11 - Sensor contact feature supported and contact detected.
  uint8_t flags = kHRValueFormat8Bit | kHRSensorContactDetected;

  // Our demo's heart rate. Pick a value between 90 and 130.
  uint8_t heart_rate = base::RandInt(90, 130);

  // On every tenth beat we include the Energy Expended value.
  bool include_ee = false;
  if (!(hr_notification_count_ % 10)) {
    include_ee = true;
    flags |= kHREnergyExpendedPresent;
  }

  hr_notification_count_++;
  energy_expended_ = std::min(UINT16_MAX, (int)energy_expended_ + 1);

  // Add all the value bytes.
  out_value->push_back(flags);
  out_value->push_back(heart_rate);
  if (include_ee) {
    out_value->push_back(energy_expended_);
    out_value->push_back(energy_expended_ >> 8);
  }
}

void HeartRateServer::OnServerRegistered(int status, int server_if) {
  std::lock_guard<std::mutex> lock(mutex_);

  if (status != bluetooth::BLE_STATUS_SUCCESS) {
    LOG(ERROR) << "Failed to register GATT server";
    pending_run_cb_(false);
    return;
  }

  // Registration succeeded. Store our ID, as we need it for GATT server
  // operations.
  server_if_ = server_if;

  LOG(INFO) << "Heart Rate server registered - server_if: " << server_if_;
  LOG(INFO) << "Populating attributes";

  // Start service declaration.
  std::unique_ptr<bluetooth::GattIdentifier> gatt_id;
  if (!gatt_->BeginServiceDeclaration(server_if_, true,
                                      kHRServiceUUID,
                                      &gatt_id)) {
    LOG(ERROR) << "Failed to begin service declaration";
    pending_run_cb_(false);
    return;
  }

  hr_service_id_ = *gatt_id;

  // Add Heart Rate Measurement characteristic.
  if (!gatt_->AddCharacteristic(
      server_if_, kHRMeasurementUUID,
      bluetooth::kCharacteristicPropertyNotify,
      0, &gatt_id)) {
    LOG(ERROR) << "Failed to add heart rate measurement characteristic";
    pending_run_cb_(false);
    return;
  }

  hr_measurement_id_ = *gatt_id;

  // Add Client Characteristic Configuration descriptor for the Heart Rate
  // Measurement characteristic.
  if (!gatt_->AddDescriptor(
      server_if_, kCCCDescriptorUUID,
      bluetooth::kAttributePermissionRead|bluetooth::kAttributePermissionWrite,
      &gatt_id)) {
    LOG(ERROR) << "Failed to add CCC descriptor";
    pending_run_cb_(false);
    return;
  }

  hr_measurement_cccd_id_ = *gatt_id;

  // Add Body Sensor Location characteristic.
  if (!gatt_->AddCharacteristic(
      server_if_, kBodySensorLocationUUID,
      bluetooth::kCharacteristicPropertyRead,
      bluetooth::kAttributePermissionRead,
      &gatt_id)) {
    LOG(ERROR) << "Failed to add body sensor location characteristic";
    pending_run_cb_(false);
    return;
  }

  body_sensor_loc_id_ = *gatt_id;

  // Add Heart Rate Control Point characteristic.
  if (!gatt_->AddCharacteristic(
      server_if_, kHRControlPointUUID,
      bluetooth::kCharacteristicPropertyWrite,
      bluetooth::kAttributePermissionWrite,
      &gatt_id)) {
    LOG(ERROR) << "Failed to add heart rate control point characteristic";
    pending_run_cb_(false);
    return;
  }

  hr_control_point_id_ = *gatt_id;

  // End service declaration. We will be notified whether or not this succeeded
  // via the OnServiceAdded callback.
  if (!gatt_->EndServiceDeclaration(server_if_)) {
    LOG(ERROR) << "Failed to end service declaration";
    pending_run_cb_(false);
    return;
  }

  LOG(INFO) << "Initiated EndServiceDeclaration request";
}

void HeartRateServer::OnServiceAdded(
    int status,
    const bluetooth::GattIdentifier& service_id) {
  std::lock_guard<std::mutex> lock(mutex_);

  if (status != bluetooth::BLE_STATUS_SUCCESS) {
    LOG(ERROR) << "Failed to add Heart Rate service";
    pending_run_cb_(false);
    return;
  }

  if (service_id != hr_service_id_) {
    LOG(ERROR) << "Received callback for the wrong service ID";
    pending_run_cb_(false);
    return;
  }

  // EndServiceDeclaration succeeded! Our Heart Rate service is now discoverable
  // over GATT connections.

  LOG(INFO) << "Heart Rate service added";
  pending_run_cb_(true);

  if (advertise_) {
    auto ble = bluetooth_->GetLowEnergyInterface();
    if (!ble.get()) {
      LOG(ERROR) << "Failed to obtain handle to IBluetoothLowEnergy interface";
      return;
    }
    ble->RegisterClient(new CLIBluetoothLowEnergyCallback(bluetooth_));
  }

}

void HeartRateServer::OnCharacteristicReadRequest(
    const std::string& device_address,
    int request_id, int offset, bool /* is_long */,
    const bluetooth::GattIdentifier& characteristic_id) {
  std::lock_guard<std::mutex> lock(mutex_);

  // This is where we handle an incoming characteristic read. Only the body
  // sensor location characteristic is readable.
  CHECK(characteristic_id == body_sensor_loc_id_);

  std::vector<uint8_t> value;
  bluetooth::GATTError error = bluetooth::GATT_ERROR_NONE;
  if (offset > 1)
    error = bluetooth::GATT_ERROR_INVALID_OFFSET;
  else if (offset == 0)
    value.push_back(kHRBodyLocationFoot);

  gatt_->SendResponse(server_if_, device_address, request_id, error,
                      offset, value);
}

void HeartRateServer::OnDescriptorReadRequest(
    const std::string& device_address,
    int request_id, int offset, bool /* is_long */,
    const bluetooth::GattIdentifier& descriptor_id) {
  std::lock_guard<std::mutex> lock(mutex_);

  // This is where we handle an incoming characteristic descriptor read. There
  // is only one descriptor.
  if (descriptor_id != hr_measurement_cccd_id_) {
    std::vector<uint8_t> value;
    gatt_->SendResponse(server_if_, device_address, request_id,
                        bluetooth::GATT_ERROR_ATTRIBUTE_NOT_FOUND,
                        offset, value);
    return;
  }

  // 16-bit value encoded as little-endian.
  const uint8_t value_bytes[] = { device_ccc_map_[device_address], 0x00 };

  std::vector<uint8_t> value;
  bluetooth::GATTError error = bluetooth::GATT_ERROR_NONE;
  if (offset > 2)
    error = bluetooth::GATT_ERROR_INVALID_OFFSET;
  else
    value.insert(value.begin(), value_bytes + offset, value_bytes + 2 - offset);

  gatt_->SendResponse(server_if_, device_address, request_id, error,
                      offset, value);
}

void HeartRateServer::OnCharacteristicWriteRequest(
    const std::string& device_address,
    int request_id, int offset, bool is_prepare_write, bool need_response,
    const std::vector<uint8_t>& value,
    const bluetooth::GattIdentifier& characteristic_id) {
  std::lock_guard<std::mutex> lock(mutex_);

  std::vector<uint8_t> dummy;

  // This is where we handle an incoming characteristic write. The Heart Rate
  // service doesn't really support prepared writes, so we just reject them to
  // keep things simple.
  if (is_prepare_write) {
    gatt_->SendResponse(server_if_, device_address, request_id,
                        bluetooth::GATT_ERROR_REQUEST_NOT_SUPPORTED,
                        offset, dummy);
    return;
  }

  // Heart Rate Control point is the only writable characteristic.
  CHECK(characteristic_id == hr_control_point_id_);

  // Writes to the Heart Rate Control Point characteristic must contain a single
  // byte with the value 0x01.
  if (value.size() != 1 || value[0] != 0x01) {
    gatt_->SendResponse(server_if_, device_address, request_id,
                        bluetooth::GATT_ERROR_OUT_OF_RANGE,
                        offset, dummy);
    return;
  }

  LOG(INFO) << "Heart Rate Control Point written; Enery Expended reset!";
  energy_expended_ = 0;

  if (!need_response)
    return;

  gatt_->SendResponse(server_if_, device_address, request_id,
                      bluetooth::GATT_ERROR_NONE, offset, dummy);
}

void HeartRateServer::OnDescriptorWriteRequest(
    const std::string& device_address,
    int request_id, int offset, bool is_prepare_write, bool need_response,
    const std::vector<uint8_t>& value,
    const bluetooth::GattIdentifier& descriptor_id) {
  std::lock_guard<std::mutex> lock(mutex_);

  std::vector<uint8_t> dummy;

  // This is where we handle an incoming characteristic write. The Heart Rate
  // service doesn't really support prepared writes, so we just reject them to
  // keep things simple.
  if (is_prepare_write) {
    gatt_->SendResponse(server_if_, device_address, request_id,
                        bluetooth::GATT_ERROR_REQUEST_NOT_SUPPORTED,
                        offset, dummy);
    return;
  }

  // CCC is the only descriptor we have.
  CHECK(descriptor_id == hr_measurement_cccd_id_);

  // CCC must contain 2 bytes for a 16-bit value in little-endian. The only
  // allowed values here are 0x0000 and 0x0001.
  if (value.size() != 2 || value[1] != 0x00 || value[0] > 0x01) {
    gatt_->SendResponse(server_if_, device_address, request_id,
                        bluetooth::GATT_ERROR_CCCD_IMPROPERLY_CONFIGURED,
                        offset, dummy);
    return;
  }

  device_ccc_map_[device_address] = value[0];

  LOG(INFO) << "Heart Rate Measurement CCC written - device: "
            << device_address << " value: " << (int)value[0];

  // Start the simulation.
  if (!simulation_started_ && value[0]) {
    simulation_started_ = true;
    ScheduleNextMeasurement();
  }

  if (!need_response)
    return;

  gatt_->SendResponse(server_if_, device_address, request_id,
                      bluetooth::GATT_ERROR_NONE, offset, dummy);
}

void HeartRateServer::OnExecuteWriteRequest(
    const std::string& device_address,
    int request_id,
    bool /* is_execute */) {
  // We don't support Prepared Writes so, simply return Not Supported error.
  std::vector<uint8_t> dummy;
  gatt_->SendResponse(server_if_, device_address, request_id,
                      bluetooth::GATT_ERROR_REQUEST_NOT_SUPPORTED, 0, dummy);
}

void HeartRateServer::OnNotificationSent(
    const std::string& device_address, int status) {
  LOG(INFO) << "Notification was sent - device: " << device_address
            << " status: " << status;
  std::lock_guard<std::mutex> lock(mutex_);
  pending_notification_map_[device_address] = false;
}

}  // namespace heart_rate
