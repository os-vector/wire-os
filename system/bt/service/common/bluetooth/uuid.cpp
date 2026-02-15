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

#include "service/common/bluetooth/uuid.h"

#include <algorithm>
#include <array>
#include <stack>
#include <string>

#include <base/rand_util.h>
#include <base/strings/stringprintf.h>
#include <base/strings/string_split.h>
#include <base/strings/string_util.h>

namespace bluetooth {

namespace {

const UUID::UUID128Bit kSigBaseUUID = {
  { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00,
    0x80, 0x00, 0x00, 0x80, 0x5f, 0x9b, 0x34, 0xfb }
};

}  // namespace

// static
UUID UUID::GetRandom() {
  UUID128Bit bytes;
  base::RandBytes(bytes.data(), bytes.size());
  return UUID(bytes);
}

// static
UUID UUID::GetNil() {
  UUID128Bit bytes;
  bytes.fill(0);
  return UUID(bytes);
}

// static
UUID UUID::GetMax() {
  UUID128Bit bytes;
  bytes.fill(1);
  return UUID(bytes);
}

void UUID::InitializeDefault() {
  // Initialize to Bluetooth SIG base UUID.
  id_ = kSigBaseUUID;
  is_valid_ = true;
}

UUID::UUID() {
  InitializeDefault();
}

UUID::UUID(std::string uuid) {
  InitializeDefault();
  is_valid_ = false;

  if (uuid.empty())
    return;

  if (uuid.size() < 11 && uuid.find("0x") == 0)
    uuid = uuid.substr(2);

  if (uuid.size() != 4 && uuid.size() != 8 && uuid.size() != 36)
    return;

  if (uuid.size() == 36) {
    if (uuid[8] != '-')
      return;
    if (uuid[13] != '-')
      return;
    if (uuid[18] != '-')
      return;
    if (uuid[23] != '-')
      return;

    std::vector<std::string> tokens = base::SplitString(
        uuid, "-", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);

    if (tokens.size() != 5)
      return;

    uuid = base::JoinString(tokens, "");
  }

  const int start_index = uuid.size() == 4 ? 2 : 0;
  const size_t copy_size = std::min(id_.size(), uuid.size() / 2);
  for (size_t i = 0; i < copy_size; ++i) {
      std::string octet_text(uuid, i * 2, 2);
      char* temp = nullptr;
      id_[start_index + i] = strtol(octet_text.c_str(), &temp, 16);
      if (*temp != '\0')
        return;
  }

  is_valid_ = true;
}

UUID::UUID(const bt_uuid_t& uuid) {
  std::reverse_copy(uuid.uu, uuid.uu + sizeof(uuid.uu), id_.begin());
  is_valid_ = true;
}

UUID::UUID(const UUID16Bit& uuid) {
  InitializeDefault();
  std::copy(uuid.begin(), uuid.end(), id_.begin() + kNumBytes16);
}

UUID::UUID(const UUID32Bit& uuid) {
  InitializeDefault();
  std::copy(uuid.begin(), uuid.end(), id_.begin());
}

UUID::UUID(const UUID128Bit& uuid) : id_(uuid), is_valid_(true) {}

UUID::UUID128Bit UUID::GetFullBigEndian() const {
  return id_;
}

UUID::UUID128Bit UUID::GetFullLittleEndian() const {
  UUID::UUID128Bit ret;
  std::reverse_copy(id_.begin(), id_.end(), ret.begin());
  return ret;
}

bt_uuid_t UUID::GetBlueDroid() const {
  bt_uuid_t ret;
  std::reverse_copy(id_.begin(), id_.end(), ret.uu);
  return ret;
}

std::string UUID::ToString() const {
  return base::StringPrintf(
      "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
      id_[0], id_[1], id_[2], id_[3],
      id_[4], id_[5], id_[6], id_[7],
      id_[8], id_[9], id_[10], id_[11],
      id_[12], id_[13], id_[14], id_[15]);
}

size_t UUID::GetShortestRepresentationSize() const {
  if (memcmp(id_.data() + 4, kSigBaseUUID.data() + 4, id_.size() - 4) != 0)
    return kNumBytes128;

  if (id_[0] == 0 && id_[1] == 0)
    return kNumBytes16;

  return kNumBytes32;
}

bool UUID::operator<(const UUID& rhs) const {
  return std::lexicographical_compare(id_.begin(), id_.end(), rhs.id_.begin(),
                                      rhs.id_.end());
}

bool UUID::operator==(const UUID& rhs) const {
  return std::equal(id_.begin(), id_.end(), rhs.id_.begin());
}

}  // namespace bluetooth
