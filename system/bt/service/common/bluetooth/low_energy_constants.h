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

#include <stdint.h>

namespace bluetooth {

// Defined here are various status codes that can be returned from the stack for
// BLE operations.
enum BLEStatus {
  BLE_STATUS_SUCCESS = 0,
  BLE_STATUS_ADV_ERROR_DATA_TOO_LARGE = 1,
  BLE_STATUS_ADV_ERROR_TOO_MANY_ADVERTISERS = 2,
  BLE_STATUS_ADV_ERROR_ALREADY_STARTED = 3,
  BLE_STATUS_ADV_ERROR_FEATURE_UNSUPPORTED = 5,
  BLE_STATUS_FAILURE = 0x101,
};

enum GATTError {
  GATT_ERROR_NONE = 0,
  GATT_ERROR_INVALID_HANDLE = 0x01,
  GATT_ERROR_READ_NOT_PERMITTED = 0x02,
  GATT_ERROR_WRITE_NOT_PERMITTED = 0x03,
  GATT_ERROR_INVALID_PDU = 0x04,
  GATT_ERROR_INSUFFICIENT_AUTHEN = 0x05,
  GATT_ERROR_REQUEST_NOT_SUPPORTED = 0x06,
  GATT_ERROR_INVALID_OFFSET = 0x07,
  GATT_ERROR_INSUFFICIENT_AUTHOR = 0x08,
  GATT_ERROR_PREP_QUEUE_FULL = 0x09,
  GATT_ERROR_ATTRIBUTE_NOT_FOUND = 0x0a,
  GATT_ERROR_ATTRIBUTE_NOT_LONG = 0x0b,
  GATT_ERROR_INSUFFICIENT_KEY_SIZE = 0x0c,
  GATT_ERROR_INVALID_ATTRIBUTE_LENGTH = 0x0d,
  GATT_ERROR_UNLIKELY = 0x0e,
  GATT_ERROR_INSUFFICIENT_ENCR = 0x0f,
  GATT_ERROR_UNSUPPORTED_GRP_TYPE = 0x10,
  GATT_ERROR_INSUFFICIENT_RESOURCES = 0x11,
  GATT_ERROR_CCCD_IMPROPERLY_CONFIGURED = 0xFD,
  GATT_ERROR_PROCEDURE_IN_PROGRESS = 0xFE,
  GATT_ERROR_OUT_OF_RANGE = 0xFF
};

enum Transport {
  TRANSPORT_AUTO = 0,
  TRANSPORT_BREDR = 1,
  TRANSPORT_LE = 2
};

// Android attribute permission values
const uint16_t kAttributePermissionNone = 0x0;
const uint16_t kAttributePermissionRead = 0x1;
const uint16_t kAttributePermissionReadEncrypted = 0x2;
const uint16_t kAttributePermissionReadEncryptedMITM = 0x4;
const uint16_t kAttributePermissionWrite = 0x10;
const uint16_t kAttributePermissionWriteEncrypted = 0x20;
const uint16_t kAttributePermissionWriteEncryptedMITM = 0x40;
const uint16_t kAttributePermissionWriteSigned = 0x80;
const uint16_t kAttributePermissionWriteSignedMITM = 0x100;

// GATT characteristic properties bit-field values (not including the
// characteristic extended properties).
const uint8_t kCharacteristicPropertyNone = 0x0;
const uint8_t kCharacteristicPropertyBroadcast = 0x1;
const uint8_t kCharacteristicPropertyRead = 0x2;
const uint8_t kCharacteristicPropertyWriteNoResponse = 0x4;
const uint8_t kCharacteristicPropertyWrite = 0x8;
const uint8_t kCharacteristicPropertyNotify = 0x10;
const uint8_t kCharacteristicPropertyIndicate = 0x20;
const uint8_t kCharacteristicPropertySignedWrite = 0x40;
const uint8_t kCharacteristicPropertyExtendedProps = 0x80;

// Advertising interval for different modes.
const int kAdvertisingIntervalHighMs = 1000;
const int kAdvertisingIntervalMediumMs = 250;
const int kAdvertisingIntervalLowMs = 100;

// Add some randomness to the advertising min/max interval so the controller can
// do some optimization.
// TODO(armansito): I took this directly from packages/apps/Bluetooth but based
// on code review comments this constant and the accompanying logic doesn't make
// sense. Let's remove this constant and figure out how to properly calculate
// the Max. Adv. Interval. (See http://b/24344075).
const int kAdvertisingIntervalDeltaUnit = 10;

// Advertising types (ADV_IND, ADV_SCAN_IND, etc.) that are exposed to
// applications.
const int kAdvertisingEventTypeConnectable = 0;
const int kAdvertisingEventTypeScannable = 2;
const int kAdvertisingEventTypeNonConnectable = 3;

// Advertising channels. These should be kept the same as those defined in the
// stack.
const int kAdvertisingChannel37 = (1 << 0);
const int kAdvertisingChannel38 = (1 << 1);
const int kAdvertisingChannel39 = (1 << 2);
const int kAdvertisingChannelAll =
    (kAdvertisingChannel37 | kAdvertisingChannel38 | kAdvertisingChannel39);

// Various Extended Inquiry Response fields types that are used for advertising
// data fields as defined in the Core Specification Supplement.
const uint8_t kEIRTypeFlags = 0x01;
const uint8_t kEIRTypeIncomplete16BitUUIDs = 0x02;
const uint8_t kEIRTypeComplete16BitUUIDs = 0x03;
const uint8_t kEIRTypeIncomplete32BitUUIDs = 0x04;
const uint8_t kEIRTypeComplete32BitUUIDs = 0x05;
const uint8_t kEIRTypeIncomplete128BitUUIDs = 0x06;
const uint8_t kEIRTypeComplete128BitUUIDs = 0x07;
const uint8_t kEIRTypeManufacturerSpecificData = 0xFF;

}  // namespace bluetooth
