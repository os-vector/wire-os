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
 *  Filename:      btif_hf_client.c
 *
 *  Description:   Handsfree Profile (HF role) Bluetooth Interface
 *
 *
 ***********************************************************************************/

#define LOG_TAG "bt_btif_hfc"

#include <stdlib.h>
#include <string.h>

#include <hardware/bluetooth.h>
#include <hardware/bt_hf_client.h>

#include "bt_utils.h"
#include "bta_hf_client_api.h"
#include "btcore/include/bdaddr.h"
#include "btif_common.h"
#include "btif_profile_queue.h"
#include "btif_util.h"
#include "osi/include/properties.h"
#include "hardware/bt_hf_client_vendor.h"

/************************************************************************************
**  Constants & Macros
************************************************************************************/

#ifndef BTIF_HF_CLIENT_SERVICE_NAME
#define BTIF_HF_CLIENT_SERVICE_NAME ("Handsfree")
#endif

#ifndef BTIF_HF_CLIENT_SECURITY
#define BTIF_HF_CLIENT_SECURITY    (BTA_SEC_AUTHENTICATE | BTA_SEC_ENCRYPT)
#endif

#ifndef BTIF_HF_CLIENT_FEATURES
#define BTIF_HF_CLIENT_FEATURES   ( BTA_HF_CLIENT_FEAT_ECNR  | \
                                    BTA_HF_CLIENT_FEAT_3WAY  | \
                                    BTA_HF_CLIENT_FEAT_CLI   | \
                                    BTA_HF_CLIENT_FEAT_VREC  | \
                                    BTA_HF_CLIENT_FEAT_VOL   | \
                                    BTA_HF_CLIENT_FEAT_ECS   | \
                                    BTA_HF_CLIENT_FEAT_ECC   | \
                                    BTA_HF_CLIENT_FEAT_CODEC)
#endif

/************************************************************************************
**  Local type definitions
************************************************************************************/

/************************************************************************************
**  Static variables
************************************************************************************/
static bthf_client_callbacks_t *bt_hf_client_callbacks = NULL;
static bthf_client_vendor_callbacks_t *bt_hf_client_vendor_callbacks = NULL;
static UINT32 btif_hf_client_features = 0;

char btif_hf_client_version[PROPERTY_VALUE_MAX];

#define CHECK_BTHF_CLIENT_INIT() if (bt_hf_client_callbacks == NULL)\
    {\
        BTIF_TRACE_WARNING("BTHF CLIENT: %s: not initialized", __FUNCTION__);\
        return BT_STATUS_NOT_READY;\
    }\
    else\
    {\
        BTIF_TRACE_EVENT("BTHF CLIENT: %s", __FUNCTION__);\
    }

#define CHECK_BTHF_CLIENT_SLC_CONNECTED() if (bt_hf_client_callbacks == NULL)\
    {\
        BTIF_TRACE_WARNING("BTHF CLIENT: %s: not initialized", __FUNCTION__);\
        return BT_STATUS_NOT_READY;\
    }\
    else if (btif_hf_client_cb.state != BTHF_CLIENT_CONNECTION_STATE_SLC_CONNECTED)\
    {\
        BTIF_TRACE_WARNING("BTHF CLIENT: %s: SLC connection not up. state=%s",\
                           __FUNCTION__, \
                           dump_hf_conn_state(btif_hf_client_cb.state));\
        return BT_STATUS_NOT_READY;\
    }\
    else\
    {\
        BTIF_TRACE_EVENT("BTHF CLIENT: %s", __FUNCTION__);\
    }

/* BTIF-HF control block to map bdaddr to BTA handle */
typedef struct
{
    UINT16                          handle;
    bt_bdaddr_t                     connected_bda;
    bthf_client_connection_state_t  state;
    bthf_client_vr_state_t          vr_state;
    tBTA_HF_CLIENT_PEER_FEAT        peer_feat;
    tBTA_HF_CLIENT_CHLD_FEAT        chld_feat;
} btif_hf_client_cb_t;

static btif_hf_client_cb_t btif_hf_client_cb;

/************************************************************************************
**  Static functions
************************************************************************************/

/*******************************************************************************
**
** Function        btif_in_hf_client_generic_evt
**
** Description     Processes generic events to be sent to JNI that are not triggered from the BTA.
**                 Always runs in BTIF context
**
** Returns          void
**
*******************************************************************************/
static void btif_in_hf_client_generic_evt(UINT16 event, char *p_param)
{
    UNUSED(p_param);

    BTIF_TRACE_EVENT("%s: event=%d", __FUNCTION__, event);
    switch (event) {
        case BTIF_HF_CLIENT_CB_AUDIO_CONNECTING:
        {
            HAL_CBACK(bt_hf_client_callbacks, audio_state_cb, (bthf_client_audio_state_t)BTHF_AUDIO_STATE_CONNECTING,
                      &btif_hf_client_cb.connected_bda);
        } break;
        default:
        {
            BTIF_TRACE_WARNING("%s : Unknown event 0x%x", __FUNCTION__, event);
        }
        break;
    }
}

