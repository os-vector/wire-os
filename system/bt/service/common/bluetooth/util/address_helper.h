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

#include <hardware/bluetooth.h>
#include <string>

namespace util {

// Checks if the given string representing a Bluetooth device address (BD_ADDR)
// is correctly formatted. The correct formatting is of the form
//
//   XX:XX:XX:XX:XX:XX
//
// where X is an alpha-numeric character.
bool IsAddressValid(const std::string& address);

// Populates a bt_bdaddr_t from a given string. Returns false if the data is
// invalid.
bool BdAddrFromString(const std::string& address, bt_bdaddr_t* out_addr);

}  // namespace util
