/******************************************************************************
 *
 *  Copyright (C) 2009-2012 Broadcom Corporation
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

#ifndef BT_UTILS_H
#define BT_UTILS_H

static const char BT_UTILS_MODULE[] = "bt_utils_module";

#include <stdbool.h>
/*******************************************************************************
**  Type definitions
********************************************************************************/

typedef enum {
    TASK_HIGH_MEDIA = 0,
    TASK_HIGH_GKI_TIMER,
    TASK_HIGH_BTU,
    TASK_HIGH_HCI_WORKER,
    TASK_HIGH_USERIAL_READ,
    TASK_UIPC_READ,
    TASK_JAVA_ALARM,
    TASK_HIGH_MAX
} tHIGH_PRIORITY_TASK;

/* Run-time configuration file to store AVRCP version info*/
#ifndef AVRC_PEER_VERSION_CONF_FILE
#ifdef ANDROID
#define AVRC_PEER_VERSION_CONF_FILE "/data/misc/bluedroid/avrc_peer_entries.conf"
#else
#define AVRC_PEER_VERSION_CONF_FILE "/data/misc/bluetooth/avrc_peer_entries.conf"
#endif
#endif

/*******************************************************************************
**  Functions
********************************************************************************/

typedef enum {
    METHOD_BD = 0,
    METHOD_NAME
} tBLACKLIST_METHOD;

typedef enum {
    BT_SOC_DEFAULT = 0,
    BT_SOC_SMD = BT_SOC_DEFAULT,
    BT_SOC_AR3K,
    BT_SOC_ROME,
    BT_SOC_CHEROKEE,
    /* Add chipset type here */
    BT_SOC_RESERVED
} bt_soc_type;

#define MAX_NAME_LEN                  (50)
#ifdef ANDROID
#define IOT_DEV_BASE_CONF_FILE        "/etc/bluetooth/iot_devlist.conf"
#define IOT_DEV_CONF_FILE             "/data/misc/bluedroid/iot_devlist.conf"
#define IOT_DEV_CONF_BKP_FILE         "/data/misc/bluedroid/iot_devlist_bkp.conf"
#else
#define IOT_DEV_BASE_CONF_FILE        "/misc/bluetooth/iot_devlist.conf"
#define IOT_DEV_CONF_FILE             "/misc/bluetooth/iot_devlist.conf"
#define IOT_DEV_CONF_BKP_FILE         "/misc/bluetooth/iot_devlist_bkp.conf"
#endif
#define IOT_ROLE_CHANGE_BLACKLIST     "RoleChangeBlacklistAddr"
#define IOT_HFP_1_7_BLACKLIST          "Hfp1_7BlacklistAddr"
#define COD_AUDIO_DEVICE              (0x200400)
void raise_priority_a2dp(tHIGH_PRIORITY_TASK high_task);
void adjust_priority_a2dp(int start);
void load_iot_devlist(const char *filename);
void unload_iot_devlist();
bool is_device_present(char* header, unsigned char* device_details);
bool add_iot_device(const char *filename, char* header,
    unsigned char* device_details, tBLACKLIST_METHOD method_type);
bool remove_iot_device(const char *filename, char* header,
    unsigned char* device_details, tBLACKLIST_METHOD method_type);
bt_soc_type get_soc_type();
#define UNUSED(x) (void)(x)
#endif /* BT_UTILS_H */