/************************************************************************************
**  Externs
************************************************************************************/

/************************************************************************************
**  Functions
************************************************************************************/

static void clear_state(void)
{
    memset(&btif_hf_client_cb, 0, sizeof(btif_hf_client_cb_t));
}

static BOOLEAN is_connected(bt_bdaddr_t *bd_addr)
{
    if (((btif_hf_client_cb.state == BTHF_CLIENT_CONNECTION_STATE_CONNECTED) ||
            (btif_hf_client_cb.state == BTHF_CLIENT_CONNECTION_STATE_SLC_CONNECTED))&&
        ((bd_addr == NULL) || (bdcmp(bd_addr->address, btif_hf_client_cb.connected_bda.address) == 0)))
        return TRUE;
    return FALSE;
}

/*****************************************************************************
**   Section name (Group of functions)
*****************************************************************************/

/*****************************************************************************
**
**   btif hf api functions (no context switch)
**
*****************************************************************************/

/*******************************************************************************
**
** Function         btif_hf_client_init
**
** Description     initializes the hf interface
**
** Returns         bt_status_t
**
*******************************************************************************/
static bt_status_t init( bthf_client_callbacks_t* callbacks )
{
    BTIF_TRACE_EVENT("%s", __FUNCTION__);

    bt_hf_client_callbacks = callbacks;

    btif_enable_service(BTA_HFP_HS_SERVICE_ID);

    clear_state();

    return BT_STATUS_SUCCESS;
}

/*******************************************************************************
**
** Function         btif_hf_client_vendor_init
**
** Description     initializes the hf client vendor interface
**
** Returns         bt_status_t
**
*******************************************************************************/
static bt_status_t init_vendor( bthf_client_vendor_callbacks_t* callbacks )
{
    BTIF_TRACE_EVENT("%s", __FUNCTION__);

    bt_hf_client_vendor_callbacks = callbacks;

    return BT_STATUS_SUCCESS;
}

/*******************************************************************************
**
** Function         connect
**
** Description     connect to audio gateway
**
** Returns         bt_status_t
**
*******************************************************************************/
static bt_status_t connect_int( bt_bdaddr_t *bd_addr, uint16_t uuid )
{
    if (is_connected(bd_addr))
        return BT_STATUS_BUSY;

    btif_hf_client_cb.state = BTHF_CLIENT_CONNECTION_STATE_CONNECTING;
    bdcpy(btif_hf_client_cb.connected_bda.address, bd_addr->address);

    BTA_HfClientOpen(btif_hf_client_cb.handle, btif_hf_client_cb.connected_bda.address,
               BTIF_HF_CLIENT_SECURITY);

    return BT_STATUS_SUCCESS;
}

static bt_status_t connect( bt_bdaddr_t *bd_addr )
{
    BTIF_TRACE_EVENT("HFP Client version is  %s", btif_hf_client_version);
    CHECK_BTHF_CLIENT_INIT();
    return btif_queue_connect(UUID_SERVCLASS_HF_HANDSFREE, bd_addr, connect_int);

}

/*******************************************************************************
**
** Function         disconnect
**
** Description      disconnect from audio gateway
**
** Returns         bt_status_t
**
*******************************************************************************/
static bt_status_t disconnect( bt_bdaddr_t *bd_addr )
{
    CHECK_BTHF_CLIENT_INIT();

    if (is_connected(bd_addr))
    {
        BTA_HfClientClose(btif_hf_client_cb.handle);
        return BT_STATUS_SUCCESS;
    }

    return BT_STATUS_FAIL;
}

/*******************************************************************************
**
** Function         connect_audio
**
** Description     create an audio connection
**
** Returns         bt_status_t
**
*******************************************************************************/
static bt_status_t connect_audio( bt_bdaddr_t *bd_addr )
{
    CHECK_BTHF_CLIENT_SLC_CONNECTED();

    if (is_connected(bd_addr))
    {
        if ((BTIF_HF_CLIENT_FEATURES & BTA_HF_CLIENT_FEAT_CODEC) &&
                (btif_hf_client_cb.peer_feat & BTA_HF_CLIENT_PEER_CODEC))
        {
            BTA_HfClientSendAT(btif_hf_client_cb.handle, BTA_HF_CLIENT_AT_CMD_BCC, 0, 0, NULL);
        }
        else
        {
            BTA_HfClientAudioOpen(btif_hf_client_cb.handle);
        }

        /* Inform the application that the audio connection has been initiated successfully */
        btif_transfer_context(btif_in_hf_client_generic_evt, BTIF_HF_CLIENT_CB_AUDIO_CONNECTING,
                              (char *)bd_addr, sizeof(bt_bdaddr_t), NULL);
        return BT_STATUS_SUCCESS;
    }

    return BT_STATUS_FAIL;
}

