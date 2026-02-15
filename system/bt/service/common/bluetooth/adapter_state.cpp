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

#include "service/common/bluetooth/adapter_state.h"

namespace bluetooth {

std::string AdapterStateToString(AdapterState state) {
  switch (state) {
  case ADAPTER_STATE_DISCONNECTED:
    return "ADAPTER_STATE_DISCONNECTED";
  case ADAPTER_STATE_CONNECTING:
    return "ADAPTER_STATE_CONNECTING";
  case ADAPTER_STATE_CONNECTED:
    return "ADAPTER_STATE_CONNECTED";
  case ADAPTER_STATE_DISCONNECTING:
    return "ADAPTER_STATE_DISCONNECTING";
  case ADAPTER_STATE_OFF:
    return "ADAPTER_STATE_OFF";
  case ADAPTER_STATE_TURNING_ON:
    return "ADAPTER_STATE_TURNING_ON";
  case ADAPTER_STATE_ON:
    return "ADAPTER_STATE_ON";
  case ADAPTER_STATE_TURNING_OFF:
    return "ADAPTER_STATE_TURNING_OFF";
  default:
    return "unknown state";
  }
}

}  // namespace bluetooth
