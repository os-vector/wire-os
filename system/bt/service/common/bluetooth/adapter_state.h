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

#include <string>

namespace bluetooth {

// Possible Adapter states. The values for each enumration have been copied
// from frameworks/base/core/java/android/bluetooth/BluetoothAdapter.java.
// These values need to match their android.bluetooth.BluetoothAdapter
// counterparts for this to be compatible with the framework, hence we
// redeclare them here.
enum AdapterState {
  ADAPTER_STATE_DISCONNECTED = 0,
  ADAPTER_STATE_CONNECTING = 1,
  ADAPTER_STATE_CONNECTED = 2,
  ADAPTER_STATE_DISCONNECTING = 3,
  ADAPTER_STATE_OFF = 10,
  ADAPTER_STATE_TURNING_ON = 11,
  ADAPTER_STATE_ON = 12,
  ADAPTER_STATE_TURNING_OFF = 13,
  ADAPTER_STATE_INVALID = 0xFFFF
};

// Returns a string for the given Adapter state |state|.
std::string AdapterStateToString(AdapterState state);

}  // namespace bluetooth