/*******************************************************************************
**
** Function         disconnect_audio
**
** Description      close the audio connection
**
** Returns         bt_status_t
**
*******************************************************************************/
static bt_status_t disconnect_audio( bt_bdaddr_t *bd_addr )
{
    CHECK_BTHF_CLIENT_SLC_CONNECTED();

    if (is_connected(bd_addr))
    {
        BTA_HfClientAudioClose(btif_hf_client_cb.handle);
        return BT_STATUS_SUCCESS;
    }

    return BT_STATUS_FAIL;
}

/*******************************************************************************
**
** Function         start_voice_recognition
**
** Description      start voice recognition
**
** Returns          bt_status_t
**
*******************************************************************************/
static bt_status_t start_voice_recognition()
{
    CHECK_BTHF_CLIENT_SLC_CONNECTED();

    if (btif_hf_client_cb.peer_feat & BTA_HF_CLIENT_PEER_FEAT_VREC)
    {
        BTA_HfClientSendAT(btif_hf_client_cb.handle, BTA_HF_CLIENT_AT_CMD_BVRA, 1, 0, NULL);

        return BT_STATUS_SUCCESS;
    }

    return BT_STATUS_UNSUPPORTED;
}

/*******************************************************************************
**
** Function         stop_voice_recognition
**
** Description      stop voice recognition
**
** Returns          bt_status_t
**
*******************************************************************************/
static bt_status_t stop_voice_recognition()
{
    CHECK_BTHF_CLIENT_SLC_CONNECTED();

    if (btif_hf_client_cb.peer_feat & BTA_HF_CLIENT_PEER_FEAT_VREC)
    {
        BTA_HfClientSendAT(btif_hf_client_cb.handle, BTA_HF_CLIENT_AT_CMD_BVRA, 0, 0, NULL);

        return BT_STATUS_SUCCESS;
    }

    return BT_STATUS_UNSUPPORTED;
}

/*******************************************************************************
**
** Function         volume_control
**
** Description      volume control
**
** Returns          bt_status_t
**
*******************************************************************************/
static bt_status_t volume_control(bthf_client_volume_type_t type, int volume)
{
    CHECK_BTHF_CLIENT_SLC_CONNECTED();

    switch (type)
    {
        case BTHF_CLIENT_VOLUME_TYPE_SPK:
            BTA_HfClientSendAT(btif_hf_client_cb.handle, BTA_HF_CLIENT_AT_CMD_VGS, volume, 0, NULL);
            break;
        case BTHF_CLIENT_VOLUME_TYPE_MIC:
            BTA_HfClientSendAT(btif_hf_client_cb.handle, BTA_HF_CLIENT_AT_CMD_VGM, volume, 0, NULL);
            break;
        default:
            return BT_STATUS_UNSUPPORTED;
    }

    return BT_STATUS_SUCCESS;
}

/*******************************************************************************
**
** Function         dial
**
** Description      place a call
**
** Returns          bt_status_t
**
*******************************************************************************/
static bt_status_t dial(const char *number)
{
    CHECK_BTHF_CLIENT_SLC_CONNECTED();

    if (strcmp(number,"") != 0)
    {
        BTA_HfClientSendAT(btif_hf_client_cb.handle, BTA_HF_CLIENT_AT_CMD_ATD, 0, 0, number);
    }
    else
    {
        BTA_HfClientSendAT(btif_hf_client_cb.handle, BTA_HF_CLIENT_AT_CMD_BLDN, 0, 0, NULL);
    }

    return BT_STATUS_SUCCESS;
}

/*******************************************************************************
**
** Function         dial_memory
**
** Description      place a call with number specified by location (speed dial)
**
** Returns          bt_status_t
**
*******************************************************************************/
static bt_status_t dial_memory(int location)
{
    CHECK_BTHF_CLIENT_SLC_CONNECTED();

    BTA_HfClientSendAT(btif_hf_client_cb.handle, BTA_HF_CLIENT_AT_CMD_ATD, location, 0, NULL);

    return BT_STATUS_SUCCESS;
}

