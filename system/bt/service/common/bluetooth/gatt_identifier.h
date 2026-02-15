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

#include <memory>
#include <string>

#include <bluetooth/uuid.h>

namespace bluetooth {

// Used to uniquely identify a GATT object/attribute
// (service/characteristic/descriptor/include entry) after it has been
// registered with the stack. Each registered object will be assigned a GATT
// identifier that the callers may use in future callbacks.
//
// For local services, the uniqueness of each identifier is guaranteed only
// within the registered GATT server that they exist in.
class GattIdentifier final {
 public:
  // Static initialization methods. These return NULL if invalid parameters are
  // given.
  static std::unique_ptr<GattIdentifier> CreateServiceId(
      const std::string& device_address,
      int id,
      const UUID& uuid,
      bool is_primary);
  static std::unique_ptr<GattIdentifier> CreateCharacteristicId(
      int id, const UUID& uuid,
      const GattIdentifier& service_id);
  static std::unique_ptr<GattIdentifier> CreateDescriptorId(
      int id, const UUID& uuid,
      const GattIdentifier& characteristic_id);

  // Constructors and assignment operator.
  GattIdentifier();
  GattIdentifier(
      const std::string& device_address,
      bool is_primary,
      const UUID& service_uuid,
      const UUID& characteristic_uuid,
      const UUID& descriptor_uuid,
      int service_instance_id,
      int characteristic_instance_id,
      int descriptor_instance_id);
  ~GattIdentifier() = default;
  GattIdentifier(const GattIdentifier& other);
  GattIdentifier& operator=(const GattIdentifier& other);


  // Comparison function and operator.
  bool Equals(const GattIdentifier& other) const;
  bool operator==(const GattIdentifier& rhs) const;
  bool operator!=(const GattIdentifier& rhs) const;

  // Functions to verify the type of attribute represented by this identifier.
  bool IsService() const;
  bool IsCharacteristic() const;
  bool IsDescriptor() const;

  // For characteristics and descriptors, this returns the identifier of the
  // owning service. For services, this returns NULL.
  std::unique_ptr<GattIdentifier> GetOwningServiceId() const;

  // For descriptors, this returns the identifier of the owning characteristic.
  // For services and characteristics, this returns NULL.
  std::unique_ptr<GattIdentifier> GetOwningCharacteristicId() const;

  // Getters for internal fields.
  const std::string& device_address() const { return device_address_; }
  bool is_primary() const { return is_primary_; }
  const UUID& service_uuid() const { return service_uuid_; }
  const UUID& characteristic_uuid() const { return char_uuid_; }
  const UUID& descriptor_uuid() const { return desc_uuid_; }
  int service_instance_id() const { return service_instance_id_; }
  int characteristic_instance_id() const { return char_instance_id_; }
  int descriptor_instance_id() const { return desc_instance_id_; }

 private:
  friend struct std::hash<bluetooth::GattIdentifier>;

  // NOTE: Don't forget to update the std::hash specialization below if you
  // update any of the instance variables in this class.

  // The BD_ADDR of the device associated with the attribute.
  std::string device_address_;

  // An instance ID value of -1 means that it is unitialized. For example, a
  // service ID would have -1 for characteristic and descriptor instance IDs.
  bool is_primary_;
  UUID service_uuid_;
  UUID char_uuid_;
  UUID desc_uuid_;
  int service_instance_id_;
  int char_instance_id_;
  int desc_instance_id_;
};

}  // namespace bluetooth

// Custom std::hash specialization so that bluetooth::GattIdentifier can be used
// as a key in std::unordered_map.
namespace std {

template<>
struct hash<bluetooth::GattIdentifier> {
  std::size_t operator()(const bluetooth::GattIdentifier& key) const {
    std::size_t seed = 0;

    hash_combine(seed, key.device_address_);
    hash_combine(seed, key.is_primary_);
    hash_combine(seed, key.service_uuid_);
    hash_combine(seed, key.char_uuid_);
    hash_combine(seed, key.desc_uuid_);
    hash_combine(seed, key.service_instance_id_);
    hash_combine(seed, key.char_instance_id_);
    hash_combine(seed, key.desc_instance_id_);

    return seed;
  }

 private:
  template<typename T>
  inline void hash_combine(std::size_t& seed, const T& v) const {
    std::hash<T> hasher;
    seed ^= hasher(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
  }
};

}  // namespace std
