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

#include <bluetooth/uuid.h>

// This header defines constants specific to the GATT Heart Rate Service.

namespace heart_rate {

// HR Measurement characteristic value flags
const uint8_t kHRValueFormat8Bit = (0 << 0);
const uint8_t kHRValueFormat16Bit = (1 << 0);
const uint16_t kHRSensorContactDetected = (3 << 1);
const uint8_t kHREnergyExpendedPresent = (1 << 3);

const bluetooth::UUID kCCCDescriptorUUID("2902");
const bluetooth::UUID kHRServiceUUID("180D");
const bluetooth::UUID kHRMeasurementUUID("2A37");
const bluetooth::UUID kBodySensorLocationUUID("2A38");
const bluetooth::UUID kHRControlPointUUID("2A39");

const uint8_t kHRBodyLocationOther = 0;
const uint8_t kHRBodyLocationChest = 1;
const uint8_t kHRBodyLocationWrist = 2;
const uint8_t kHRBodyLocationFinger = 3;
const uint8_t kHRBodyLocationHand = 4;
const uint8_t kHRBodyLocationEarLobe = 5;
const uint8_t kHRBodyLocationFoot = 6;

}  // namespace heart_rate