/*******************************************************************************
**
** Function         handle_call_action
**
** Description      handle specified call related action
**
** Returns          bt_status_t
**
*******************************************************************************/
static bt_status_t handle_call_action(bthf_client_call_action_t action, int idx)
{
    CHECK_BTHF_CLIENT_SLC_CONNECTED();

    switch (action)
    {
    case BTHF_CLIENT_CALL_ACTION_CHLD_0:
        if (btif_hf_client_cb.chld_feat & BTA_HF_CLIENT_CHLD_REL)
        {
            BTA_HfClientSendAT(btif_hf_client_cb.handle, BTA_HF_CLIENT_AT_CMD_CHLD, 0, 0, NULL);
            break;
        }
        return BT_STATUS_UNSUPPORTED;
    case BTHF_CLIENT_CALL_ACTION_CHLD_1:
        // CHLD 1 is mandatory for 3 way calling
        if (btif_hf_client_cb.peer_feat & BTA_HF_CLIENT_PEER_FEAT_3WAY)
        {
            BTA_HfClientSendAT(btif_hf_client_cb.handle, BTA_HF_CLIENT_AT_CMD_CHLD, 1, 0, NULL);
            break;
        }
        return BT_STATUS_UNSUPPORTED;
    case BTHF_CLIENT_CALL_ACTION_CHLD_2:
        // CHLD 2 is mandatory for 3 way calling
        if (btif_hf_client_cb.peer_feat & BTA_HF_CLIENT_PEER_FEAT_3WAY)
        {
            BTA_HfClientSendAT(btif_hf_client_cb.handle, BTA_HF_CLIENT_AT_CMD_CHLD, 2, 0, NULL);
            break;
        }
        return BT_STATUS_UNSUPPORTED;
    case BTHF_CLIENT_CALL_ACTION_CHLD_3:
        if (btif_hf_client_cb.chld_feat & BTA_HF_CLIENT_CHLD_MERGE)
        {
            BTA_HfClientSendAT(btif_hf_client_cb.handle, BTA_HF_CLIENT_AT_CMD_CHLD, 3, 0, NULL);
            break;
        }
        return BT_STATUS_UNSUPPORTED;
    case BTHF_CLIENT_CALL_ACTION_CHLD_4:
        if (btif_hf_client_cb.chld_feat & BTA_HF_CLIENT_CHLD_MERGE_DETACH)
        {
            BTA_HfClientSendAT(btif_hf_client_cb.handle, BTA_HF_CLIENT_AT_CMD_CHLD, 4, 0, NULL);
            break;
        }
        return BT_STATUS_UNSUPPORTED;
    case BTHF_CLIENT_CALL_ACTION_CHLD_1x:
        if (btif_hf_client_cb.peer_feat & BTA_HF_CLIENT_PEER_ECC)
        {
            if (idx < 1)
            {
                return BT_STATUS_FAIL;
            }
            BTA_HfClientSendAT(btif_hf_client_cb.handle, BTA_HF_CLIENT_AT_CMD_CHLD, 1, idx, NULL);
            break;
        }
        return BT_STATUS_UNSUPPORTED;
    case BTHF_CLIENT_CALL_ACTION_CHLD_2x:
        if (btif_hf_client_cb.peer_feat & BTA_HF_CLIENT_PEER_ECC)
        {
            if (idx < 1)
            {
                return BT_STATUS_FAIL;
            }
            BTA_HfClientSendAT(btif_hf_client_cb.handle, BTA_HF_CLIENT_AT_CMD_CHLD, 2, idx, NULL);
            break;
        }
        return BT_STATUS_UNSUPPORTED;
    case BTHF_CLIENT_CALL_ACTION_ATA:
        BTA_HfClientSendAT(btif_hf_client_cb.handle, BTA_HF_CLIENT_AT_CMD_ATA, 0, 0, NULL);
        break;
    case BTHF_CLIENT_CALL_ACTION_CHUP:
        BTA_HfClientSendAT(btif_hf_client_cb.handle, BTA_HF_CLIENT_AT_CMD_CHUP, 0, 0, NULL);
        break;
    case BTHF_CLIENT_CALL_ACTION_BTRH_0:
        BTA_HfClientSendAT(btif_hf_client_cb.handle, BTA_HF_CLIENT_AT_CMD_BTRH, 0, 0, NULL);
        break;
    case BTHF_CLIENT_CALL_ACTION_BTRH_1:
        BTA_HfClientSendAT(btif_hf_client_cb.handle, BTA_HF_CLIENT_AT_CMD_BTRH, 1, 0, NULL);
        break;
    case BTHF_CLIENT_CALL_ACTION_BTRH_2:
        BTA_HfClientSendAT(btif_hf_client_cb.handle, BTA_HF_CLIENT_AT_CMD_BTRH, 2, 0, NULL);
        break;
    default:
        return BT_STATUS_FAIL;

    }

    return BT_STATUS_SUCCESS;
}

/*******************************************************************************
**
** Function         query_current_calls
**
** Description      query list of current calls
**
** Returns          bt_status_t
**
*******************************************************************************/
static bt_status_t query_current_calls(void)
{
    CHECK_BTHF_CLIENT_SLC_CONNECTED();

    if (btif_hf_client_cb.peer_feat & BTA_HF_CLIENT_PEER_ECS)
    {
        BTA_HfClientSendAT(btif_hf_client_cb.handle, BTA_HF_CLIENT_AT_CMD_CLCC, 0, 0, NULL);

        return BT_STATUS_SUCCESS;
    }

    return BT_STATUS_UNSUPPORTED;
}

