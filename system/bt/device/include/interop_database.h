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

#include "device/include/interop.h"

typedef struct {
  bt_bdaddr_t addr;
  size_t length;
  interop_feature_t feature;
} interop_addr_entry_t;

static const interop_addr_entry_t interop_addr_database[] = {
  // Nexus Remote (Spike)
  // Note: May affect other Asus brand devices
  {{{0x08, 0x62, 0x66,      0,0,0}}, 3, INTEROP_DISABLE_LE_SECURE_CONNECTIONS},
  {{{0x38, 0x2c, 0x4a, 0xc9,  0,0}}, 4, INTEROP_DISABLE_LE_SECURE_CONNECTIONS},
  {{{0x38, 0x2c, 0x4a, 0xe6,  0,0}}, 4, INTEROP_DISABLE_LE_SECURE_CONNECTIONS},
  {{{0x54, 0xa0, 0x50, 0xd9,  0,0}}, 4, INTEROP_DISABLE_LE_SECURE_CONNECTIONS},
  {{{0xac, 0x9e, 0x17,      0,0,0}}, 3, INTEROP_DISABLE_LE_SECURE_CONNECTIONS},
  {{{0xf0, 0x79, 0x59,      0,0,0}}, 3, INTEROP_DISABLE_LE_SECURE_CONNECTIONS},

  // Ausdom M05 - unacceptably loud volume
  {{{0xa0, 0xe9, 0xdb,      0,0,0}}, 3, INTEROP_DISABLE_ABSOLUTE_VOLUME},

  // BMW car kits (Harman/Becker)
  {{{0x9c, 0xdf, 0x03,      0,0,0}}, 3, INTEROP_AUTO_RETRY_PAIRING},

  // Flic smart button
  {{{0x80, 0xe4, 0xda, 0x70,  0,0}}, 4, INTEROP_DISABLE_LE_SECURE_CONNECTIONS},

  // iKross IKBT83B HS - unacceptably loud volume
  {{{0x00, 0x14, 0x02,      0,0,0}}, 3, INTEROP_DISABLE_ABSOLUTE_VOLUME},

  // Jabra EXTREME 2 - unacceptably loud volume
  {{{0x1c, 0x48, 0xf9,      0,0,0}}, 3, INTEROP_DISABLE_ABSOLUTE_VOLUME},

  // JayBird BlueBuds X - low granularity on volume control
  {{{0x44, 0x5e, 0xf3,      0,0,0}}, 3, INTEROP_DISABLE_ABSOLUTE_VOLUME},
  {{{0xd4, 0x9c, 0x28,      0,0,0}}, 3, INTEROP_DISABLE_ABSOLUTE_VOLUME},

  // JayBird Family
  {{{0x00, 0x18, 0x91,      0,0,0}}, 3, INTEROP_2MBPS_LINK_ONLY},

  // LG Tone HBS-730 - unacceptably loud volume
  {{{0x00, 0x18, 0x6b,      0,0,0}}, 3, INTEROP_DISABLE_ABSOLUTE_VOLUME},
  {{{0xb8, 0xad, 0x3e,      0,0,0}}, 3, INTEROP_DISABLE_ABSOLUTE_VOLUME},

  // LG Tone HV-800 - unacceptably loud volume
  {{{0xa0, 0xe9, 0xdb,      0,0,0}}, 3, INTEROP_DISABLE_ABSOLUTE_VOLUME},

  // Motorola Key Link
  {{{0x1c, 0x96, 0x5a,      0,0,0}}, 3, INTEROP_DISABLE_LE_SECURE_CONNECTIONS},

  // Mpow Cheetah - unacceptably loud volume
  {{{0x00, 0x11, 0xb1,      0,0,0}}, 3, INTEROP_DISABLE_ABSOLUTE_VOLUME},

  // Nissan car kits (ALPS) - auto-pairing fails and rejects next pairing
  {{{0x34, 0xc7, 0x31,      0,0,0}}, 3, INTEROP_DISABLE_AUTO_PAIRING},

  // SOL REPUBLIC Tracks Air - unable to adjust volume back off from max
  {{{0xa4, 0x15, 0x66,      0,0,0}}, 3, INTEROP_DISABLE_ABSOLUTE_VOLUME},

  // Subaru car kits (ALPS) - auto-pairing fails and rejects next pairing
  {{{0x00, 0x07, 0x04,      0,0,0}}, 3, INTEROP_DISABLE_AUTO_PAIRING},
  {{{0xe0, 0x75, 0x0a,      0,0,0}}, 3, INTEROP_DISABLE_AUTO_PAIRING},

  // Swage Rokitboost HS - unacceptably loud volume
  {{{0x00, 0x14, 0xf1,      0,0,0}}, 3, INTEROP_DISABLE_ABSOLUTE_VOLUME},

  // VW Car Kit - not enough granularity with volume
  {{{0x00, 0x26, 0x7e,      0,0,0}}, 3, INTEROP_DISABLE_ABSOLUTE_VOLUME},
  {{{0x90, 0x03, 0xb7,      0,0,0}}, 3, INTEROP_DISABLE_ABSOLUTE_VOLUME},

  // Unknown keyboard (carried over from auto_pair_devlist.conf)
  {{{0x00, 0x0F, 0xF6,      0,0,0}}, 3, INTEROP_KEYBOARD_REQUIRES_FIXED_PIN},

  // Apple Magic Mouse
  {{{0x04, 0x0C, 0xCE,       0,0,0}}, 3, INTEROP_DISABLE_SDP_AFTER_PAIRING},
  // Bluetooth Laser Travel Mouse
  {{{0x00, 0x07, 0x61,       0,0,0}}, 3, INTEROP_DISABLE_SDP_AFTER_PAIRING},
  // Microsoft Bluetooth Notebook Mouse 5000
  {{{0x00, 0x1d, 0xd8,       0,0,0}}, 3, INTEROP_DISABLE_SDP_AFTER_PAIRING},
  // Logitech MX Revolution Mouse
  {{{0x00, 0x1f, 0x20,       0,0,0}}, 3, INTEROP_DISABLE_SDP_AFTER_PAIRING},
  // Rapoo 6080 mouse
  {{{0x6c, 0x5d, 0x63,       0,0,0}}, 3, INTEROP_DISABLE_SDP_AFTER_PAIRING},
  // Microsoft Sculpt Touch Mouse
  {{{0x28, 0x18, 0x78,       0,0,0}}, 3, INTEROP_DISABLE_SDP_AFTER_PAIRING},
  // Tero's Game Controller
  {{{0x60, 0x45, 0xBD,       0,0,0}}, 3, INTEROP_DISABLE_SDP_AFTER_PAIRING},

  // Targus BT Laser Notebook Mouse
  {{{0x00, 0x12, 0xA1,       0,0,0}}, 3, INTEROP_DISABLE_AUTH_FOR_HID_POINTING},

  // Bluetooth Keyboard
  {{{0x20, 0x4C, 0x10,       0,0,0}}, 3, INTEROP_DISABLE_SNIFF_DURING_SCO},

  // Fiat Carkit
  {{{0x00, 0x14, 0x09,       0,0,0}}, 3, INTEROP_INCREASE_AG_CONN_TIMEOUT},

  // Dialog Keyboard and mouse
  {{{0x80, 0xea, 0xca,      0,0,0}}, 3, INTEROP_DISABLE_LE_SECURE_CONNECTIONS},

  // Marvel CK used in Mercedes C300/BMW 640i
  // For a more specific black listing(e.g. just for Mercedes), both BD addr
  // and device name has to be added for AVRCP 1.3 blacklisting
  {{{0xa0, 0x56, 0xb2,      0,0,0}}, 3, INTEROP_ADV_AVRCP_VER_1_3},

  // Mazda Atenza
  {{{0x04, 0xf8, 0xc2,      0,0,0}}, 3, INTEROP_DISABLE_ABSOLUTE_VOLUME},
  // Beats Solo 3
  {{{0x20, 0x3c, 0xae,      0,0,0}}, 3, INTEROP_DISABLE_AAC_CODEC},
  // Cadillac
  {{{0x28, 0xA1, 0x83,      0,0,0}}, 3, INTEROP_DISABLE_AAC_CODEC},
  // Buick Verona
  {{{0xAC, 0x7A, 0x4D,      0,0,0}}, 3, INTEROP_DISABLE_AAC_CODEC},
};

