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

#include "service/common/bluetooth/util/address_helper.h"

#include <cstdlib>

#include <base/logging.h>
#include <base/strings/string_split.h>

namespace util {

bool IsAddressValid(const std::string& address) {
  bt_bdaddr_t addr;
  return BdAddrFromString(address, &addr);
}

bool BdAddrFromString(const std::string& address, bt_bdaddr_t* out_addr) {
  CHECK(out_addr);

  if (address.length() != 17)
    return false;

  std::vector<std::string> byte_tokens = base::SplitString(
      address, ":", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);

  if (byte_tokens.size() != 6)
    return false;

  for (int i = 0; i < 6; i++) {
    const auto& token = byte_tokens[i];

    if (token.length() != 2)
      return false;

    char* temp = nullptr;
    out_addr->address[i] = strtol(token.c_str(), &temp, 16);
    if (*temp != '\0')
      return false;
  }

  return true;
}

}  // namespace util