/*******************************************************************************
**
** Function         query_current_operator_name
**
** Description      query current selected operator name
**
** Returns          bt_status_t
**
*******************************************************************************/
static bt_status_t query_current_operator_name(void)
{
    CHECK_BTHF_CLIENT_SLC_CONNECTED();

    BTA_HfClientSendAT(btif_hf_client_cb.handle, BTA_HF_CLIENT_AT_CMD_COPS, 0, 0, NULL);

    return BT_STATUS_SUCCESS;
}

/*******************************************************************************
**
** Function         retieve_subscriber_info
**
** Description      retrieve subscriber number information
**
** Returns          bt_status_t
**
*******************************************************************************/
static bt_status_t retrieve_subscriber_info(void)
{
    CHECK_BTHF_CLIENT_SLC_CONNECTED();

    BTA_HfClientSendAT(btif_hf_client_cb.handle, BTA_HF_CLIENT_AT_CMD_CNUM, 0, 0, NULL);

    return BT_STATUS_SUCCESS;
}

/*******************************************************************************
**
** Function         send_dtmf
**
** Description      send dtmf
**
** Returns          bt_status_t
**
*******************************************************************************/
static bt_status_t send_dtmf(char code)
{
    CHECK_BTHF_CLIENT_SLC_CONNECTED();

    BTA_HfClientSendAT(btif_hf_client_cb.handle, BTA_HF_CLIENT_AT_CMD_VTS, code, 0, NULL);

    return BT_STATUS_SUCCESS;
}

/*******************************************************************************
**
** Function         request_last_voice_tag_number
**
** Description      Request number from AG for VR purposes
**
** Returns          bt_status_t
**
*******************************************************************************/
static bt_status_t request_last_voice_tag_number(void)
{
    CHECK_BTHF_CLIENT_SLC_CONNECTED();

    if (btif_hf_client_cb.peer_feat & BTA_HF_CLIENT_PEER_VTAG)
    {
        BTA_HfClientSendAT(btif_hf_client_cb.handle, BTA_HF_CLIENT_AT_CMD_BINP, 1, 0, NULL);

        return BT_STATUS_SUCCESS;
    }

    return BT_STATUS_UNSUPPORTED;
}

/*******************************************************************************
**
** Function         cleanup
**
** Description      Closes the HF interface
**
** Returns          bt_status_t
**
*******************************************************************************/
static void  cleanup( void )
{
    BTIF_TRACE_EVENT("%s", __FUNCTION__);

    btif_disable_service(BTA_HFP_HS_SERVICE_ID);
}

/*******************************************************************************
**
** Function        cleanup_vendor
**
** Description     cleans up the hf client vendor interface
**
** Returns         bt_status_t
**
*******************************************************************************/
static bt_status_t cleanup_vendor( void )
{
    BTIF_TRACE_EVENT("%s", __FUNCTION__);

    bt_hf_client_vendor_callbacks = NULL;

    return BT_STATUS_SUCCESS;
}
/*******************************************************************************
**
** Function         send_at_cmd
**
** Description      Send requested AT command to rempte device.
**
** Returns          bt_status_t
**
*******************************************************************************/
static bt_status_t send_at_cmd(int cmd,int val1,int val2,const char *arg)
{
    CHECK_BTHF_CLIENT_SLC_CONNECTED();
    BTIF_TRACE_EVENT("%s Cmd %d val1 %d val2 %d arg %s",
            __FUNCTION__,cmd,val1,val2,arg);
    BTA_HfClientSendAT(btif_hf_client_cb.handle, cmd, val1, val2, arg);

    return BT_STATUS_SUCCESS;
}

static const bthf_client_interface_t bthfClientInterface = {
    sizeof(bthf_client_interface_t),
    .init = init,
    .connect = connect,
    .disconnect = disconnect,
    .connect_audio = connect_audio,
    .disconnect_audio = disconnect_audio,
    .start_voice_recognition = start_voice_recognition,
    .stop_voice_recognition = stop_voice_recognition,
    .volume_control = volume_control,
    .dial = dial,
    .dial_memory = dial_memory,
    .handle_call_action = handle_call_action,
    .query_current_calls = query_current_calls,
    .query_current_operator_name = query_current_operator_name,
    .retrieve_subscriber_info = retrieve_subscriber_info,
    .send_dtmf = send_dtmf,
    .request_last_voice_tag_number = request_last_voice_tag_number,
    .cleanup = cleanup,
    .send_at_cmd = send_at_cmd,
};

static const bthf_client_vendor_interface_t bthfClientVendorInterface = {
    sizeof(bthf_client_vendor_interface_t),
    .init_vendor = init_vendor,
    .cleanup_vendor = cleanup_vendor,
};

