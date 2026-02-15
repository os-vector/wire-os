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

#include <binder/Parcel.h>

#include <bluetooth/advertise_data.h>
#include <bluetooth/advertise_settings.h>
#include <bluetooth/gatt_identifier.h>
#include <bluetooth/scan_filter.h>
#include <bluetooth/scan_result.h>
#include <bluetooth/scan_settings.h>
#include <bluetooth/uuid.h>

#include <hardware/bt_gatt_client.h>

namespace ipc {
namespace binder {

// Java Parcel meta-data constants.
const int kParcelValList = 11;

// Helpers for converting bluetooth::AdvertiseData to/from Parcel

void WriteAdvertiseDataToParcel(
    const bluetooth::AdvertiseData& data,
    android::Parcel* parcel);

std::unique_ptr<bluetooth::AdvertiseData> CreateAdvertiseDataFromParcel(
    const android::Parcel& parcel);

// Helpers for converting bluetooth::AdvertiseSettings to/from Parcel

void WriteAdvertiseSettingsToParcel(
    const bluetooth::AdvertiseSettings& settings,
    android::Parcel* parcel);

std::unique_ptr<bluetooth::AdvertiseSettings> CreateAdvertiseSettingsFromParcel(
    const android::Parcel& parcel);

// Helpers for converting bluetooth::UUID to/from Parcel

void WriteUUIDToParcel(const bluetooth::UUID& uuid, android::Parcel* parcel);

std::unique_ptr<bluetooth::UUID> CreateUUIDFromParcel(
    const android::Parcel& parcel);

// Helpers for converting bluetooth::GattIdentifier to/from Parcel

void WriteGattIdentifierToParcel(
    const bluetooth::GattIdentifier& gatt_id,
    android::Parcel* parcel);

std::unique_ptr<bluetooth::GattIdentifier> CreateGattIdentifierFromParcel(
    const android::Parcel& parcel);

// Helpers for converting bluetooth::ScanFilter to/from Parcel

void WriteScanFilterToParcel(
    const bluetooth::ScanFilter& filter,
    android::Parcel* parcel);

std::unique_ptr<bluetooth::ScanFilter> CreateScanFilterFromParcel(
    const android::Parcel& parcel);

// Helpers for converting bluetooth::ScanSettings to/from Parcel

void WriteScanSettingsToParcel(
    const bluetooth::ScanSettings& settings,
    android::Parcel* parcel);

std::unique_ptr<bluetooth::ScanSettings> CreateScanSettingsFromParcel(
    const android::Parcel& parcel);

// Helpers for converting bluetooth::ScanResult to/from Parcel

void WriteScanResultToParcel(
    const bluetooth::ScanResult& scan_result,
    android::Parcel* parcel);

std::unique_ptr<bluetooth::ScanResult> CreateScanResultFromParcel(
    const android::Parcel& parcel);

// Helpers for converting array of btgatt_db_element_t to/from Parcel

void WriteBtGattDbArrayToParcel(const btgatt_db_element_t* db, int size,
                                android::Parcel* parcel);

void CreateBtGattDbArrayFromParcel(const android::Parcel& parcel,
                                   btgatt_db_element_t** pDb, int* pSize);

// Helpers for converting btgatt_read_params_t to/from Parcel
void WriteBtGattReadParamsToParcel(const btgatt_read_params_t* data,
                                   android::Parcel* parcel);

void CreateBtGattReadParamsFromParcel(const android::Parcel& parcel,
                                      btgatt_read_params_t** pData);

// Helpers for converting btgatt_notify_params_t to/from Parcel
void WriteBtGattNotifyParamsToParcel(const btgatt_notify_params_t* notification,
                                     android::Parcel* parcel);

void CreateBtGattNotifyParamsFromParcel(const android::Parcel& parcel,
                                        btgatt_notify_params_t** pNotification);

}  // namespace binder
}  // namespace ipc
