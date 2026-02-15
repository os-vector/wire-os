/******************************************************************************
 *
 *  Copyright (c) 2014 The Android Open Source Project
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

/************************************************************************************
 *
 *  Filename:      btif_storage.c
 *
 *  Description:   Stores the local BT adapter and remote device properties in
 *                 NVRAM storage, typically as xml file in the
 *                 mobile's filesystem
 *
 *
 */

#define LOG_TAG "bt_btif_storage"

#include "btif_storage.h"

#include <alloca.h>
#include <assert.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "bta_hh_api.h"
#include "btcore/include/bdaddr.h"
#include "btif_api.h"
#include "btif_config.h"
#include "btif_hh.h"
#include "btif_util.h"
#include "bt_common.h"
#include "osi/include/allocator.h"
#include "osi/include/compat.h"
#include "osi/include/config.h"
#include "osi/include/log.h"
#include "osi/include/osi.h"

/************************************************************************************
**  Constants & Macros
************************************************************************************/

// TODO(armansito): Find a better way than using a hardcoded path.
#ifdef ANDROID
#define BTIF_STORAGE_PATH_BLUEDROID "/data/misc/bluedroid"
#else
#define BTIF_STORAGE_PATH_BLUEDROID "/data/misc/bluetooth"
#endif
//#define BTIF_STORAGE_PATH_ADAPTER_INFO "adapter_info"
//#define BTIF_STORAGE_PATH_REMOTE_DEVICES "remote_devices"
#define BTIF_STORAGE_PATH_REMOTE_DEVTIME "Timestamp"
#define BTIF_STORAGE_PATH_REMOTE_DEVCLASS "DevClass"
#define BTIF_STORAGE_PATH_REMOTE_DEVTYPE "DevType"
#define BTIF_STORAGE_PATH_REMOTE_NAME "Name"
#define BTIF_STORAGE_PATH_REMOTE_VER_MFCT "Manufacturer"
#define BTIF_STORAGE_PATH_REMOTE_VER_VER "LmpVer"
#define BTIF_STORAGE_PATH_REMOTE_VER_SUBVER "LmpSubVer"

//#define BTIF_STORAGE_PATH_REMOTE_LINKKEYS "remote_linkkeys"
#define BTIF_STORAGE_PATH_REMOTE_ALIASE "Aliase"
#define BTIF_STORAGE_PATH_REMOTE_SERVICE "Service"
#define BTIF_STORAGE_PATH_REMOTE_HIDINFO "HidInfo"
#define BTIF_STORAGE_KEY_ADAPTER_NAME "Name"
#define BTIF_STORAGE_KEY_ADAPTER_SCANMODE "ScanMode"
#define BTIF_STORAGE_KEY_ADAPTER_DISC_TIMEOUT "DiscoveryTimeout"

/* This is a local property to add a device found */
#define BT_PROPERTY_REMOTE_DEVICE_TIMESTAMP 0xFF

#define BTIF_STORAGE_GET_ADAPTER_PROP(t,v,l,p) \
      {p.type=t;p.val=v;p.len=l; btif_storage_get_adapter_property(&p);}

#define BTIF_STORAGE_GET_REMOTE_PROP(b,t,v,l,p) \
      {p.type=t;p.val=v;p.len=l;btif_storage_get_remote_device_property(b,&p);}

#define STORAGE_BDADDR_STRING_SZ           (18)      /* 00:11:22:33:44:55 */
#define STORAGE_UUID_STRING_SIZE           (36+1)    /* 00001200-0000-1000-8000-00805f9b34fb; */
#define STORAGE_PINLEN_STRING_MAX_SIZE     (2)       /* ascii pinlen max chars */
#define STORAGE_KEYTYPE_STRING_MAX_SIZE    (1)       /* ascii keytype max chars */

#define STORAGE_KEY_TYPE_MAX               (10)

#define STORAGE_HID_ATRR_MASK_SIZE           (4)
#define STORAGE_HID_SUB_CLASS_SIZE           (2)
#define STORAGE_HID_APP_ID_SIZE              (2)
#define STORAGE_HID_VENDOR_ID_SIZE           (4)
#define STORAGE_HID_PRODUCT_ID_SIZE          (4)
#define STORAGE_HID_VERSION_SIZE             (4)
#define STORAGE_HID_CTRY_CODE_SIZE           (2)
#define STORAGE_HID_DESC_LEN_SIZE            (4)
#define STORAGE_HID_DESC_MAX_SIZE            (2*512)

/* <18 char bd addr> <space> LIST< <36 char uuid> <;> > <keytype (dec)> <pinlen> */
#define BTIF_REMOTE_SERVICES_ENTRY_SIZE_MAX (STORAGE_BDADDR_STRING_SZ + 1 +\
                                             STORAGE_UUID_STRING_SIZE*BT_MAX_NUM_UUIDS + \
                                             STORAGE_PINLEN_STRING_MAX_SIZE +\
                                             STORAGE_KEYTYPE_STRING_MAX_SIZE)

#define STORAGE_REMOTE_LINKKEYS_ENTRY_SIZE (LINK_KEY_LEN*2 + 1 + 2 + 1 + 2)

/* <18 char bd addr> <space>LIST <attr_mask> <space> > <sub_class> <space> <app_id> <space>
                                <vendor_id> <space> > <product_id> <space> <version> <space>
                                <ctry_code> <space> > <desc_len> <space> <desc_list> <space> */
#define BTIF_HID_INFO_ENTRY_SIZE_MAX    (STORAGE_BDADDR_STRING_SZ + 1 +\
                                         STORAGE_HID_ATRR_MASK_SIZE + 1 +\
                                         STORAGE_HID_SUB_CLASS_SIZE + 1 +\
                                         STORAGE_HID_APP_ID_SIZE+ 1 +\
                                         STORAGE_HID_VENDOR_ID_SIZE+ 1 +\
                                         STORAGE_HID_PRODUCT_ID_SIZE+ 1 +\
                                         STORAGE_HID_VERSION_SIZE+ 1 +\
                                         STORAGE_HID_CTRY_CODE_SIZE+ 1 +\
                                         STORAGE_HID_DESC_LEN_SIZE+ 1 +\
                                         STORAGE_HID_DESC_MAX_SIZE+ 1 )

/* currently remote services is the potentially largest entry */
#define BTIF_STORAGE_MAX_LINE_SZ BTIF_REMOTE_SERVICES_ENTRY_SIZE_MAX

/* check against unv max entry size at compile time */
#if (BTIF_STORAGE_ENTRY_MAX_SIZE > UNV_MAXLINE_LENGTH)
    #error "btif storage entry size exceeds unv max line size"
#endif

#define BTIF_STORAGE_HL_APP          "hl_app"
#define BTIF_STORAGE_HL_APP_CB       "hl_app_cb"
#define BTIF_STORAGE_HL_APP_DATA     "hl_app_data_"
#define BTIF_STORAGE_HL_APP_MDL_DATA "hl_app_mdl_data_"

/************************************************************************************
**  Local type definitions
************************************************************************************/
typedef struct
{
    uint32_t num_devices;
    bt_bdaddr_t devices[BTM_SEC_MAX_DEVICE_RECORDS];
} btif_bonded_devices_t;

/************************************************************************************
**  External variables
************************************************************************************/
extern UINT16 bta_service_id_to_uuid_lkup_tbl [BTA_MAX_SERVICE_ID];
extern bt_bdaddr_t btif_local_bd_addr;

/************************************************************************************
**  External functions
************************************************************************************/

extern void btif_gatts_add_bonded_dev_from_nv(BD_ADDR bda);