static void process_ind_evt(tBTA_HF_CLIENT_IND *ind)
{
    switch (ind->type)
    {
        case BTA_HF_CLIENT_IND_CALL:
            HAL_CBACK(bt_hf_client_callbacks, call_cb, ind->value);
            break;

        case BTA_HF_CLIENT_IND_CALLSETUP:
            HAL_CBACK(bt_hf_client_callbacks, callsetup_cb, ind->value);
            break;
        case BTA_HF_CLIENT_IND_CALLHELD:
            HAL_CBACK(bt_hf_client_callbacks, callheld_cb, ind->value);
            break;

        case BTA_HF_CLIENT_IND_SERVICE:
            HAL_CBACK(bt_hf_client_callbacks, network_state_cb, ind->value);
            break;

        case BTA_HF_CLIENT_IND_SIGNAL:
            HAL_CBACK(bt_hf_client_callbacks, network_signal_cb, ind->value);
            break;

        case BTA_HF_CLIENT_IND_ROAM:
            HAL_CBACK(bt_hf_client_callbacks, network_roaming_cb, ind->value);
            break;

        case BTA_HF_CLIENT_IND_BATTCH:
            HAL_CBACK(bt_hf_client_callbacks, battery_level_cb, ind->value);
            break;

        default:
            break;
    }
}

