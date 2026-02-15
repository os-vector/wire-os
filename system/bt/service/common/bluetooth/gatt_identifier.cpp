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

#include "service/common/bluetooth/gatt_identifier.h"

#include "service/common/bluetooth/util/address_helper.h"

namespace bluetooth {

namespace {

const int kInvalidInstanceId = -1;

}  // namespace

// static
std::unique_ptr<GattIdentifier> GattIdentifier::CreateServiceId(
    const std::string& device_address,
    int id, const UUID& uuid,
    bool is_primary) {
  if (id < 0 ||
      (!device_address.empty() && !util::IsAddressValid(device_address)))
    return nullptr;

  std::unique_ptr<GattIdentifier> gatt_id(new GattIdentifier());

  gatt_id->device_address_ = device_address;
  gatt_id->service_uuid_ = uuid;
  gatt_id->service_instance_id_ = id;
  gatt_id->is_primary_ = is_primary;

  return gatt_id;
}

// static
std::unique_ptr<GattIdentifier> GattIdentifier::CreateCharacteristicId(
    int id, const UUID& uuid,
    const GattIdentifier& service_id) {
  if (!service_id.IsService())
    return nullptr;

  std::unique_ptr<GattIdentifier> gatt_id(new GattIdentifier(service_id));

  gatt_id->char_uuid_ = uuid;
  gatt_id->char_instance_id_ = id;

  return gatt_id;
}

// static
std::unique_ptr<GattIdentifier> GattIdentifier::CreateDescriptorId(
    int id, const UUID& uuid,
    const GattIdentifier& char_id) {
  if (!char_id.IsCharacteristic())
    return nullptr;

  std::unique_ptr<GattIdentifier> gatt_id(new GattIdentifier(char_id));

  gatt_id->desc_uuid_ = uuid;
  gatt_id->desc_instance_id_ = id;

  return gatt_id;
}

// Copy constructor and assignment operator.
GattIdentifier::GattIdentifier()
  : is_primary_(false),
    service_instance_id_(kInvalidInstanceId),
    char_instance_id_(kInvalidInstanceId),
    desc_instance_id_(kInvalidInstanceId) {
}

GattIdentifier::GattIdentifier(const GattIdentifier& other) {
  device_address_ = other.device_address_;
  is_primary_ = other.is_primary_;
  service_uuid_ = other.service_uuid_;
  char_uuid_ = other.char_uuid_;
  desc_uuid_ = other.desc_uuid_;
  service_instance_id_ = other.service_instance_id_;
  service_instance_id_ = other.service_instance_id_;
  char_instance_id_ = other.char_instance_id_;
  desc_instance_id_ = other.desc_instance_id_;
}

GattIdentifier::GattIdentifier(
    const std::string& device_address,
    bool is_primary,
    const UUID& service_uuid,
    const UUID& characteristic_uuid,
    const UUID& descriptor_uuid,
    int service_instance_id,
    int characteristic_instance_id,
    int descriptor_instance_id)
    : device_address_(device_address),
      is_primary_(is_primary),
      service_uuid_(service_uuid),
      char_uuid_(characteristic_uuid),
      desc_uuid_(descriptor_uuid),
      service_instance_id_(service_instance_id),
      char_instance_id_(characteristic_instance_id),
      desc_instance_id_(descriptor_instance_id) {
}

GattIdentifier& GattIdentifier::operator=(const GattIdentifier& other) {
  if (*this == other)
    return *this;

  device_address_ = other.device_address_;
  is_primary_ = other.is_primary_;
  service_uuid_ = other.service_uuid_;
  char_uuid_ = other.char_uuid_;
  desc_uuid_ = other.desc_uuid_;
  service_instance_id_ = other.service_instance_id_;
  char_instance_id_ = other.char_instance_id_;
  desc_instance_id_ = other.desc_instance_id_;

  return *this;
}

bool GattIdentifier::Equals(const GattIdentifier& other) const {
  return (device_address_ == other.device_address_ &&
      is_primary_ == other.is_primary_ &&
      service_uuid_ == other.service_uuid_ &&
      char_uuid_ == other.char_uuid_ &&
      desc_uuid_ == other.desc_uuid_ &&
      service_instance_id_ == other.service_instance_id_ &&
      char_instance_id_ == other.char_instance_id_ &&
      desc_instance_id_ == other.desc_instance_id_);
}

bool GattIdentifier::operator==(const GattIdentifier& rhs) const {
  return Equals(rhs);
}

bool GattIdentifier::operator!=(const GattIdentifier& rhs) const {
  return !Equals(rhs);
}

bool GattIdentifier::IsService() const {
  return (service_instance_id_ != kInvalidInstanceId &&
          char_instance_id_ == kInvalidInstanceId &&
          desc_instance_id_ == kInvalidInstanceId);
}

bool GattIdentifier::IsCharacteristic() const {
  return (service_instance_id_ != kInvalidInstanceId &&
          char_instance_id_ != kInvalidInstanceId &&
          desc_instance_id_ == kInvalidInstanceId);
}

bool GattIdentifier::IsDescriptor() const {
  return (service_instance_id_ != kInvalidInstanceId &&
          char_instance_id_ != kInvalidInstanceId &&
          desc_instance_id_ != kInvalidInstanceId);
}

std::unique_ptr<GattIdentifier> GattIdentifier::GetOwningServiceId() const {
  if (IsService())
    return nullptr;

  return CreateServiceId(
      device_address_, service_instance_id_, service_uuid_, is_primary_);
}

std::unique_ptr<GattIdentifier>
GattIdentifier::GetOwningCharacteristicId() const {
  if (!IsDescriptor())
    return nullptr;

  std::unique_ptr<GattIdentifier> service_id = GetOwningServiceId();

  return CreateCharacteristicId(char_instance_id_, char_uuid_, *service_id);
}

}  // namespace bluetooth