/************************************************************************************
**  Internal Functions
************************************************************************************/

static bt_status_t btif_in_fetch_bonded_ble_device(const char *remote_bd_addr,int add,
                                              btif_bonded_devices_t *p_bonded_devices);
static bt_status_t btif_in_fetch_bonded_device(const char *bdstr);

/************************************************************************************
**  Static functions
************************************************************************************/

static int prop2cfg(bt_bdaddr_t *remote_bd_addr, bt_property_t *prop)
{
    bdstr_t bdstr = {0};
    int name_length = 0;
    if(remote_bd_addr)
        bdaddr_to_string(remote_bd_addr, bdstr, sizeof(bdstr));
    BTIF_TRACE_DEBUG("in, bd addr:%s, prop type:%d, len:%d", bdstr, prop->type, prop->len);
    char value[1024];
    if(prop->len <= 0 || prop->len > (int)sizeof(value) - 1)
    {
        BTIF_TRACE_ERROR("property type:%d, len:%d is invalid", prop->type, prop->len);
        return FALSE;
    }
    switch(prop->type)
    {
       case BT_PROPERTY_REMOTE_DEVICE_TIMESTAMP:
            btif_config_set_int(bdstr,
                                BTIF_STORAGE_PATH_REMOTE_DEVTIME, (int)time(NULL));
            break;
        case BT_PROPERTY_BDNAME:
            name_length = prop->len > BTM_MAX_LOC_BD_NAME_LEN ? BTM_MAX_LOC_BD_NAME_LEN:
                                                                               prop->len;
            strncpy(value, (char*)prop->val, name_length);
            value[name_length]='\0';
            if(remote_bd_addr)
                btif_config_set_str(bdstr,
                                BTIF_STORAGE_PATH_REMOTE_NAME, value);
            else
            {
                btif_config_set_str("Adapter",
                                BTIF_STORAGE_KEY_ADAPTER_NAME, value);
                btif_config_flush();
            }
            break;
        case BT_PROPERTY_REMOTE_FRIENDLY_NAME:
            strncpy(value, (char*)prop->val, prop->len);
            value[prop->len]='\0';
            btif_config_set_str(bdstr, BTIF_STORAGE_PATH_REMOTE_ALIASE, value);
            break;
        case BT_PROPERTY_ADAPTER_SCAN_MODE:
            btif_config_set_int("Adapter",
                                BTIF_STORAGE_KEY_ADAPTER_SCANMODE, *(int*)prop->val);
            break;
        case BT_PROPERTY_ADAPTER_DISCOVERY_TIMEOUT:
            btif_config_set_int("Adapter",
                                BTIF_STORAGE_KEY_ADAPTER_DISC_TIMEOUT, *(int*)prop->val);
            break;
        case BT_PROPERTY_CLASS_OF_DEVICE:
            btif_config_set_int(bdstr,
                                BTIF_STORAGE_PATH_REMOTE_DEVCLASS, *(int*)prop->val);
            break;
        case BT_PROPERTY_TYPE_OF_DEVICE:
            btif_config_set_int(bdstr,
                                BTIF_STORAGE_PATH_REMOTE_DEVTYPE, *(int*)prop->val);
            break;
        case BT_PROPERTY_UUIDS:
        {
            uint32_t i;
            char buf[64];
            value[0] = 0;
            int size = sizeof(value);
            for (i=0; i < (prop->len)/sizeof(bt_uuid_t); i++)
            {
                bt_uuid_t *p_uuid = (bt_uuid_t*)prop->val + i;
                memset(buf, 0, sizeof(buf));
                uuid_to_string_legacy(p_uuid, buf);
                strlcat(value, buf, size);
                strlcat(value, " ", size);
            }
            btif_config_set_str(bdstr, BTIF_STORAGE_PATH_REMOTE_SERVICE, value);
            break;
        }
        case BT_PROPERTY_REMOTE_VERSION_INFO:
        {
            bt_remote_version_t *info = (bt_remote_version_t *)prop->val;

            if (!info)
                return FALSE;

            btif_config_set_int(bdstr,
                                BTIF_STORAGE_PATH_REMOTE_VER_MFCT, info->manufacturer);
            btif_config_set_int(bdstr,
                                BTIF_STORAGE_PATH_REMOTE_VER_VER, info->version);
            btif_config_set_int(bdstr,
                                BTIF_STORAGE_PATH_REMOTE_VER_SUBVER, info->sub_ver);
         } break;

        default:
             BTIF_TRACE_ERROR("Unknown prop type:%d", prop->type);
             return FALSE;
    }

    /* save changes if the device was bonded */
    if (btif_in_fetch_bonded_device(bdstr) == BT_STATUS_SUCCESS) {
      btif_config_flush();
    }

    return TRUE;
}

static int cfg2prop(bt_bdaddr_t *remote_bd_addr, bt_property_t *prop)
{
    bdstr_t bdstr = {0};
    if(remote_bd_addr)
        bdaddr_to_string(remote_bd_addr, bdstr, sizeof(bdstr));
    BTIF_TRACE_DEBUG("in, bd addr:%s, prop type:%d, len:%d", bdstr, prop->type, prop->len);
    if(prop->len <= 0)
    {
        BTIF_TRACE_ERROR("property type:%d, len:%d is invalid", prop->type, prop->len);
        return FALSE;
    }
    int ret = FALSE;
    switch(prop->type)
    {
       case BT_PROPERTY_REMOTE_DEVICE_TIMESTAMP:
            if(prop->len >= (int)sizeof(int))
                ret = btif_config_get_int(bdstr,
                                        BTIF_STORAGE_PATH_REMOTE_DEVTIME, (int*)prop->val);
            break;
        case BT_PROPERTY_BDNAME:
        {
            int len = prop->len;
            if(remote_bd_addr)
                ret = btif_config_get_str(bdstr,
                                        BTIF_STORAGE_PATH_REMOTE_NAME, (char*)prop->val, &len);
            else ret = btif_config_get_str("Adapter",
                                        BTIF_STORAGE_KEY_ADAPTER_NAME, (char*)prop->val, &len);
            if(ret && len && len <= prop->len)
                prop->len = len - 1;
            else
            {
                prop->len = 0;
                ret = FALSE;
            }
            break;
        }
        case BT_PROPERTY_REMOTE_FRIENDLY_NAME:
        {
            int len = prop->len;
            ret = btif_config_get_str(bdstr,
                                       BTIF_STORAGE_PATH_REMOTE_ALIASE, (char*)prop->val, &len);
            if(ret && len && len <= prop->len)
                prop->len = len - 1;
            else
            {
                prop->len = 0;
                ret = FALSE;
            }
            break;
        }
        case BT_PROPERTY_ADAPTER_SCAN_MODE:
           if(prop->len >= (int)sizeof(int))
                ret = btif_config_get_int("Adapter",
                                          BTIF_STORAGE_KEY_ADAPTER_SCANMODE, (int*)prop->val);
           break;
        case BT_PROPERTY_ADAPTER_DISCOVERY_TIMEOUT:
           if(prop->len >= (int)sizeof(int))
                ret = btif_config_get_int("Adapter",
                                          BTIF_STORAGE_KEY_ADAPTER_DISC_TIMEOUT, (int*)prop->val);
            break;
        case BT_PROPERTY_CLASS_OF_DEVICE:
            if(prop->len >= (int)sizeof(int))
                ret = btif_config_get_int(bdstr,
                                BTIF_STORAGE_PATH_REMOTE_DEVCLASS, (int*)prop->val);
            break;
        case BT_PROPERTY_TYPE_OF_DEVICE:
            if(prop->len >= (int)sizeof(int))
                ret = btif_config_get_int(bdstr, BTIF_STORAGE_PATH_REMOTE_DEVTYPE, (int*)prop->val);
            break;
        case BT_PROPERTY_UUIDS:
        {
            char value[1280];
            int size = sizeof(value);
            if(btif_config_get_str(bdstr,
                                    BTIF_STORAGE_PATH_REMOTE_SERVICE, value, &size))
            {
                bt_uuid_t *p_uuid = (bt_uuid_t*)prop->val;
                size_t num_uuids = btif_split_uuids_string(value, p_uuid, BT_MAX_NUM_UUIDS);
                prop->len = num_uuids * sizeof(bt_uuid_t);
                ret = TRUE;
            }
            else
            {
                prop->val = NULL;
                prop->len = 0;
            }
        } break;

        case BT_PROPERTY_REMOTE_VERSION_INFO:
        {
            bt_remote_version_t *info = (bt_remote_version_t *)prop->val;

            if(prop->len >= (int)sizeof(bt_remote_version_t))
            {
                ret = btif_config_get_int(bdstr,
                                BTIF_STORAGE_PATH_REMOTE_VER_MFCT, &info->manufacturer);

                if (ret == TRUE)
                    ret = btif_config_get_int(bdstr,
                                BTIF_STORAGE_PATH_REMOTE_VER_VER, &info->version);

                if (ret == TRUE)
                    ret = btif_config_get_int(bdstr,
                                BTIF_STORAGE_PATH_REMOTE_VER_SUBVER, &info->sub_ver);
            }
         } break;

        default:
            BTIF_TRACE_ERROR("Unknow prop type:%d", prop->type);
            return FALSE;
    }
    return ret;
}