/*******************************************************************************
**
** Function         btif_hf_client_upstreams_evt
**
** Description      Executes HF CLIENT UPSTREAMS events in btif context
**
** Returns          void
**
*******************************************************************************/
static void btif_hf_client_upstreams_evt(UINT16 event, char* p_param)
{
    tBTA_HF_CLIENT *p_data = (tBTA_HF_CLIENT *)p_param;
    bdstr_t bdstr;

    BTIF_TRACE_IMP("%s: event=%s (%u)", __FUNCTION__, dump_hf_client_event(event), event);

    switch (event)
    {
        case BTA_HF_CLIENT_ENABLE_EVT:
        case BTA_HF_CLIENT_DISABLE_EVT:
            break;

        case BTA_HF_CLIENT_REGISTER_EVT:
            btif_hf_client_cb.handle = p_data->reg.handle;
            break;

        case BTA_HF_CLIENT_OPEN_EVT:
            if (p_data->open.status == BTA_HF_CLIENT_SUCCESS)
            {
                bdcpy(btif_hf_client_cb.connected_bda.address, p_data->open.bd_addr);
                btif_hf_client_cb.state = BTHF_CLIENT_CONNECTION_STATE_CONNECTED;
                btif_hf_client_cb.peer_feat = 0;
                btif_hf_client_cb.chld_feat = 0;
                //clear_phone_state();
            }
            else if (btif_hf_client_cb.state == BTHF_CLIENT_CONNECTION_STATE_CONNECTING)
            {
                btif_hf_client_cb.state = BTHF_CLIENT_CONNECTION_STATE_DISCONNECTED;
            }
            else
            {
                BTIF_TRACE_WARNING("%s: HF CLient open failed, but another device connected. status=%d state=%d connected device=%s",
                        __FUNCTION__, p_data->open.status, btif_hf_client_cb.state, bdaddr_to_string(&btif_hf_client_cb.connected_bda, bdstr, sizeof(bdstr)));
                break;
            }

            HAL_CBACK(bt_hf_client_callbacks, connection_state_cb, btif_hf_client_cb.state,
                        0, 0, &btif_hf_client_cb.connected_bda);

            if (btif_hf_client_cb.state == BTHF_CLIENT_CONNECTION_STATE_DISCONNECTED)
                bdsetany(btif_hf_client_cb.connected_bda.address);

            if (p_data->open.status != BTA_HF_CLIENT_SUCCESS)
                btif_queue_advance();
            break;

        case BTA_HF_CLIENT_CONN_EVT:
            btif_hf_client_cb.peer_feat = p_data->conn.peer_feat;
            btif_hf_client_cb.chld_feat = p_data->conn.chld_feat;
            btif_hf_client_cb.state = BTHF_CLIENT_CONNECTION_STATE_SLC_CONNECTED;

            HAL_CBACK(bt_hf_client_callbacks, connection_state_cb, btif_hf_client_cb.state,
                        btif_hf_client_cb.peer_feat, btif_hf_client_cb.chld_feat,
                        &btif_hf_client_cb.connected_bda);

            /* Inform the application about in-band ringtone */
            if (btif_hf_client_cb.peer_feat & BTA_HF_CLIENT_PEER_INBAND)
            {
                HAL_CBACK(bt_hf_client_callbacks, in_band_ring_tone_cb, BTHF_CLIENT_IN_BAND_RINGTONE_PROVIDED);
            }

            btif_queue_advance();
            break;

        case BTA_HF_CLIENT_CLOSE_EVT:
            btif_hf_client_cb.state = BTHF_CLIENT_CONNECTION_STATE_DISCONNECTED;
            HAL_CBACK(bt_hf_client_callbacks, connection_state_cb,  btif_hf_client_cb.state,
                        0, 0, &btif_hf_client_cb.connected_bda);
            bdsetany(btif_hf_client_cb.connected_bda.address);
            btif_hf_client_cb.peer_feat = 0;
            btif_hf_client_cb.chld_feat = 0;
            btif_queue_advance();
            break;

        case BTA_HF_CLIENT_IND_EVT:
            process_ind_evt(&p_data->ind);
            break;

        case BTA_HF_CLIENT_MIC_EVT:
            HAL_CBACK(bt_hf_client_callbacks, volume_change_cb, BTHF_CLIENT_VOLUME_TYPE_MIC, p_data->val.value);
            break;

        case BTA_HF_CLIENT_SPK_EVT:
            HAL_CBACK(bt_hf_client_callbacks, volume_change_cb, BTHF_CLIENT_VOLUME_TYPE_SPK, p_data->val.value);
            break;

        case BTA_HF_CLIENT_VOICE_REC_EVT:
            HAL_CBACK(bt_hf_client_callbacks, vr_cmd_cb, p_data->val.value);
            break;

        case BTA_HF_CLIENT_OPERATOR_NAME_EVT:
            HAL_CBACK(bt_hf_client_callbacks, current_operator_cb, p_data->operator.name);
            break;

        case BTA_HF_CLIENT_CLIP_EVT:
            HAL_CBACK(bt_hf_client_callbacks, clip_cb, p_data->number.number);
            break;

        case BTA_HF_CLIENT_BINP_EVT:
            HAL_CBACK(bt_hf_client_callbacks, last_voice_tag_number_callback, p_data->number.number);
            break;

        case BTA_HF_CLIENT_CCWA_EVT:
            HAL_CBACK(bt_hf_client_callbacks, call_waiting_cb, p_data->number.number);
            break;

        case BTA_HF_CLIENT_AT_RESULT_EVT:
            HAL_CBACK(bt_hf_client_callbacks, cmd_complete_cb, p_data->result.type, p_data->result.cme);
            break;

        case BTA_HF_CLIENT_CLCC_EVT:
            HAL_CBACK(bt_hf_client_callbacks, current_calls_cb, p_data->clcc.idx,
                        p_data->clcc.inc ? BTHF_CLIENT_CALL_DIRECTION_INCOMING : BTHF_CLIENT_CALL_DIRECTION_OUTGOING,
                        p_data->clcc.status,
                        p_data->clcc.mpty ? BTHF_CLIENT_CALL_MPTY_TYPE_MULTI : BTHF_CLIENT_CALL_MPTY_TYPE_SINGLE,
                        p_data->clcc.number_present ? p_data->clcc.number : NULL);
            break;

        case BTA_HF_CLIENT_CNUM_EVT:
            if (p_data->cnum.service == 4)
            {
                HAL_CBACK(bt_hf_client_callbacks, subscriber_info_cb, p_data->cnum.number, BTHF_CLIENT_SERVICE_VOICE);
            }
            else if (p_data->cnum.service == 5)
            {
                HAL_CBACK(bt_hf_client_callbacks, subscriber_info_cb, p_data->cnum.number, BTHF_CLIENT_SERVICE_FAX);
            }
            else
            {
                HAL_CBACK(bt_hf_client_callbacks, subscriber_info_cb, p_data->cnum.number, BTHF_CLIENT_SERVICE_UNKNOWN);
            }
            break;

        case BTA_HF_CLIENT_BTRH_EVT:
            if (p_data->val.value <= BTRH_CLIENT_RESP_AND_HOLD_REJECT)
            {
                HAL_CBACK(bt_hf_client_callbacks, resp_and_hold_cb, p_data->val.value);
            }
            break;

        case BTA_HF_CLIENT_BSIR_EVT:
            if (p_data->val.value != 0)
            {
                HAL_CBACK(bt_hf_client_callbacks, in_band_ring_tone_cb, BTHF_CLIENT_IN_BAND_RINGTONE_PROVIDED);
            }
            else
            {
                HAL_CBACK(bt_hf_client_callbacks, in_band_ring_tone_cb, BTHF_CLIENT_IN_BAND_RINGTONE_NOT_PROVIDED);
            }
            break;

        case BTA_HF_CLIENT_AUDIO_OPEN_EVT:
            HAL_CBACK(bt_hf_client_callbacks, audio_state_cb, BTHF_CLIENT_AUDIO_STATE_CONNECTED, &btif_hf_client_cb.connected_bda);
            break;

        case BTA_HF_CLIENT_AUDIO_MSBC_OPEN_EVT:
            HAL_CBACK(bt_hf_client_callbacks, audio_state_cb, BTHF_CLIENT_AUDIO_STATE_CONNECTED_MSBC, &btif_hf_client_cb.connected_bda);
            break;

        case BTA_HF_CLIENT_AUDIO_CLOSE_EVT:
            HAL_CBACK(bt_hf_client_callbacks, audio_state_cb, BTHF_CLIENT_AUDIO_STATE_DISCONNECTED, &btif_hf_client_cb.connected_bda);
            break;
        case BTA_HF_CLIENT_RING_INDICATION:
            HAL_CBACK(bt_hf_client_callbacks, ring_indication_cb);
            break;
        case BTA_HF_CLIENT_CGMI_EVT:
            HAL_CBACK(bt_hf_client_vendor_callbacks, cgmi_vendor_cb, p_data->cgmi.name);
            break;
        case BTA_HF_CLIENT_CGMM_EVT:
            HAL_CBACK(bt_hf_client_vendor_callbacks, cgmm_vendor_cb, p_data->cgmm.model);
            break;
        default:
            BTIF_TRACE_WARNING("%s: Unhandled event: %d", __FUNCTION__, event);
            break;
    }
}

