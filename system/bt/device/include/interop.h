/******************************************************************************
 *
 *  Copyright (C) 2015 Google, Inc.
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at:
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 ******************************************************************************/

#pragma once

#include <stdbool.h>

#include "btcore/include/bdaddr.h"

static const char INTEROP_MODULE[] = "interop_module";

// NOTE:
// Only add values at the end of this enum and before END_OF_INTEROP_LIST
// do NOT delete values as they may be used in dynamic device configuration.
typedef enum {

  BEGINING_OF_INTEROP_LIST = 0,
  // Disable secure connections
  // This is for pre BT 4.1/2 devices that do not handle secure mode
  // very well.
  INTEROP_DISABLE_LE_SECURE_CONNECTIONS = BEGINING_OF_INTEROP_LIST,

  // Some devices have proven problematic during the pairing process, often
  // requiring multiple retries to complete pairing. To avoid degrading the user
  // experience for those devices, automatically re-try pairing if page
  // timeouts are received during pairing.
  INTEROP_AUTO_RETRY_PAIRING,

  // Devices requiring this workaround do not handle Bluetooth Absolute Volume
  // control correctly, leading to undesirable (potentially harmful) volume levels
  // or general lack of controlability.
  INTEROP_DISABLE_ABSOLUTE_VOLUME,

  // Disable automatic pairing with headsets/car-kits
  // Some car kits do not react kindly to a failed pairing attempt and
  // do not allow immediate re-pairing. Blacklist these so that the initial
  // pairing attempt makes it to the user instead.
  INTEROP_DISABLE_AUTO_PAIRING,

  // Use a fixed pin for specific keyboards
  // Keyboards should use a variable pin at all times. However, some keyboards
  // require a fixed pin of all 0000. This workaround enables auto pairing for
  // those keyboards.
  INTEROP_KEYBOARD_REQUIRES_FIXED_PIN,

  // Some headsets have audio jitter issues because of increased re-transmissions as the
  // 3 Mbps packets have a lower link margin, and are more prone to interference. We can
  // disable 3DH packets (use only 2DH packets) for the ACL link to improve sensitivity
  // when streaming A2DP audio to the headset. Air sniffer logs show reduced
  // re-transmissions after switching to 2DH packets.
  //
  // Disable 3Mbps packets and use only 2Mbps packets for ACL links when streaming audio.
  INTEROP_2MBPS_LINK_ONLY,

  // Some HID devices have proven problematic behaviour if SDP is initiated again
  // while HID connection is in progress or if more than 1 SDP connection is created
  // with those HID devices rsulting in issues of connection failure with such devices.
  // To avoid degrading the user experience with those devices, SDP is not attempted
  // as part of pairing process.
  INTEROP_DISABLE_SDP_AFTER_PAIRING,

  // Some HID pointing devices have proven problematic behaviour if pairing is initiated with
  // them, resulting in no response for authentication request and ultimately resulting
  // in connection failure.
  // To avoid degrading the user experience with those devices, authentication request
  // is not requested explictly.
  INTEROP_DISABLE_AUTH_FOR_HID_POINTING,

  // HID Keyboards that claim support for multitouch functionality have issue with
  // normal functioning of keyboard because of issues in USB HID kernel driver.
  // To avoid degrading the user experience with those devices, digitizer record
  // is removed from the report descriptor.
  INTEROP_REMOVE_HID_DIG_DESCRIPTOR,

  // Some HID devices have problematic behaviour where when hid link is in Sniff
  // and DUT is in Slave role for SCO link ( not eSCO) any solution cannot maintain
  // the link as  SCO scheduling over a short period will overlap with Sniff link due to
  // slave drift.
  // To avoid degrading the user experience with those devices, sniff is disabled from
  // link policy when sco is active, and enabled when sco is disabled.
  INTEROP_DISABLE_SNIFF_DURING_SCO,

  //Few carkits take long time to start sending AT commands
  //Increase AG_CONN TIMEOUT so that AG connection go through
  INTEROP_INCREASE_AG_CONN_TIMEOUT,

  // Some HOGP devices do not respond well when we switch from default LE conn parameters
  // to preferred conn params immediately post connection. Disable automatic switching to
  // preferred conn params for such devices and allow them to explicity ask for it.
  INTEROP_DISABLE_LE_CONN_PREFERRED_PARAMS,

  // Few remote devices do not understand AVRCP version greater than 1.3. For these
  // devices, we would like to blacklist them and advertise AVRCP version as 1.3
  INTEROP_ADV_AVRCP_VER_1_3,
  // certain remote A2DP sinks have issue playing back Music in AAC format.
  // disable AAC for those headsets so that it switch to SBC
  INTEROP_DISABLE_AAC_CODEC,

  // Some car kits notifies role switch supported but it rejects
  // the role switch and after some attempts of role switch
  // car kits will go to bad state.
  INTEROP_DYNAMIC_ROLE_SWITCH,

  // Disable role switch for headsets/car-kits
  // Some car kits allow role switch but when DUT initiates role switch
  // Remote will go to bad state and its leads to LMP time out.
  INTEROP_DISABLE_ROLE_SWITCH,

  // Disable role switch for headsets/car-kits
  // Some car kits initiate a role switch but won't initiate encryption
  // after role switch complete
  INTEROP_DISABLE_ROLE_SWITCH_POLICY,

  INTEROP_HFP_1_7_BLACKLIST,

  END_OF_INTEROP_LIST

} interop_feature_t;

// Check if a given |addr| matches a known interoperability workaround as identified
// by the |interop_feature_t| enum. This API is used for simple address based lookups
// where more information is not available. No look-ups or random address resolution
// are performed on |addr|.
bool interop_match_addr(const interop_feature_t feature, const bt_bdaddr_t *addr);

// Check if a given remote device |name| matches a known interoperability workaround.
// Name comparisons are case sensitive and do not allow for partial matches. As in, if
// |name| is "TEST" and a workaround exists for "TESTING", then this function will
// return false. But, if |name| is "TESTING" and a workaround exists for "TEST", this
// function will return true.
// |name| cannot be null and must be null terminated.
bool interop_match_name(const interop_feature_t feature, const char *name);

// Check if a given remote device |name| matches a known interoperability workaround.
// Name comparisons are case sensitive and do not allow for partial matches. As in, if
// |name| is "TEST" and a workaround exists for "TESTING", then this function will
// return false. But, if |name| is "TESTING" and a workaround exists for "TEST", this
// function will return true.
// |name| cannot be null and must be null terminated.
bool interop_match_name(const interop_feature_t feature, const char *name);

// Check if a given |manufacturer| matches a known interoperability workaround as identified
// by the |interop_feature_t| enum. This API is used for manufacturer based lookups
// where more information is not available.
bool interop_match_manufacturer(const interop_feature_t feature, uint16_t manufacturer);


// Check if a given |vendor_id, product_id, name| matches a known interoperability workaround
// as identified by the |interop_feature_t| enum. This API is used for simple name based lookups
// where more information is not available.
bool interop_match_vendor_product_ids(const interop_feature_t feature,
        uint16_t vendor_id, uint16_t product_id);

// Add a dynamic interop database entry for a device matching the first |length| bytes
// of |addr|, implementing the workaround identified by |feature|. |addr| may not be
// null and |length| must be greater than 0 and less than sizeof(bt_bdaddr_t).
// As |interop_feature_t| is not exposed in the public API, feature must be a valid
// integer representing an optoin in the enum.
void interop_database_add(const uint16_t feature, const bt_bdaddr_t *addr, size_t length);

// Clear the dynamic portion of the interoperability workaround database.
void interop_database_clear(void);