/*******************************************************************************
**
** Function         btif_in_fetch_bonded_devices
**
** Description      Internal helper function to fetch the bonded devices
**                  from NVRAM
**
** Returns          BT_STATUS_SUCCESS if successful, BT_STATUS_FAIL otherwise
**
*******************************************************************************/
static bt_status_t btif_in_fetch_bonded_device(const char *bdstr)
{
    BOOLEAN bt_linkkey_file_found=FALSE;

        LINK_KEY link_key;
        size_t size = sizeof(link_key);
        if(btif_config_get_bin(bdstr, "LinkKey", (uint8_t *)link_key, &size))
        {
            int linkkey_type;
            if(btif_config_get_int(bdstr, "LinkKeyType", &linkkey_type))
            {
                bt_linkkey_file_found = TRUE;
            }
            else
            {
                bt_linkkey_file_found = FALSE;
            }
        }
#if (BLE_INCLUDED == TRUE)
        if((btif_in_fetch_bonded_ble_device(bdstr, FALSE, NULL) != BT_STATUS_SUCCESS)
                && (!bt_linkkey_file_found))
        {
            BTIF_TRACE_DEBUG("Remote device:%s, no link key or ble key found", bdstr);
            return BT_STATUS_FAIL;
        }
#else
        if((!bt_linkkey_file_found))
        {
            BTIF_TRACE_DEBUG("Remote device:%s, no link key found", bdstr);
            return BT_STATUS_FAIL;
        }
#endif
    return BT_STATUS_SUCCESS;
}

/*******************************************************************************
**
** Function         btif_in_fetch_bonded_devices
**
** Description      Internal helper function to fetch the bonded devices
**                  from NVRAM
**
** Returns          BT_STATUS_SUCCESS if successful, BT_STATUS_FAIL otherwise
**
*******************************************************************************/
static bt_status_t btif_in_fetch_bonded_devices(btif_bonded_devices_t *p_bonded_devices, int add)
{
    memset(p_bonded_devices, 0, sizeof(btif_bonded_devices_t));

    BOOLEAN bt_linkkey_file_found=FALSE;
    int device_type;

    for (const btif_config_section_iter_t *iter = btif_config_section_begin(); iter != btif_config_section_end(); iter = btif_config_section_next(iter)) {
        const char *name = btif_config_section_name(iter);
        if (!string_is_bdaddr(name))
            continue;

        BTIF_TRACE_DEBUG("Remote device:%s", name);
        LINK_KEY link_key;
        size_t size = sizeof(link_key);
        if (btif_config_get_bin(name, "LinkKey", link_key, &size)) {
            int linkkey_type;
            if (btif_config_get_int(name, "LinkKeyType", &linkkey_type)) {
                bt_bdaddr_t bd_addr;
                string_to_bdaddr(name, &bd_addr);
                if (add) {
                    DEV_CLASS dev_class = {0, 0, 0};
                    int cod;
                    int pin_length = 0;
                    if (btif_config_get_int(name, "DevClass", &cod))
                        uint2devclass((UINT32)cod, dev_class);
                    btif_config_get_int(name, "PinLength", &pin_length);
                    BTA_DmAddDevice(bd_addr.address, dev_class, link_key, 0, 0,
                                    (UINT8)linkkey_type, 0, pin_length);

#if BLE_INCLUDED == TRUE
                    if (btif_config_get_int(name, "DevType", &device_type) &&
                        (device_type == BT_DEVICE_TYPE_DUMO) ) {
                        btif_gatts_add_bonded_dev_from_nv(bd_addr.address);
                    }
#endif
                }
                bt_linkkey_file_found = TRUE;
                memcpy(&p_bonded_devices->devices[p_bonded_devices->num_devices++], &bd_addr, sizeof(bt_bdaddr_t));
            } else {
#if (BLE_INCLUDED == TRUE)
                bt_linkkey_file_found = FALSE;
#else
                BTIF_TRACE_ERROR("bounded device:%s, LinkKeyType or PinLength is invalid", name);
#endif
            }
        }
#if (BLE_INCLUDED == TRUE)
        if (!btif_in_fetch_bonded_ble_device(name, add, p_bonded_devices) &&
            !bt_linkkey_file_found) {
            BTIF_TRACE_DEBUG("Remote device:%s, no link key or ble key found", name);
        }
#else
        if(!bt_linkkey_file_found)
            BTIF_TRACE_DEBUG("Remote device:%s, no link key", name);
#endif
    }
    return BT_STATUS_SUCCESS;
}

static void btif_read_le_key(const uint8_t key_type, const size_t key_len, bt_bdaddr_t bd_addr,
                 const uint8_t addr_type, const bool add_key, bool *device_added, bool *key_found)
{
    assert(device_added);
    assert(key_found);

    char buffer[100];
    memset(buffer, 0, sizeof(buffer));

    if (btif_storage_get_ble_bonding_key(&bd_addr, key_type, buffer, key_len) == BT_STATUS_SUCCESS)
    {
        if (add_key)
        {
            BD_ADDR bta_bd_addr;
            bdcpy(bta_bd_addr, bd_addr.address);

            if (!*device_added)
            {
                BTA_DmAddBleDevice(bta_bd_addr, addr_type, BT_DEVICE_TYPE_BLE);
                *device_added = true;
            }

            char bd_str[20] = {0};
            BTIF_TRACE_DEBUG("%s() Adding key type %d for %s", __func__,
                key_type, bdaddr_to_string(&bd_addr, bd_str, sizeof(bd_str)));
            BTA_DmAddBleKey(bta_bd_addr, (tBTA_LE_KEY_VALUE *)buffer, key_type);
        }

        *key_found = true;
    }
}

/*******************************************************************************
 * Functions
 *
 * Functions are synchronous and can be called by both from internal modules
 * such as BTIF_DM and by external entiries from HAL via BTIF_context_switch.
 * For OUT parameters, the caller is expected to provide the memory.
 * Caller is expected to provide a valid pointer to 'property->value' based on
 * the property->type.
 *******************************************************************************/