/*******************************************************************************
**
** Function         bte_hf_client_evt
**
** Description      Switches context from BTE to BTIF for all HF Client events
**
** Returns          void
**
*******************************************************************************/

static void bte_hf_client_evt(tBTA_HF_CLIENT_EVT event, tBTA_HF_CLIENT *p_data)
{
    bt_status_t status;

    /* switch context to btif task context (copy full union size for convenience) */
    status = btif_transfer_context(btif_hf_client_upstreams_evt, (uint16_t)event, (void*)p_data, sizeof(*p_data), NULL);

    /* catch any failed context transfers */
    ASSERTC(status == BT_STATUS_SUCCESS, "context transfer failed", status);
}

/*******************************************************************************
**
** Function         btif_hf_client_execute_service
**
** Description      Initializes/Shuts down the service
**
** Returns          BT_STATUS_SUCCESS on success, BT_STATUS_FAIL otherwise
**
*******************************************************************************/
bt_status_t btif_hf_client_execute_service(BOOLEAN b_enable)
{
    BTIF_TRACE_EVENT("%s enable:%d", __FUNCTION__, b_enable);

    property_get("ro.bluetooth.hfp.ver", btif_hf_client_version, "1.6");

    BTIF_TRACE_EVENT("%s: hfp client version is %s", __func__, btif_hf_client_version);

     if (b_enable)
     {
          /* Enable and register with BTA-HFClient */
          BTA_HfClientEnable(bte_hf_client_evt);
          if (strtof(btif_hf_client_version, NULL) >= 1.6)
          {
              BTIF_TRACE_EVENT("Support Codec Nego. %d ", BTIF_HF_CLIENT_FEATURES);
              BTA_HfClientRegister(BTIF_HF_CLIENT_SECURITY, BTIF_HF_CLIENT_FEATURES,
                      BTIF_HF_CLIENT_SERVICE_NAME);
          }
          else
          {
              BTIF_TRACE_EVENT("No Codec Nego Supported");
              btif_hf_client_features = BTIF_HF_CLIENT_FEATURES;
              btif_hf_client_features = btif_hf_client_features & (~BTA_HF_CLIENT_FEAT_CODEC);
              BTIF_TRACE_EVENT("btif_hf_client_features is   %d", btif_hf_client_features);
              BTA_HfClientRegister(BTIF_HF_CLIENT_SECURITY, btif_hf_client_features,
                      BTIF_HF_CLIENT_SERVICE_NAME);
          }

     }
     else
     {
         if (bt_hf_client_callbacks)
         {
             BTIF_TRACE_IMP("%s: setting call backs to NULL", __FUNCTION__);
             bt_hf_client_callbacks = NULL;
         }
         BTA_HfClientDeregister(btif_hf_client_cb.handle);
         BTA_HfClientDisable();
     }
     BTIF_TRACE_IMP("%s: enable: %d completed", __FUNCTION__, b_enable);
     return BT_STATUS_SUCCESS;
}

/*******************************************************************************
**
** Function         btif_hf_client_get_interface
**
** Description      Get the hf client callback interface
**
** Returns          bthf_client_interface_t
**
*******************************************************************************/
const bthf_client_interface_t *btif_hf_client_get_interface(void)
{
    BTIF_TRACE_EVENT("%s", __FUNCTION__);
    return &bthfClientInterface;
}

/*******************************************************************************
**
** Function         btif_hf_client_vendor_get_interface
**
** Description      Get the hf client vendor callback interface
**
** Returns          bthf_client_vendor_interface_t
**
*******************************************************************************/
const bthf_client_vendor_interface_t *btif_hf_client_vendor_get_interface(void)
{
    BTIF_TRACE_IMP("%s", __FUNCTION__);
    return &bthfClientVendorInterface;
}

