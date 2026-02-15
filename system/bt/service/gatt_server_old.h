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

#include <array>
#include <memory>
#include <unordered_map>
#include <vector>

#include "hardware/bluetooth.h"
#include "hardware/bt_gatt.h"
#include "service/common/bluetooth/uuid.h"

namespace bluetooth {
namespace gatt {

// Attribute permission values
const int kPermissionRead = 0x1;
const int kPermissionReadEncrypted = 0x2;
const int kPermissionReadEncryptedMitm = 0x4;
const int kPermissionWrite = 0x10;
const int kPermissionWriteEnecrypted = 0x20;
const int KPermissionWriteEncryptedMitm = 0x40;
const int kPermissionWriteSigned = 0x80;
const int kPermissionWriteSignedMitm = 0x100;

// GATT characteristic properties bit-field values
const int kPropertyBroadcast = 0x1;
const int kPropertyRead = 0x2;
const int kPropertyWriteNoResponse = 0x4;
const int kPropertyWrite = 0x8;
const int kPropertyNotify = 0x10;
const int kPropertyIndicate = 0x20;
const int kPropertySignedWrite = 0x40;
const int kPropertyExtendedProps = 0x80;

// A mapping from string bluetooth addresses to RSSI measurements.
typedef std::unordered_map<std::string, int> ScanResults;

// TODO(armansito): This should be a private internal class though I don't see
// why we even need this class. Instead it should probably be merged into
// Server.
struct ServerInternals;

// Server is threadsafe and internally locked.
// Asynchronous IO is identified via a gatt_pipe FD,
// and synchronously read with 'GetCharacteristicValue'
//
// ****DEPRECATED****
//
// TODO(armansito): This class has been deprecated and is being replaced by
// bluetooth::GattServer. We will remove this entirely once the new code is
// ready.
class Server {
 public:
  Server();
  ~Server();

  // Register GATT interface, initialize internal state,
  // and open a pipe for characteristic write notification.
  bool Initialize(const UUID& service_id, int* gatt_pipe);

  // Control the content of service advertisement.
  bool SetAdvertisement(const std::vector<UUID>& ids,
                        const std::vector<uint8_t>& service_data,
                        const std::vector<uint8_t>& manufacturer_data,
                        bool transmit_name);

  // Control the content of service scan response.
  bool SetScanResponse(const std::vector<UUID>& ids,
                       const std::vector<uint8_t>& service_data,
                       const std::vector<uint8_t>& manufacturer_data,
                       bool transmit_name);

  // Add an ordinary characteristic for reading and/or writing.
  bool AddCharacteristic(const UUID &id, int properties, int permissions);

  // Add a special 'blob' characteristic with a corresponding control
  // attribute to manipulate which part of the blob the attribute represents.
  bool AddBlob(const UUID &id, const UUID &control_id, int properties,
               int permissions);

  // Put a new value into a characeteristic.
  // It will be read from a client starting at the next 0-offset read.
  bool SetCharacteristicValue(const UUID &id, const std::vector<uint8_t> &value);

  // Get the current value of a characteristic.
  bool GetCharacteristicValue(const UUID &id, std::vector<uint8_t> *value);

  // Start this service. Activate advertisements, allow connections.
  // Characteristics should all be created before this.
  bool Start();

  // Cease advertisements and disallow connections.
  bool Stop();

  // Enable LE scan. Scan results will be cached internally.
  bool ScanEnable();

  // Disable LE scan.
  bool ScanDisable();

  // Copy out the cached scan results.
  bool GetScanResults(ScanResults *results);

 private:
  // Internal data.
  std::unique_ptr<ServerInternals> internal_;
};

}  // namespace gatt
}  // namespace bluetooth