/*******************************************************************************
**
** Function         btif_split_uuids_string
**
** Description      Internal helper function to split the string of UUIDs
**                  read from the NVRAM to an array
**
** Returns          Number of UUIDs parsed from the supplied string
**
*******************************************************************************/
size_t btif_split_uuids_string(const char *str, bt_uuid_t *p_uuid, size_t max_uuids)
{
    assert(str);
    assert(p_uuid);

    size_t num_uuids = 0;
    while (str && num_uuids < max_uuids)
    {
        bool rc = string_to_uuid(str, p_uuid++);
        if (!rc) break;
        num_uuids++;
        str = strchr(str, ' ');
        if (str) str++;
    }

    return num_uuids;
}

/*******************************************************************************
**
** Function         btif_storage_get_adapter_property
**
** Description      BTIF storage API - Fetches the adapter property->type
**                  from NVRAM and fills property->val.
**                  Caller should provide memory for property->val and
**                  set the property->val
**
** Returns          BT_STATUS_SUCCESS if the fetch was successful,
**                  BT_STATUS_FAIL otherwise
**
*******************************************************************************/
bt_status_t btif_storage_get_adapter_property(bt_property_t *property)
{

    /* Special handling for adapter BD_ADDR and BONDED_DEVICES */
    if (property->type == BT_PROPERTY_BDADDR)
    {
        bt_bdaddr_t *bd_addr = (bt_bdaddr_t*)property->val;
        /* This has been cached in btif. Just fetch it from there */
        memcpy(bd_addr, &btif_local_bd_addr, sizeof(bt_bdaddr_t));
        property->len = sizeof(bt_bdaddr_t);
        return BT_STATUS_SUCCESS;
    }
    else if (property->type == BT_PROPERTY_ADAPTER_BONDED_DEVICES)
    {
        btif_bonded_devices_t bonded_devices;

        btif_in_fetch_bonded_devices(&bonded_devices, 0);

        BTIF_TRACE_DEBUG("%s: Number of bonded devices: %d Property:BT_PROPERTY_ADAPTER_BONDED_DEVICES", __FUNCTION__, bonded_devices.num_devices);

        if (bonded_devices.num_devices > 0)
        {
            property->len = bonded_devices.num_devices * sizeof(bt_bdaddr_t);
            memcpy(property->val, bonded_devices.devices, property->len);
        }

        /* if there are no bonded_devices, then length shall be 0 */
        return BT_STATUS_SUCCESS;
    }
    else if (property->type == BT_PROPERTY_UUIDS)
    {
        /* publish list of local supported services */
        bt_uuid_t *p_uuid = (bt_uuid_t*)property->val;
        uint32_t num_uuids = 0;
        uint32_t i;

        tBTA_SERVICE_MASK service_mask = btif_get_enabled_services_mask();
        LOG_INFO(LOG_TAG, "%s service_mask:0x%x", __FUNCTION__, service_mask);
        for (i=0; i < BTA_MAX_SERVICE_ID; i++)
        {
            /* This should eventually become a function when more services are enabled */
            if (service_mask
                &(tBTA_SERVICE_MASK)(1 << i))
            {
                switch (i)
                {
                    case BTA_HFP_SERVICE_ID:
                        {
                            uuid16_to_uuid128(UUID_SERVCLASS_AG_HANDSFREE,
                                              p_uuid+num_uuids);
                            num_uuids++;
                        }
                    /* intentional fall through: Send both BFP & HSP UUIDs if HFP is enabled */
                    case BTA_HSP_SERVICE_ID:
                        {
                            uuid16_to_uuid128(UUID_SERVCLASS_HEADSET_AUDIO_GATEWAY,
                                              p_uuid+num_uuids);
                            num_uuids++;
                        }break;
                    case BTA_A2DP_SOURCE_SERVICE_ID:
                        {
                            uuid16_to_uuid128(UUID_SERVCLASS_AUDIO_SOURCE,
                                              p_uuid+num_uuids);
                            num_uuids++;
                        }break;
                    case BTA_A2DP_SINK_SERVICE_ID:
                        {
                            uuid16_to_uuid128(UUID_SERVCLASS_AUDIO_SINK,
                                              p_uuid+num_uuids);
                            num_uuids++;
                        }break;
                    case BTA_HFP_HS_SERVICE_ID:
                        {
                            uuid16_to_uuid128(UUID_SERVCLASS_HF_HANDSFREE,
                                              p_uuid+num_uuids);
                            num_uuids++;
                        }break;
                }
            }
        }
        property->len = (num_uuids)*sizeof(bt_uuid_t);
        return BT_STATUS_SUCCESS;
    }

    /* fall through for other properties */
    if(!cfg2prop(NULL, property))
    {
        return btif_dm_get_adapter_property(property);
    }
    return BT_STATUS_SUCCESS;
 }

/*******************************************************************************
**
** Function         btif_storage_set_adapter_property
**
** Description      BTIF storage API - Stores the adapter property
**                  to NVRAM
**
** Returns          BT_STATUS_SUCCESS if the store was successful,
**                  BT_STATUS_FAIL otherwise
**
*******************************************************************************/
bt_status_t btif_storage_set_adapter_property(bt_property_t *property)
{
    return prop2cfg(NULL, property) ? BT_STATUS_SUCCESS : BT_STATUS_FAIL;
}

/*******************************************************************************
**
** Function         btif_storage_get_remote_device_property
**
** Description      BTIF storage API - Fetches the remote device property->type
**                  from NVRAM and fills property->val.
**                  Caller should provide memory for property->val and
**                  set the property->val
**
** Returns          BT_STATUS_SUCCESS if the fetch was successful,
**                  BT_STATUS_FAIL otherwise
**
*******************************************************************************/
bt_status_t btif_storage_get_remote_device_property(bt_bdaddr_t *remote_bd_addr,
                                                    bt_property_t *property)
{
    return cfg2prop(remote_bd_addr, property) ? BT_STATUS_SUCCESS : BT_STATUS_FAIL;
}
/*******************************************************************************
**
** Function         btif_storage_set_remote_device_property
**
** Description      BTIF storage API - Stores the remote device property
**                  to NVRAM
**
** Returns          BT_STATUS_SUCCESS if the store was successful,
**                  BT_STATUS_FAIL otherwise
**
*******************************************************************************/
bt_status_t btif_storage_set_remote_device_property(bt_bdaddr_t *remote_bd_addr,
                                                    bt_property_t *property)
{
    return prop2cfg(remote_bd_addr, property) ? BT_STATUS_SUCCESS : BT_STATUS_FAIL;
}

/*******************************************************************************
**
** Function         btif_storage_add_remote_device
**
** Description      BTIF storage API - Adds a newly discovered device to NVRAM
**                  along with the timestamp. Also, stores the various
**                  properties - RSSI, BDADDR, NAME (if found in EIR)
**
** Returns          BT_STATUS_SUCCESS if the store was successful,
**                  BT_STATUS_FAIL otherwise
**
*******************************************************************************/
bt_status_t btif_storage_add_remote_device(bt_bdaddr_t *remote_bd_addr,
                                           uint32_t num_properties,
                                           bt_property_t *properties)
{
    uint32_t i = 0;
    /* TODO: If writing a property, fails do we go back undo the earlier
     * written properties? */
    for (i=0; i < num_properties; i++)
    {
        /* Ignore the RSSI as this is not stored in DB */
        if (properties[i].type == BT_PROPERTY_REMOTE_RSSI)
            continue;

        /* BD_ADDR for remote device needs special handling as we also store timestamp */
        if (properties[i].type == BT_PROPERTY_BDADDR)
        {
            bt_property_t addr_prop;
            memcpy(&addr_prop, &properties[i], sizeof(bt_property_t));
            addr_prop.type = BT_PROPERTY_REMOTE_DEVICE_TIMESTAMP;
            btif_storage_set_remote_device_property(remote_bd_addr,
                                                    &addr_prop);
        }
        else
        {
            btif_storage_set_remote_device_property(remote_bd_addr,
                                                    &properties[i]);
        }
    }
    return BT_STATUS_SUCCESS;
}