typedef struct {
  char name[249];
  size_t length;
  interop_feature_t feature;
} interop_name_entry_t;

static const interop_name_entry_t interop_name_database[] = {
  // Carried over from auto_pair_devlist.conf migration
  {"Audi",    4, INTEROP_DISABLE_AUTO_PAIRING},
  {"BMW",     3, INTEROP_DISABLE_AUTO_PAIRING},
  {"Parrot",  6, INTEROP_DISABLE_AUTO_PAIRING},
  {"Car",     3, INTEROP_DISABLE_AUTO_PAIRING},

  // Nissan Quest rejects pairing after "0000"
  {"NISSAN",  6, INTEROP_DISABLE_AUTO_PAIRING},

  // Subaru car kits ("CAR M_MEDIA")
  {"CAR",     3, INTEROP_DISABLE_AUTO_PAIRING},

  // HID SDP Blacklist
  {"Apple Magic Mouse", 17, INTEROP_DISABLE_SDP_AFTER_PAIRING},
  {"Bluetooth Laser Travel Mouse", 28, INTEROP_DISABLE_SDP_AFTER_PAIRING},
  {"Microsoft Bluetooth Notebook Mouse 5000", 39, INTEROP_DISABLE_SDP_AFTER_PAIRING},
  {"Logitech MX Revolution Mouse", 28, INTEROP_DISABLE_SDP_AFTER_PAIRING},
  {"Microsoft Sculpt Touch Mouse", 28, INTEROP_DISABLE_SDP_AFTER_PAIRING},
  {"Tero's Game Controller", 22, INTEROP_DISABLE_SDP_AFTER_PAIRING},

  // HID Authentication Blacklist
  {"Targus BT Laser Notebook Mouse", 30, INTEROP_DISABLE_AUTH_FOR_HID_POINTING},

  //Below devices reject connection updated with preferred
  {"BSMBB09DS", 9, INTEROP_DISABLE_LE_CONN_PREFERRED_PARAMS},
  {"ELECOM", 6, INTEROP_DISABLE_LE_CONN_PREFERRED_PARAMS},
  {"MB Bluetooth", 12, INTEROP_ADV_AVRCP_VER_1_3},

  // HID Moto KZ500 Keyboard - Problematic SDP digitizer descriptor
  {"Motorola Keyboard KZ500", 23, INTEROP_REMOVE_HID_DIG_DESCRIPTOR},
  {"Motorola Keyboard KZ500 v122", 28, INTEROP_REMOVE_HID_DIG_DESCRIPTOR},
};

typedef struct {
  uint16_t manufacturer;
  interop_feature_t feature;
} interop_manufacturer_t;

static const interop_manufacturer_t interop_manufacturer_database[] = {
  // Apple Devices
  {76, INTEROP_DISABLE_SDP_AFTER_PAIRING},

  // Apple Devices
  {76, INTEROP_DISABLE_SNIFF_DURING_SCO},
};

typedef struct {
  uint16_t vendor_id;
  uint16_t product_id;
  interop_feature_t feature;
} interop_hid_multitouch_t;

static const interop_hid_multitouch_t interop_hid_multitouch_database[] = {
  // HID Moto KZ500 Keyboard - Problematic SDP digitizer descriptor
  {0x22b8, 0x093d, INTEROP_REMOVE_HID_DIG_DESCRIPTOR},
};