/*******************************************************************************
**
** Function         btif_storage_add_bonded_device
**
** Description      BTIF storage API - Adds the newly bonded device to NVRAM
**                  along with the link-key, Key type and Pin key length
**
** Returns          BT_STATUS_SUCCESS if the store was successful,
**                  BT_STATUS_FAIL otherwise
**
*******************************************************************************/

bt_status_t btif_storage_add_bonded_device(bt_bdaddr_t *remote_bd_addr,
                                           LINK_KEY link_key,
                                           uint8_t key_type,
                                           uint8_t pin_length)
{
    bdstr_t bdstr;
    bdaddr_to_string(remote_bd_addr, bdstr, sizeof(bdstr));
    int ret = btif_config_set_int(bdstr, "LinkKeyType", (int)key_type);
    ret &= btif_config_set_int(bdstr, "PinLength", (int)pin_length);
    ret &= btif_config_set_bin(bdstr, "LinkKey", link_key, sizeof(LINK_KEY));

    if (is_restricted_mode()) {
        BTIF_TRACE_WARNING("%s: '%s' pairing will be removed if unrestricted",
                         __func__, bdstr);
        btif_config_set_int(bdstr, "Restricted", 1);
    }

    /* write bonded info immediately */
    btif_config_flush();
    return ret ? BT_STATUS_SUCCESS : BT_STATUS_FAIL;
}

/*******************************************************************************
**
** Function         btif_storage_remove_bonded_device
**
** Description      BTIF storage API - Deletes the bonded device from NVRAM
**
** Returns          BT_STATUS_SUCCESS if the deletion was successful,
**                  BT_STATUS_FAIL otherwise
**
*******************************************************************************/
bt_status_t btif_storage_remove_bonded_device(bt_bdaddr_t *remote_bd_addr)
{
    bdstr_t bdstr;
    bdaddr_to_string(remote_bd_addr, bdstr, sizeof(bdstr));
    BTIF_TRACE_DEBUG("in bd addr:%s", bdstr);

#if (defined(BLE_INCLUDED) && (BLE_INCLUDED == TRUE))
    btif_storage_remove_ble_bonding_keys(remote_bd_addr);
#endif

    int ret = 1;
    if(btif_config_exist(bdstr, "LinkKeyType"))
        ret &= btif_config_remove(bdstr, "LinkKeyType");
    if(btif_config_exist(bdstr, "PinLength"))
        ret &= btif_config_remove(bdstr, "PinLength");
    if(btif_config_exist(bdstr, "LinkKey"))
        ret &= btif_config_remove(bdstr, "LinkKey");
    /* write bonded info immediately */
    btif_config_flush();
    return ret ? BT_STATUS_SUCCESS : BT_STATUS_FAIL;

}

/*******************************************************************************
**
** Function         btif_storage_is_device_bonded
**
** Description      BTIF storage API - checks if device present in bonded list
**
** Returns          TRUE if the device is bonded
**                  FALSE otherwise
**
*******************************************************************************/
BOOLEAN btif_storage_is_device_bonded(bt_bdaddr_t *remote_bd_addr)
{
    bdstr_t bdstr;
    bdaddr_to_string(remote_bd_addr, bdstr, sizeof(bdstr));
    if((btif_config_exist(bdstr, "LinkKey")) &&
       (btif_config_exist(bdstr, "LinkKeyType")))
        return TRUE;
    else
        return FALSE;
}

/*******************************************************************************
**
** Function         btif_storage_load_bonded_devices
**
** Description      BTIF storage API - Loads all the bonded devices from NVRAM
**                  and adds to the BTA.
**                  Additionally, this API also invokes the adaper_properties_cb
**                  and remote_device_properties_cb for each of the bonded devices.
**
** Returns          BT_STATUS_SUCCESS if successful, BT_STATUS_FAIL otherwise
**
*******************************************************************************/
bt_status_t btif_storage_load_bonded_devices(void)
{
    btif_bonded_devices_t bonded_devices;
    uint32_t i = 0;
    bt_property_t adapter_props[6];
    uint32_t num_props = 0;
    bt_property_t remote_properties[8];
    bt_bdaddr_t addr;
    bt_bdname_t name, alias;
    bt_scan_mode_t mode;
    uint32_t disc_timeout;
    bt_uuid_t local_uuids[BT_MAX_NUM_UUIDS];
    bt_uuid_t remote_uuids[BT_MAX_NUM_UUIDS];

    btif_in_fetch_bonded_devices(&bonded_devices, 1);

    /* Now send the adapter_properties_cb with all adapter_properties */
    {
        memset(adapter_props, 0, sizeof(adapter_props));

        /* BD_ADDR */
        BTIF_STORAGE_GET_ADAPTER_PROP(BT_PROPERTY_BDADDR, &addr, sizeof(addr),
                                      adapter_props[num_props]);
        num_props++;

        /* BD_NAME */
        BTIF_STORAGE_GET_ADAPTER_PROP(BT_PROPERTY_BDNAME, &name, sizeof(name),
                                      adapter_props[num_props]);
        num_props++;

        /* SCAN_MODE */
        /* TODO: At the time of BT on, always report the scan mode as 0 irrespective
         of the scan_mode during the previous enable cycle.
         This needs to be re-visited as part of the app/stack enable sequence
         synchronization */
        mode = BT_SCAN_MODE_NONE;
        adapter_props[num_props].type = BT_PROPERTY_ADAPTER_SCAN_MODE;
        adapter_props[num_props].len = sizeof(mode);
        adapter_props[num_props].val = &mode;
        num_props++;

        /* DISC_TIMEOUT */
        BTIF_STORAGE_GET_ADAPTER_PROP(BT_PROPERTY_ADAPTER_DISCOVERY_TIMEOUT,
                                      &disc_timeout, sizeof(disc_timeout),
                                      adapter_props[num_props]);
        num_props++;

        /* BONDED_DEVICES */
        bt_bdaddr_t *devices_list =
            (bt_bdaddr_t *)osi_malloc(sizeof(bt_bdaddr_t) * bonded_devices.num_devices);
        adapter_props[num_props].type = BT_PROPERTY_ADAPTER_BONDED_DEVICES;
        adapter_props[num_props].len = bonded_devices.num_devices * sizeof(bt_bdaddr_t);
        adapter_props[num_props].val = devices_list;
        for (i=0; i < bonded_devices.num_devices; i++)
        {
            memcpy(devices_list + i, &bonded_devices.devices[i], sizeof(bt_bdaddr_t));
        }
        num_props++;

        /* LOCAL UUIDs */
        BTIF_STORAGE_GET_ADAPTER_PROP(BT_PROPERTY_UUIDS,
                                      local_uuids, sizeof(local_uuids),
                                      adapter_props[num_props]);
        num_props++;

        btif_adapter_properties_evt(BT_STATUS_SUCCESS, num_props, adapter_props);

        osi_free(devices_list);
    }

    BTIF_TRACE_EVENT("%s: %d bonded devices found", __FUNCTION__, bonded_devices.num_devices);

    {
        for (i = 0; i < bonded_devices.num_devices; i++)
        {
            bt_bdaddr_t *p_remote_addr;

            /*
             * TODO: improve handling of missing fields in NVRAM.
             */
            uint32_t cod = 0;
            uint32_t devtype = 0;

            num_props = 0;
            p_remote_addr = &bonded_devices.devices[i];
            memset(remote_properties, 0, sizeof(remote_properties));
            BTIF_STORAGE_GET_REMOTE_PROP(p_remote_addr, BT_PROPERTY_BDNAME,
                                         &name, sizeof(name),
                                         remote_properties[num_props]);
            num_props++;

            BTIF_STORAGE_GET_REMOTE_PROP(p_remote_addr, BT_PROPERTY_REMOTE_FRIENDLY_NAME,
                                         &alias, sizeof(alias),
                                         remote_properties[num_props]);
            num_props++;

            BTIF_STORAGE_GET_REMOTE_PROP(p_remote_addr, BT_PROPERTY_CLASS_OF_DEVICE,
                                         &cod, sizeof(cod),
                                         remote_properties[num_props]);
            num_props++;

            BTIF_STORAGE_GET_REMOTE_PROP(p_remote_addr, BT_PROPERTY_TYPE_OF_DEVICE,
                                         &devtype, sizeof(devtype),
                                         remote_properties[num_props]);
            num_props++;

            BTIF_STORAGE_GET_REMOTE_PROP(p_remote_addr, BT_PROPERTY_UUIDS,
                                         remote_uuids, sizeof(remote_uuids),
                                         remote_properties[num_props]);
            num_props++;

            btif_remote_properties_evt(BT_STATUS_SUCCESS, p_remote_addr,
                                       num_props, remote_properties);
        }
    }
    return BT_STATUS_SUCCESS;
}

#if (BLE_INCLUDED == TRUE)

/*******************************************************************************
**
** Function         btif_storage_add_ble_bonding_key
**
** Description      BTIF storage API - Adds the newly bonded device to NVRAM
**                  along with the ble-key, Key type and Pin key length
**
** Returns          BT_STATUS_SUCCESS if the store was successful,
**                  BT_STATUS_FAIL otherwise
**
*******************************************************************************/

bt_status_t btif_storage_add_ble_bonding_key(bt_bdaddr_t *remote_bd_addr,
                                           char *key,
                                           UINT8 key_type,
                                           UINT8 key_length)
{
    bdstr_t bdstr;
    bdaddr_to_string(remote_bd_addr, bdstr, sizeof(bdstr));
    const char* name;
    switch(key_type)
    {
        case BTIF_DM_LE_KEY_PENC:
            name = "LE_KEY_PENC";
            break;
        case BTIF_DM_LE_KEY_PID:
            name = "LE_KEY_PID";
            break;
        case BTIF_DM_LE_KEY_PCSRK:
            name = "LE_KEY_PCSRK";
            break;
        case BTIF_DM_LE_KEY_LENC:
            name = "LE_KEY_LENC";
            break;
        case BTIF_DM_LE_KEY_LCSRK:
            name = "LE_KEY_LCSRK";
            break;
        case BTIF_DM_LE_KEY_LID:
            name = "LE_KEY_LID";
            break;
        default:
            return BT_STATUS_FAIL;
    }
    int ret = btif_config_set_bin(bdstr, name, (const uint8_t *)key, key_length);
    btif_config_save();
    return ret ? BT_STATUS_SUCCESS : BT_STATUS_FAIL;
}

/*******************************************************************************
**
** Function         btif_storage_get_ble_bonding_key
**
** Description
**
** Returns          BT_STATUS_SUCCESS if the fetch was successful,
**                  BT_STATUS_FAIL otherwise
**
*******************************************************************************/
bt_status_t btif_storage_get_ble_bonding_key(bt_bdaddr_t *remote_bd_addr,
                                             UINT8 key_type,
                                             char *key_value,
                                             int key_length)
{
    bdstr_t bdstr;
    bdaddr_to_string(remote_bd_addr, bdstr, sizeof(bdstr));
    const char* name;
    switch(key_type)
    {
        case BTIF_DM_LE_KEY_PENC:
            name = "LE_KEY_PENC";
            break;
        case BTIF_DM_LE_KEY_PID:
            name = "LE_KEY_PID";
            break;
        case BTIF_DM_LE_KEY_PCSRK:
            name = "LE_KEY_PCSRK";
            break;
        case BTIF_DM_LE_KEY_LENC:
            name = "LE_KEY_LENC";
            break;
        case BTIF_DM_LE_KEY_LCSRK:
            name = "LE_KEY_LCSRK";
            break;
        case BTIF_DM_LE_KEY_LID:
            name =  "LE_KEY_LID";
        default:
            return BT_STATUS_FAIL;
    }
    size_t length = key_length;
    int ret = btif_config_get_bin(bdstr, name, (uint8_t *)key_value, &length);
    return ret ? BT_STATUS_SUCCESS : BT_STATUS_FAIL;

}

/*******************************************************************************
**
** Function         btif_storage_remove_ble_keys
**
** Description      BTIF storage API - Deletes the bonded device from NVRAM
**
** Returns          BT_STATUS_SUCCESS if the deletion was successful,
**                  BT_STATUS_FAIL otherwise
**
*******************************************************************************/
bt_status_t btif_storage_remove_ble_bonding_keys(bt_bdaddr_t *remote_bd_addr)
{
    bdstr_t bdstr;
    bdaddr_to_string(remote_bd_addr, bdstr, sizeof(bdstr));
    BTIF_TRACE_DEBUG(" %s in bd addr:%s",__FUNCTION__, bdstr);
    int ret = 1;
    if(btif_config_exist(bdstr, "LE_KEY_PENC"))
        ret &= btif_config_remove(bdstr, "LE_KEY_PENC");
    if(btif_config_exist(bdstr, "LE_KEY_PID"))
        ret &= btif_config_remove(bdstr, "LE_KEY_PID");
    if(btif_config_exist(bdstr, "LE_KEY_PCSRK"))
        ret &= btif_config_remove(bdstr, "LE_KEY_PCSRK");
    if(btif_config_exist(bdstr, "LE_KEY_LENC"))
        ret &= btif_config_remove(bdstr, "LE_KEY_LENC");
    if(btif_config_exist(bdstr, "LE_KEY_LCSRK"))
        ret &= btif_config_remove(bdstr, "LE_KEY_LCSRK");
    btif_config_save();
    return ret ? BT_STATUS_SUCCESS : BT_STATUS_FAIL;
}

/*******************************************************************************
**
** Function         btif_storage_add_ble_local_key
**
** Description      BTIF storage API - Adds the ble key to NVRAM
**
** Returns          BT_STATUS_SUCCESS if the store was successful,
**                  BT_STATUS_FAIL otherwise
**
*******************************************************************************/
bt_status_t btif_storage_add_ble_local_key(char *key,
                                           uint8_t key_type,
                                           uint8_t key_length)
{
    const char* name;
    switch(key_type)
    {
        case BTIF_DM_LE_LOCAL_KEY_IR:
            name = "LE_LOCAL_KEY_IR";
            break;
        case BTIF_DM_LE_LOCAL_KEY_IRK:
            name = "LE_LOCAL_KEY_IRK";
            break;
        case BTIF_DM_LE_LOCAL_KEY_DHK:
            name = "LE_LOCAL_KEY_DHK";
            break;
        case BTIF_DM_LE_LOCAL_KEY_ER:
            name = "LE_LOCAL_KEY_ER";
            break;
        default:
            return BT_STATUS_FAIL;
    }
    int ret = btif_config_set_bin("Adapter", name, (const uint8_t *)key, key_length);
    btif_config_save();
    return ret ? BT_STATUS_SUCCESS : BT_STATUS_FAIL;
}

/*******************************************************************************
**
** Function         btif_storage_get_ble_local_key
**
** Description
**
** Returns          BT_STATUS_SUCCESS if the fetch was successful,
**                  BT_STATUS_FAIL otherwise
**
*******************************************************************************/
bt_status_t btif_storage_get_ble_local_key(UINT8 key_type,
                                           char *key_value,
                                           int key_length)
{
    const char* name;
    switch(key_type)
    {
        case BTIF_DM_LE_LOCAL_KEY_IR:
            name = "LE_LOCAL_KEY_IR";
            break;
        case BTIF_DM_LE_LOCAL_KEY_IRK:
            name = "LE_LOCAL_KEY_IRK";
            break;
        case BTIF_DM_LE_LOCAL_KEY_DHK:
            name = "LE_LOCAL_KEY_DHK";
            break;
        case BTIF_DM_LE_LOCAL_KEY_ER:
            name = "LE_LOCAL_KEY_ER";
            break;
        default:
            return BT_STATUS_FAIL;
    }
    size_t length = key_length;
    int ret = btif_config_get_bin("Adapter", name, (uint8_t *)key_value, &length);
    return ret ? BT_STATUS_SUCCESS : BT_STATUS_FAIL;
}

/*******************************************************************************
**
** Function         btif_storage_remove_ble_local_keys
**
** Description      BTIF storage API - Deletes the bonded device from NVRAM
**
** Returns          BT_STATUS_SUCCESS if the deletion was successful,
**                  BT_STATUS_FAIL otherwise
**
*******************************************************************************/
bt_status_t btif_storage_remove_ble_local_keys(void)
{
    int ret = 1;
    if(btif_config_exist("Adapter", "LE_LOCAL_KEY_IR"))
        ret &= btif_config_remove("Adapter", "LE_LOCAL_KEY_IR");
    if(btif_config_exist("Adapter", "LE_LOCAL_KEY_IRK"))
        ret &= btif_config_remove("Adapter", "LE_LOCAL_KEY_IRK");
    if(btif_config_exist("Adapter", "LE_LOCAL_KEY_DHK"))
        ret &= btif_config_remove("Adapter", "LE_LOCAL_KEY_DHK");
    if(btif_config_exist("Adapter", "LE_LOCAL_KEY_ER"))
        ret &= btif_config_remove("Adapter", "LE_LOCAL_KEY_ER");
    btif_config_save();
    return ret ? BT_STATUS_SUCCESS : BT_STATUS_FAIL;
}

static bt_status_t btif_in_fetch_bonded_ble_device(const char *remote_bd_addr, int add, btif_bonded_devices_t *p_bonded_devices)
{
    int device_type;
    int addr_type;
    bt_bdaddr_t bd_addr;
    BD_ADDR bta_bd_addr;
    bool device_added = false;
    bool key_found = false;

    if (!btif_config_get_int(remote_bd_addr, "DevType", &device_type))
        return BT_STATUS_FAIL;

    if ((device_type & BT_DEVICE_TYPE_BLE) == BT_DEVICE_TYPE_BLE)
    {
        BTIF_TRACE_DEBUG("%s Found a LE device: %s", __func__, remote_bd_addr);

        string_to_bdaddr(remote_bd_addr, &bd_addr);
        bdcpy(bta_bd_addr, bd_addr.address);

        if (btif_storage_get_remote_addr_type(&bd_addr, &addr_type) != BT_STATUS_SUCCESS)
        {
            addr_type = BLE_ADDR_PUBLIC;
            btif_storage_set_remote_addr_type(&bd_addr, BLE_ADDR_PUBLIC);
        }

        btif_read_le_key(BTIF_DM_LE_KEY_PENC, sizeof(tBTM_LE_PENC_KEYS),
                         bd_addr, addr_type, add, &device_added, &key_found);

        btif_read_le_key(BTIF_DM_LE_KEY_PID, sizeof(tBTM_LE_PID_KEYS),
                         bd_addr, addr_type, add, &device_added, &key_found);

        btif_read_le_key(BTIF_DM_LE_KEY_LID, sizeof(tBTM_LE_PID_KEYS),
                         bd_addr, addr_type, add, &device_added, &key_found);

        btif_read_le_key(BTIF_DM_LE_KEY_PCSRK, sizeof(tBTM_LE_PCSRK_KEYS),
                         bd_addr, addr_type, add, &device_added, &key_found);

        btif_read_le_key(BTIF_DM_LE_KEY_LENC, sizeof(tBTM_LE_LENC_KEYS),
                         bd_addr, addr_type, add, &device_added, &key_found);

        btif_read_le_key(BTIF_DM_LE_KEY_LCSRK, sizeof(tBTM_LE_LCSRK_KEYS),
                         bd_addr, addr_type, add, &device_added, &key_found);

        // Fill in the bonded devices
        if (device_added)
        {
            memcpy(&p_bonded_devices->devices[p_bonded_devices->num_devices++], &bd_addr, sizeof(bt_bdaddr_t));
            btif_gatts_add_bonded_dev_from_nv(bta_bd_addr);
        }

        if (key_found)
            return BT_STATUS_SUCCESS;
    }
    return BT_STATUS_FAIL;
}

bt_status_t btif_storage_set_remote_addr_type(bt_bdaddr_t *remote_bd_addr,
                                              UINT8 addr_type)
{
    bdstr_t bdstr;
    bdaddr_to_string(remote_bd_addr, bdstr, sizeof(bdstr));
    int ret = btif_config_set_int(bdstr, "AddrType", (int)addr_type);
    return ret ? BT_STATUS_SUCCESS : BT_STATUS_FAIL;
}

/*******************************************************************************
**
** Function         btif_storage_get_remote_addr_type
**
** Description      BTIF storage API - Fetches the remote addr type
**
** Returns          BT_STATUS_SUCCESS if the fetch was successful,
**                  BT_STATUS_FAIL otherwise
**
*******************************************************************************/
bt_status_t btif_storage_get_remote_addr_type(bt_bdaddr_t *remote_bd_addr,
                                              int*addr_type)
{
    bdstr_t bdstr;
    bdaddr_to_string(remote_bd_addr, bdstr, sizeof(bdstr));
    int ret = btif_config_get_int(bdstr, "AddrType", addr_type);
    return ret ? BT_STATUS_SUCCESS : BT_STATUS_FAIL;
}
#endif

#if (defined BTA_HH_INCLUDED && BTA_HH_INCLUDED == TRUE)
/*******************************************************************************
**
** Function         btif_storage_add_hid_device_info
**
** Description      BTIF storage API - Adds the hid information of bonded hid devices-to NVRAM
**
** Returns          BT_STATUS_SUCCESS if the store was successful,
**                  BT_STATUS_FAIL otherwise
**
*******************************************************************************/

bt_status_t btif_storage_add_hid_device_info(bt_bdaddr_t *remote_bd_addr,
                                                    UINT16 attr_mask, UINT8 sub_class,
                                                    UINT8 app_id, UINT16 vendor_id,
                                                    UINT16 product_id, UINT16 version,
                                                    UINT8 ctry_code, UINT16 ssr_max_latency,
                                                    UINT16 ssr_min_tout, UINT16 dl_len, UINT8 *dsc_list)
{
    bdstr_t bdstr;
    BTIF_TRACE_DEBUG("btif_storage_add_hid_device_info:");
    bdaddr_to_string(remote_bd_addr, bdstr, sizeof(bdstr));
    btif_config_set_int(bdstr, "HidAttrMask", attr_mask);
    btif_config_set_int(bdstr, "HidSubClass", sub_class);
    btif_config_set_int(bdstr, "HidAppId", app_id);
    btif_config_set_int(bdstr, "HidVendorId", vendor_id);
    btif_config_set_int(bdstr, "HidProductId", product_id);
    btif_config_set_int(bdstr, "HidVersion", version);
    btif_config_set_int(bdstr, "HidCountryCode", ctry_code);
    btif_config_set_int(bdstr, "HidSSRMaxLatency", ssr_max_latency);
    btif_config_set_int(bdstr, "HidSSRMinTimeout", ssr_min_tout);
    if(dl_len > 0)
        btif_config_set_bin(bdstr, "HidDescriptor", dsc_list, dl_len);
    btif_config_save();
    return BT_STATUS_SUCCESS;
}

/*******************************************************************************
**
** Function         btif_storage_load_bonded_hid_info
**
** Description      BTIF storage API - Loads hid info for all the bonded devices from NVRAM
**                  and adds those devices  to the BTA_HH.
**
** Returns          BT_STATUS_SUCCESS if successful, BT_STATUS_FAIL otherwise
**
*******************************************************************************/
bt_status_t btif_storage_load_bonded_hid_info(void)
{
    bt_bdaddr_t bd_addr;
    tBTA_HH_DEV_DSCP_INFO dscp_info;
    uint16_t attr_mask;
    uint8_t  sub_class;
    uint8_t  app_id;

    memset(&dscp_info, 0, sizeof(dscp_info));
    for (const btif_config_section_iter_t *iter = btif_config_section_begin(); iter != btif_config_section_end(); iter = btif_config_section_next(iter)) {
        const char *name = btif_config_section_name(iter);
        if (!string_is_bdaddr(name))
            continue;

        BTIF_TRACE_DEBUG("Remote device:%s", name);
        int value;
        if(btif_in_fetch_bonded_device(name) == BT_STATUS_SUCCESS)
        {
            if(btif_config_get_int(name, "HidAttrMask", &value))
            {
                attr_mask = (uint16_t)value;

                btif_config_get_int(name, "HidSubClass", &value);
                sub_class = (uint8_t)value;

                btif_config_get_int(name, "HidAppId", &value);
                app_id = (uint8_t)value;

                btif_config_get_int(name, "HidVendorId", &value);
                dscp_info.vendor_id = (uint16_t) value;

                btif_config_get_int(name, "HidProductId", &value);
                dscp_info.product_id = (uint16_t) value;

                btif_config_get_int(name, "HidVersion", &value);
                dscp_info.version = (uint8_t) value;

                btif_config_get_int(name, "HidCountryCode", &value);
                dscp_info.ctry_code = (uint8_t) value;

                value = 0;
                btif_config_get_int(name, "HidSSRMaxLatency", &value);
                dscp_info.ssr_max_latency = (uint16_t) value;

                value = 0;
                btif_config_get_int(name, "HidSSRMinTimeout", &value);
                dscp_info.ssr_min_tout = (uint16_t) value;

                size_t len = btif_config_get_bin_length(name, "HidDescriptor");
                if(len > 0)
                {
                    dscp_info.descriptor.dl_len = (uint16_t)len;
                    dscp_info.descriptor.dsc_list = (uint8_t*)alloca(len);
                    btif_config_get_bin(name, "HidDescriptor", (uint8_t *)dscp_info.descriptor.dsc_list, &len);
                }
                string_to_bdaddr(name, &bd_addr);
                // add extracted information to BTA HH
                if (btif_hh_add_added_dev(bd_addr,attr_mask))
                {
                    BTA_HhAddDev(bd_addr.address, attr_mask, sub_class,
                            app_id, dscp_info);
                }
            }
        }
        else
        {
            if(btif_config_get_int(name, "HidAttrMask", &value))
            {
                btif_storage_remove_hid_info(&bd_addr);
                string_to_bdaddr(name, &bd_addr);
            }
        }
    }

    return BT_STATUS_SUCCESS;
}

/*******************************************************************************
**
** Function         btif_storage_remove_hid_info
**
** Description      BTIF storage API - Deletes the bonded hid device info from NVRAM
**
** Returns          BT_STATUS_SUCCESS if the deletion was successful,
**                  BT_STATUS_FAIL otherwise
**
*******************************************************************************/
bt_status_t btif_storage_remove_hid_info(bt_bdaddr_t *remote_bd_addr)
{
    bdstr_t bdstr;
    bdaddr_to_string(remote_bd_addr, bdstr, sizeof(bdstr));

    btif_config_remove(bdstr, "HidAttrMask");
    btif_config_remove(bdstr, "HidSubClass");
    btif_config_remove(bdstr, "HidAppId");
    btif_config_remove(bdstr, "HidVendorId");
    btif_config_remove(bdstr, "HidProductId");
    btif_config_remove(bdstr, "HidVersion");
    btif_config_remove(bdstr, "HidCountryCode");
    btif_config_remove(bdstr, "HidSSRMaxLatency");
    btif_config_remove(bdstr, "HidSSRMinTimeout");
    btif_config_remove(bdstr, "HidDescriptor");
    btif_config_save();
    return BT_STATUS_SUCCESS;
}
#endif

/*******************************************************************************
**
** Function         btif_storage_read_hl_apps_cb
**
** Description      BTIF storage API - Read HL application control block from NVRAM
**
** Returns          BT_STATUS_SUCCESS if the operation was successful,
**                  BT_STATUS_FAIL otherwise
**
*******************************************************************************/
bt_status_t btif_storage_read_hl_apps_cb(char *value, int value_size)
{
    bt_status_t bt_status = BT_STATUS_SUCCESS;

    if (!btif_config_exist(BTIF_STORAGE_HL_APP, BTIF_STORAGE_HL_APP_CB)) {
        memset(value, 0, value_size);
        if (!btif_config_set_bin(BTIF_STORAGE_HL_APP,BTIF_STORAGE_HL_APP_CB,
                             (const uint8_t *)value, value_size)) {
            bt_status = BT_STATUS_FAIL;
        } else {
            btif_config_save();
        }
    } else {
        size_t read_size = value_size;
        if (!btif_config_get_bin(BTIF_STORAGE_HL_APP, BTIF_STORAGE_HL_APP_CB,
                             (uint8_t *)value, &read_size)) {
            bt_status = BT_STATUS_FAIL;
        } else {
            if (read_size != (size_t)value_size) {
                BTIF_TRACE_ERROR("%s  value_size=%d read_size=%d",
                                  __FUNCTION__, value_size, read_size);
                bt_status = BT_STATUS_FAIL;
            }
        }
    }

    BTIF_TRACE_DEBUG("%s  status=%d value_size=%d", __FUNCTION__, bt_status, value_size);
    return bt_status;
}

/*******************************************************************************
**
** Function         btif_storage_is_restricted_device
**
** Description      BTIF storage API - checks if this device is a restricted device
**
** Returns          TRUE  if the device is labeled as restricted
**                  FALSE otherwise
**
*******************************************************************************/
BOOLEAN btif_storage_is_restricted_device(const bt_bdaddr_t *remote_bd_addr)
{
    bdstr_t bdstr;
    bdaddr_to_string(remote_bd_addr, bdstr, sizeof(bdstr));

    return btif_config_exist(bdstr, "Restricted");
}
