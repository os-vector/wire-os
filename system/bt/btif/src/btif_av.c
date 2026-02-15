/******************************************************************************
 *  Copyright (c) 2014, The Linux Foundation. All rights reserved.
 *  Not a Contribution.
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

#define LOG_TAG "btif_av"

#include "btif_av.h"

#include <assert.h>
#include <string.h>

#include <hardware/bluetooth.h>
#include "hardware/bt_av.h"

#define LOG_TAG "bt_btif_av"

#include "bt_utils.h"
#include "a2d_aptx.h"
#include "bta_api.h"
#include "bta_av_co.h"
#include "btif_media.h"
#include "btif_profile_queue.h"
#include "btif_util.h"
#include "btu.h"
#include "bt_common.h"
#include "osi/include/allocator.h"
#include <cutils/properties.h>
#if (defined(BTC_INCLUDED) && BTC_INCLUDED == TRUE)
#include "btc_common.h"
#endif
#include "hardware/bt_av_vendor.h"

/*****************************************************************************
**  Constants & Macros
******************************************************************************/
#define BTIF_AV_SERVICE_NAME "Advanced Audio"
#define BTIF_AVK_SERVICE_NAME "Advanced Audio Sink"

#define BTIF_TIMEOUT_AV_OPEN_ON_RC_MS  (2 * 1000)

/* Number of BTIF-AV control blocks */
/* Now supports Two AV connections. */
#define BTIF_AV_NUM_CB       2
#define HANDLE_TO_INDEX(x) ((x & BTA_AV_HNDL_MSK) - 1)
#define INVALID_INDEX        -1

typedef enum {
    BTIF_AV_STATE_IDLE = 0x0,
    BTIF_AV_STATE_OPENING,
    BTIF_AV_STATE_OPENED,
    BTIF_AV_STATE_STARTED,
    BTIF_AV_STATE_CLOSING
} btif_av_state_t;

/* Should not need dedicated suspend state as actual actions are no
   different than open state. Suspend flags are needed however to prevent
   media task from trying to restart stream during remote suspend or while
   we are in the process of a local suspend */

#define BTIF_AV_FLAG_LOCAL_SUSPEND_PENDING 0x1
#define BTIF_AV_FLAG_REMOTE_SUSPEND        0x2
#define BTIF_AV_FLAG_PENDING_START         0x4
#define BTIF_AV_FLAG_PENDING_STOP          0x8
/* Host role defenitions */
#define HOST_ROLE_MASTER                   0x00
#define HOST_ROLE_SLAVE                    0x01
#define HOST_ROLE_UNKNOWN                  0xff

#define MAX_A2DP_SINK_PCM_QUEUE_SZ         20

/*****************************************************************************
**  Local type definitions
******************************************************************************/
typedef struct
{
    UINT16 len;
    UINT16 offset;
} tBT_PCM_HDR;

typedef struct
{
    tBTA_AV_HNDL bta_handle;
    bt_bdaddr_t peer_bda;
    btif_sm_handle_t sm_handle;
    UINT8 flags;
    tBTA_AV_EDR edr;
    UINT8 peer_sep;  /* sep type of peer device */
    UINT8 edr_3mbps;
    BOOLEAN dual_handoff;
    BOOLEAN current_playing;
    btif_sm_state_t state;
    int service;
    BOOLEAN is_slave;
    BOOLEAN is_device_playing;
#ifdef BTA_AV_SPLIT_A2DP_ENABLED
    UINT16 channel_id;
#endif
} btif_av_cb_t;

static pthread_mutex_t pcm_queue_lock;
pthread_mutex_t src_codec_q_lock;

typedef struct
{
    bt_bdaddr_t *target_bda;
    uint16_t uuid;
} btif_av_connect_req_t;

typedef struct
{
    int sample_rate;
    int channel_count;
    bt_bdaddr_t peer_bd;
} btif_av_sink_config_req_t;

typedef struct
{
    UINT8 peer_bd[6];
    UINT8 codec_type;
    btav_codec_config_t codec_info;
} btif_av_src_config_req_t;

typedef struct
{
    BOOLEAN sbc_offload;
    BOOLEAN aptx_offload;
    BOOLEAN aac_offload;
    BOOLEAN aptxhd_offload;
} btif_av_a2dp_offloaded_codec_cap_t;

typedef enum {
    SBC,
    APTX,
    AAC,
    APTXHD,
}btif_av_codec_list;

/*****************************************************************************
**  Static variables
******************************************************************************/
static btav_callbacks_t *bt_av_src_callbacks = NULL;
static btav_callbacks_t *bt_av_sink_callbacks = NULL;
static alarm_t *av_open_on_rc_timer = NULL;
static btav_vendor_callbacks_t *bt_av_src_vendor_callbacks = NULL;
static btav_vendor_callbacks_t *bt_av_sink_vendor_callbacks = NULL;
static btif_av_cb_t btif_av_cb[BTIF_AV_NUM_CB];
static btif_sm_event_t idle_rc_event;
static tBTA_AV idle_rc_data;
int btif_max_av_clients = 1;
static UINT16 enable_delay_reporting = 0; // by default disable it
static BOOLEAN enable_multicast = FALSE;
static BOOLEAN is_multicast_supported = FALSE;
static BOOLEAN multicast_disabled = FALSE;
BOOLEAN bt_split_a2dp_enabled = FALSE;
btif_av_a2dp_offloaded_codec_cap_t btif_av_codec_offload;
/* both interface and media task needs to be ready to alloc incoming request */
#define CHECK_BTAV_INIT() if (((bt_av_src_callbacks == NULL) &&(bt_av_sink_callbacks == NULL)) \
        || (btif_av_cb[0].sm_handle == NULL))\
{\
     BTIF_TRACE_WARNING("%s: BTAV not initialized", __FUNCTION__);\
     return BT_STATUS_NOT_READY;\
}\
else\
{\
     BTIF_TRACE_EVENT("%s", __FUNCTION__);\
}

/* Helper macro to avoid code duplication in the state machine handlers */
#define CHECK_RC_EVENT(e, d) \
    case BTA_AV_RC_CLOSE_EVT: \
    case BTA_AV_REMOTE_CMD_EVT: \
    case BTA_AV_VENDOR_CMD_EVT: \
    case BTA_AV_META_MSG_EVT: \
    case BTA_AV_BROWSE_MSG_EVT: \
    case BTA_AV_RC_FEAT_EVT: \
    case BTA_AV_REMOTE_RSP_EVT: \
    { \
         btif_rc_handler(e, d);\
    }break; \


static BOOLEAN btif_av_state_idle_handler(btif_sm_event_t event, void *data, int index);
static BOOLEAN btif_av_state_opening_handler(btif_sm_event_t event, void *data, int index);
static BOOLEAN btif_av_state_opened_handler(btif_sm_event_t event, void *data, int index);
static BOOLEAN btif_av_state_started_handler(btif_sm_event_t event, void *data,int index);

static BOOLEAN btif_av_state_closing_handler(btif_sm_event_t event, void *data,int index);

static BOOLEAN btif_av_get_valid_idx(int idx);
static UINT8 btif_av_idx_by_bdaddr( BD_ADDR bd_addr);
int btif_get_latest_playing_device_idx();
static int btif_get_latest_device_idx_to_start();
static int btif_av_get_valid_idx_for_rc_events(BD_ADDR bd_addr, int rc_handle);
static int btif_get_conn_state_of_device(BD_ADDR address);
static bt_status_t connect_int(bt_bdaddr_t *bd_addr, uint16_t uuid);
static void btif_av_update_current_playing_device(int index);
static void btif_av_check_rc_connection_priority(void *p_data);
#ifdef AVK_BACKPORT
void btif_av_request_audio_focus( BOOLEAN enable);
#endif
static const btif_sm_handler_t btif_av_state_handlers[] =
{
    btif_av_state_idle_handler,
    btif_av_state_opening_handler,
    btif_av_state_opened_handler,
    btif_av_state_started_handler,
    btif_av_state_closing_handler
};

static void btif_av_event_free_data(btif_sm_event_t event, void *p_data);

/*************************************************************************
** Extern functions
*************************************************************************/
extern void btif_rc_handler(tBTA_AV_EVT event, tBTA_AV *p_data);
extern BOOLEAN btif_rc_get_connected_peer(BD_ADDR peer_addr);
extern UINT8 btif_rc_get_connected_peer_handle(BD_ADDR peer_addr);
extern void btif_rc_check_handle_pending_play (BD_ADDR peer_addr, BOOLEAN bSendToApp);
extern void btif_rc_get_playing_device(BD_ADDR address);
extern void btif_rc_clear_playing_state(BOOLEAN play);
extern void btif_rc_clear_priority(BD_ADDR address);
extern void btif_rc_send_pause_command();
extern void btif_rc_ctrl_send_pause(bt_bdaddr_t *bd_addr);
extern void btif_rc_ctrl_send_play(bt_bdaddr_t *bd_addr);
extern UINT16 btif_dm_get_br_edr_links();
extern UINT16 btif_dm_get_le_links();
extern UINT16 btif_hf_is_call_idle();

extern fixed_queue_t *btu_general_alarm_queue;
extern tBTA_AV_CO_CODEC_CAP_LIST *p_bta_av_codec_pri_list;
extern tBTA_AV_CO_CODEC_CAP_LIST bta_av_supp_codec_cap[BTIF_SV_AV_AA_SRC_SEP_INDEX];
extern UINT8 bta_av_num_codec_configs;
extern const tA2D_SBC_CIE bta_av_co_sbc_caps;
extern const tA2D_APTX_CIE bta_av_co_aptx_caps;
/*****************************************************************************
** Local helper functions
******************************************************************************/
void btif_av_trigger_dual_handoff(BOOLEAN handoff, BD_ADDR address);
BOOLEAN btif_av_is_device_connected(BD_ADDR address);

BOOLEAN btif_av_is_connected_on_other_idx(int current_index);
BOOLEAN btif_av_is_playing_on_other_idx(int current_index);
BOOLEAN btif_av_is_playing();
void btif_av_update_multicast_state(int index);
BOOLEAN btif_av_get_ongoing_multicast();
tBTA_AV_HNDL btif_av_get_playing_device_hdl();
tBTA_AV_HNDL btif_av_get_av_hdl_from_idx(UINT8 idx);
int btif_av_get_other_connected_idx(int current_index);
#ifdef BTA_AV_SPLIT_A2DP_ENABLED
BOOLEAN btif_av_is_codec_offload_supported(int codec);
int btif_av_get_current_playing_dev_idx();
BOOLEAN btif_av_is_under_handoff();
#else
#define btif_av_is_codec_offload_supported(codec) (0)
#define btif_av_get_current_playing_dev_idx() (0)
#define btif_av_is_under_handoff() (0)
#endif

const char *dump_av_sm_state_name(btif_av_state_t state)
{
    switch (state)
    {
        CASE_RETURN_STR(BTIF_AV_STATE_IDLE)
        CASE_RETURN_STR(BTIF_AV_STATE_OPENING)
        CASE_RETURN_STR(BTIF_AV_STATE_OPENED)
        CASE_RETURN_STR(BTIF_AV_STATE_STARTED)
        CASE_RETURN_STR(BTIF_AV_STATE_CLOSING)
        default: return "UNKNOWN_STATE";
    }
}

const char *dump_av_sm_event_name(btif_av_sm_event_t event)
{
    switch((int)event)
    {
        CASE_RETURN_STR(BTA_AV_ENABLE_EVT)
        CASE_RETURN_STR(BTA_AV_REGISTER_EVT)
        CASE_RETURN_STR(BTA_AV_OPEN_EVT)
        CASE_RETURN_STR(BTA_AV_CLOSE_EVT)
        CASE_RETURN_STR(BTA_AV_START_EVT)
        CASE_RETURN_STR(BTA_AV_STOP_EVT)
        CASE_RETURN_STR(BTA_AV_PROTECT_REQ_EVT)
        CASE_RETURN_STR(BTA_AV_PROTECT_RSP_EVT)
        CASE_RETURN_STR(BTA_AV_RC_OPEN_EVT)
        CASE_RETURN_STR(BTA_AV_RC_CLOSE_EVT)
        CASE_RETURN_STR(BTA_AV_REMOTE_CMD_EVT)
        CASE_RETURN_STR(BTA_AV_REMOTE_RSP_EVT)
        CASE_RETURN_STR(BTA_AV_VENDOR_CMD_EVT)
        CASE_RETURN_STR(BTA_AV_VENDOR_RSP_EVT)
        CASE_RETURN_STR(BTA_AV_RECONFIG_EVT)
        CASE_RETURN_STR(BTA_AV_SUSPEND_EVT)
        CASE_RETURN_STR(BTA_AV_PENDING_EVT)
        CASE_RETURN_STR(BTA_AV_META_MSG_EVT)
        CASE_RETURN_STR(BTA_AV_REJECT_EVT)
        CASE_RETURN_STR(BTA_AV_RC_FEAT_EVT)
        CASE_RETURN_STR(BTA_AV_OFFLOAD_START_RSP_EVT)
        CASE_RETURN_STR(BTIF_SM_ENTER_EVT)
        CASE_RETURN_STR(BTIF_SM_EXIT_EVT)
        CASE_RETURN_STR(BTIF_AV_CONNECT_REQ_EVT)
        CASE_RETURN_STR(BTIF_AV_DISCONNECT_REQ_EVT)
        CASE_RETURN_STR(BTIF_AV_START_STREAM_REQ_EVT)
        CASE_RETURN_STR(BTIF_AVK_START_STREAM_REQ_EVT)
        CASE_RETURN_STR(BTIF_AV_STOP_STREAM_REQ_EVT)
        CASE_RETURN_STR(BTIF_AV_SUSPEND_STREAM_REQ_EVT)
        CASE_RETURN_STR(BTIF_AVK_SUSPEND_STREAM_REQ_EVT)
        CASE_RETURN_STR(BTIF_AV_SINK_CONFIG_REQ_EVT)
        CASE_RETURN_STR(BTIF_AV_OFFLOAD_START_REQ_EVT)
#ifdef USE_AUDIO_TRACK
        CASE_RETURN_STR(BTIF_AV_SINK_FOCUS_REQ_EVT)
#endif
        CASE_RETURN_STR(BTIF_AV_UPDATE_ENCODER_REQ_EVT)
        CASE_RETURN_STR(BTIF_AV_SRC_CONFIG_REQ_EVT)
        default: return "UNKNOWN_EVENT";
   }
}

const char *dump_av_codec_name(btif_av_codec_list codec)
{
    switch((int)codec)
    {
        CASE_RETURN_STR(SBC)
        CASE_RETURN_STR(APTX)
        CASE_RETURN_STR(AAC)
        CASE_RETURN_STR(APTXHD)
        default: return "UNKNOWN_CODEC";
    }
}
//TODO.. We will remove this data structure
static BD_ADDR bd_null= {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

/****************************************************************************
**  Local helper functions
*****************************************************************************/
/*******************************************************************************
**
** Function         btif_initiate_av_open_timer_timeout
**
** Description      Timer to trigger AV open if the remote headset establishes
**                  RC connection w/o AV connection. The timer is needed to IOP
**                  with headsets that do establish AV after RC connection.
**
** Returns          void
**
*******************************************************************************/
static void btif_initiate_av_open_timer_timeout(UNUSED_ATTR void *data)
{
    BD_ADDR peer_addr;

    /* is there at least one RC connection - There should be */
    /*We have Two Connections.*/
    if (btif_rc_get_connected_peer(peer_addr))
    {
        /*Check if this peer_addr is same as currently connected AV*/
        if (btif_get_conn_state_of_device(peer_addr) == BTIF_AV_STATE_OPENED)
        {
            BTIF_TRACE_DEBUG("AV is already connected");
        }
        else
        {
            UINT8 rc_handle;
            int index;
            /* Multicast: Check if AV slot is available for connection
             * If not available, AV got connected to different devices.
             * Disconnect this RC connection without AV connection.
             */
            rc_handle = btif_rc_get_connected_peer_handle(peer_addr);
            index = btif_av_get_valid_idx_for_rc_events(peer_addr, rc_handle);
            if(index >= btif_max_av_clients)
            {
                BTIF_TRACE_ERROR("%s No slot free for AV connection, back off",
                            __FUNCTION__);
                return;
            }
            BTIF_TRACE_DEBUG("%s Issuing connect to the remote RC peer", __FUNCTION__);
            if(bt_av_sink_callbacks != NULL)
                btif_queue_connect(UUID_SERVCLASS_AUDIO_SINK, (bt_bdaddr_t*)&peer_addr,
                        connect_int);
            if(bt_av_src_callbacks != NULL)
                btif_queue_connect(UUID_SERVCLASS_AUDIO_SOURCE, (bt_bdaddr_t*)&peer_addr,
                        connect_int);
        }
    }
    else
    {
        BTIF_TRACE_ERROR("%s No connected RC peers", __FUNCTION__);
    }

}


/*****************************************************************************
**  Static functions
******************************************************************************/

/*******************************************************************************
**
** Function         btif_report_connection_state
**
** Description      Updates the components via the callbacks about the connection
**                  state of a2dp connection.
**
** Returns          None
**
*******************************************************************************/
static void btif_report_connection_state(btav_connection_state_t state, bt_bdaddr_t *bd_addr)
{
    if (bt_av_sink_callbacks != NULL) {
        HAL_CBACK(bt_av_sink_callbacks, connection_state_cb, state, bd_addr);
    } else if ( bt_av_src_callbacks != NULL) {
        HAL_CBACK(bt_av_src_callbacks, connection_state_cb, state, bd_addr);
    }

#if (defined(BTC_INCLUDED) && BTC_INCLUDED == TRUE)
    if ((bt_av_sink_callbacks != NULL) || ( bt_av_src_callbacks != NULL))
    {
        if(BTAV_CONNECTION_STATE_CONNECTED == state)
        {
            btc_post_msg(BLUETOOTH_AUDIO_SINK_CONNECTED);
        }
        else if(BTAV_CONNECTION_STATE_DISCONNECTED == state)
        {
            btc_post_msg(BLUETOOTH_AUDIO_SINK_DISCONNECTED);
        }
    }
#endif
}

/*******************************************************************************
**
** Function         btif_report_audio_state
**
** Description      Updates the components via the callbacks about the audio
**                  state of a2dp connection. The state is updated when either
**                  the remote ends starts streaming (started state) or whenever
**                  it transitions out of started state (to opened or streaming)
**                  state.
**
** Returns          None
**
*******************************************************************************/
static void btif_report_audio_state(btav_audio_state_t state, bt_bdaddr_t *bd_addr)
{
    if (bt_av_sink_callbacks != NULL) {
        HAL_CBACK(bt_av_sink_callbacks, audio_state_cb, state, bd_addr);
    } else if (bt_av_src_callbacks != NULL) {
        HAL_CBACK(bt_av_src_callbacks, audio_state_cb, state, bd_addr);
    }
#if (defined(BTC_INCLUDED) && BTC_INCLUDED == TRUE)
    if ((bt_av_sink_callbacks != NULL) || ( bt_av_src_callbacks != NULL))
    {
        if(BTAV_AUDIO_STATE_STARTED == state)
        {
            btc_post_msg(BLUETOOTH_SINK_STREAM_STARTED);
        }
        else if((BTAV_AUDIO_STATE_STOPPED == state) ||
                (BTAV_AUDIO_STATE_REMOTE_SUSPEND == state))
        {
            btc_post_msg(BLUETOOTH_SINK_STREAM_STOPPED);
        }
    }
#endif
}

/*****************************************************************************
**
** Function     btif_av_state_idle_handler
**
** Description  State managing disconnected AV link
**
** Returns      TRUE if event was processed, FALSE otherwise
**
*******************************************************************************/

static BOOLEAN btif_av_state_idle_handler(btif_sm_event_t event, void *p_data, int index)
{
    char a2dp_role[255] = "false";

    BTIF_TRACE_IMP("%s event:%s flags %x on Index = %d", __FUNCTION__,
                     dump_av_sm_event_name(event), btif_av_cb[index].flags, index);

    switch (event)
    {
        case BTIF_SM_ENTER_EVT:
            /* clear the peer_bda */
            BTIF_TRACE_EVENT("IDLE state for index: %d service %d", index,
                    btif_av_cb[index].service);
            memset(&btif_av_cb[index].peer_bda, 0, sizeof(bt_bdaddr_t));
            btif_av_cb[index].flags = 0;
            btif_av_cb[index].edr_3mbps = 0;
            btif_av_cb[index].edr = 0;
            btif_av_cb[index].current_playing = FALSE;
            btif_av_cb[index].is_slave = FALSE;
            btif_av_cb[index].is_device_playing = FALSE;
            for (int i = 0; i < btif_max_av_clients; i++)
            {
                btif_av_cb[i].dual_handoff = FALSE;
            }
            if (btif_av_cb[index].service == BTA_A2DP_SINK_SERVICE_ID)
            {
                btif_av_cb[index].peer_sep = AVDT_TSEP_SRC;
                btif_a2dp_set_peer_sep(AVDT_TSEP_SRC);
            }
            else if (btif_av_cb[index].service == BTA_A2DP_SOURCE_SERVICE_ID)
            {
                btif_av_cb[index].peer_sep = AVDT_TSEP_SNK;
                btif_a2dp_set_peer_sep(AVDT_TSEP_SNK);
            }
#ifdef BTA_AV_SPLIT_A2DP_ENABLED
            btif_av_cb[index].channel_id = 0;
#endif
            /* This API will be called twice at initialization
            ** Idle can be moved when device is disconnected too.
            ** Take care of other connected device here.*/
            if (!btif_av_is_connected())
            {
                BTIF_TRACE_EVENT("reset A2dp states in IDLE ");
                btif_a2dp_on_idle();
            }
            else
            {
                //There is another AV connection, update current playin
                BTIF_TRACE_EVENT("reset A2dp states in IDLE ");
                //btif_media_send_reset_vendor_state();
                btif_av_update_current_playing_device(index);
            }
            if (!btif_av_is_playing_on_other_idx(index) &&
                 bt_split_a2dp_enabled)
            {
                BTIF_TRACE_EVENT("reset Vendor flag A2DP state is IDLE");
                btif_media_send_reset_vendor_state();
            }
            break;

        case BTIF_SM_EXIT_EVT:
            break;

        case BTA_AV_ENABLE_EVT:
            BTIF_TRACE_EVENT("AV is enabled now for index: %d", index);
            break;

        case BTA_AV_REGISTER_EVT:
            BTIF_TRACE_EVENT("The AV Handle:%d", ((tBTA_AV*)p_data)->registr.hndl);
            btif_av_cb[index].bta_handle = ((tBTA_AV*)p_data)->registr.hndl;
            break;

           /*
            * In case Signalling channel is not down
            * and remote started Streaming Procedure
            * we have to handle config and open event in
            * idle_state. We hit these scenarios while running
            * PTS test case for AVRCP Controller
            */
        case BTIF_AV_SINK_CONFIG_REQ_EVT:
        {
            btif_av_sink_config_req_t req;
            // copy to avoid alignment problems
            memcpy(&req, p_data, sizeof(req));

            BTIF_TRACE_WARNING("BTIF_AV_SINK_CONFIG_REQ_EVT %d %d", req.sample_rate,
                    req.channel_count);
            if (bt_av_sink_callbacks != NULL) {
                HAL_CBACK(bt_av_sink_callbacks, audio_config_cb, &(req.peer_bd),
                        req.sample_rate, req.channel_count);
            }
        } break;

        case BTIF_AV_CONNECT_REQ_EVT:
            /* For outgoing connect stack and app are in sync.
            */
            memcpy(&btif_av_cb[index].peer_bda, ((btif_av_connect_req_t*)p_data)->target_bda,
                                                                        sizeof(bt_bdaddr_t));
            BTA_AvOpen(btif_av_cb[index].peer_bda.address, btif_av_cb[index].bta_handle,
                        TRUE, BTA_SEC_NONE, ((btif_av_connect_req_t*)p_data)->uuid);
            btif_sm_change_state(btif_av_cb[index].sm_handle, BTIF_AV_STATE_OPENING);
            break;

        case BTA_AV_PENDING_EVT:
        case BTA_AV_RC_OPEN_EVT:
            /* IOP_FIX: Jabra 620 only does RC open without AV open whenever it connects. So
             * as per the AV WP, an AVRC connection cannot exist without an AV connection. Therefore,
             * we initiate an AV connection if an RC_OPEN_EVT is received when we are in AV_CLOSED state.
             * We initiate the AV connection after a small 3s timeout to avoid any collisions from the
             * headsets, as some headsets initiate the AVRC connection first and then
             * immediately initiate the AV connection
             *
             * TODO: We may need to do this only on an AVRCP Play. FixMe
             */
            /* Check if connection allowed with this device */
            /* In Dual A2dp case, this event can come for both the headsets.
             * Reject second connection request as we are already checking
             * for device priority for first device and we cannot queue
             * incoming connections requests.
             */

            if (idle_rc_event != 0)
            {
                BTIF_TRACE_DEBUG("Processing another RC Event ");
                return FALSE;
            }
            memcpy(&idle_rc_data, ((tBTA_AV*)p_data), sizeof(tBTA_AV));
            if (event == BTA_AV_RC_OPEN_EVT)
            {
                if (((tBTA_AV*)p_data)->rc_open.status == BTA_AV_SUCCESS)
                {
                    bdcpy(btif_av_cb[index].peer_bda.address,
                        ((tBTA_AV*)p_data)->rc_open.peer_addr);
                }
                else
                {
                    idle_rc_event = 0;
                    return TRUE;
                }
            }
            else
            {
                bdcpy(btif_av_cb[index].peer_bda.address, ((tBTA_AV*)p_data)->pend.bd_addr);
            }

            // Only for AVDTP connection request move to opening state
            if (event == BTA_AV_PENDING_EVT)
                btif_sm_change_state(btif_av_cb[index].sm_handle, BTIF_AV_STATE_OPENING);

            if (bt_av_src_vendor_callbacks != NULL)
            {
                BTIF_TRACE_DEBUG("Calling connection priority callback ");
                idle_rc_event = event;
                HAL_CBACK(bt_av_src_vendor_callbacks, connection_priority_vendor_cb,
                         &(btif_av_cb[index].peer_bda));
            }
            if (bt_av_sink_callbacks != NULL)
            {
                if(event == BTA_AV_PENDING_EVT)
                {
                    BTA_AvOpen(btif_av_cb[index].peer_bda.address, btif_av_cb[index].bta_handle,
                       TRUE, BTA_SEC_NONE, UUID_SERVCLASS_AUDIO_SINK);
                }
                else if(event == BTA_AV_RC_OPEN_EVT)
                {
                    alarm_set_on_queue(av_open_on_rc_timer,
                              BTIF_TIMEOUT_AV_OPEN_ON_RC_MS,
                              btif_initiate_av_open_timer_timeout, NULL,
                              btu_general_alarm_queue);
                    btif_rc_handler(event, p_data);
                }
            }
            break;

        case BTA_AV_OPEN_EVT:
        {
            /* We get this event in Idle State if Signaling
             * channel is not closed, only Streaming channel was
             * closed earlier, and now only stream setup process is
             * initiated.
             */
            tBTA_AV *p_bta_data = (tBTA_AV*)p_data;
            btav_connection_state_t state;
            BTIF_TRACE_DEBUG("status:%d, edr 0x%x",p_bta_data->open.status,
                               p_bta_data->open.edr);

            if (p_bta_data->open.status == BTA_AV_SUCCESS)
            {
                 state = BTAV_CONNECTION_STATE_CONNECTED;
                 btif_av_cb[index].edr = p_bta_data->open.edr;
                 if (p_bta_data->open.role == HOST_ROLE_SLAVE)
                 {
                    btif_av_cb[index].is_slave = TRUE;
                 }
                 btif_av_cb[index].peer_sep = p_bta_data->open.sep;
                 btif_a2dp_set_peer_sep(p_bta_data->open.sep);

                 if (p_bta_data->open.edr & BTA_AV_EDR_3MBPS)
                 {
                     BTIF_TRACE_DEBUG("remote supports 3 mbps");
                     btif_av_cb[index].edr_3mbps = TRUE;
                 }

                 bdcpy(btif_av_cb[index].peer_bda.address, ((tBTA_AV*)p_data)->open.bd_addr);
#ifdef BTA_AV_SPLIT_A2DP_ENABLED
                 btif_av_cb[index].channel_id = p_bta_data->open.stream_chnl_id;
                 BTIF_TRACE_DEBUG("streaming channel id updated as : 0x%x",
                    btif_av_cb[index].channel_id);
#endif
            }
            else
            {
                BTIF_TRACE_WARNING("BTA_AV_OPEN_EVT::FAILED status: %d",
                                     p_bta_data->open.status );
                state = BTAV_CONNECTION_STATE_DISCONNECTED;
            }

            /* change state to open based on the status */
            if (p_bta_data->open.status == BTA_AV_SUCCESS)
            {
                /* inform the application of the event */
                btif_report_connection_state(state, &(btif_av_cb[index].peer_bda));
                btif_sm_change_state(btif_av_cb[index].sm_handle, BTIF_AV_STATE_OPENED);
                /* BTIF AV State updated, now check
                 * and update multicast state
                 */
                btif_av_update_multicast_state(index);
            }

            if (btif_av_cb[index].peer_sep == AVDT_TSEP_SNK)
            {
                /* if queued PLAY command,  send it now */
                btif_rc_check_handle_pending_play(p_bta_data->open.bd_addr,
                                             (p_bta_data->open.status == BTA_AV_SUCCESS));
            }
            else if ((btif_av_cb[index].peer_sep == AVDT_TSEP_SRC) &&
                    (p_bta_data->open.status == BTA_AV_SUCCESS))
            {
                /* if queued PLAY command,  send it now */
                btif_rc_check_handle_pending_play(p_bta_data->open.bd_addr, FALSE);
                /* Bring up AVRCP connection too */
                BTA_AvOpenRc(btif_av_cb[index].bta_handle);
            }
            btif_queue_advance();
        } break;

        case BTA_AV_REMOTE_CMD_EVT:
        case BTA_AV_VENDOR_CMD_EVT:
        case BTA_AV_META_MSG_EVT:
        case BTA_AV_RC_FEAT_EVT:
        case BTA_AV_REMOTE_RSP_EVT:
        case BTA_AV_BROWSE_MSG_EVT:
            btif_rc_handler(event, (tBTA_AV*)p_data);
            break;

        case BTA_AV_RC_CLOSE_EVT:
            BTIF_TRACE_DEBUG("BTA_AV_RC_CLOSE_EVT: Stopping AV timer.");
            alarm_cancel(av_open_on_rc_timer);
            btif_rc_handler(event, p_data);
            break;

        case BTIF_AV_OFFLOAD_START_REQ_EVT:
            BTIF_TRACE_ERROR("BTIF_AV_OFFLOAD_START_REQ_EVT: Stream not Started IDLE");
            btif_a2dp_on_offload_started(BTA_AV_FAIL);
            break;

        case BTIF_AV_SRC_CONFIG_REQ_EVT:
        {
            btif_av_src_config_req_t req;
            bdstr_t addr;
            // copy to avoid alignment problems
            /* in this case, L2CAP connection is still up, but bt-app moved to disc state
               so lets move bt-app to connected state first */
            memcpy(&req, p_data, sizeof(req));
            BTIF_TRACE_WARNING("BTIF_AV_SRC_CONFIG_REQ_EVT %s %d",
                    bdaddr_to_string((bt_bdaddr_t *)req.peer_bd,
                    &addr, sizeof(addr)), req.codec_type);

            if (bt_av_src_vendor_callbacks != NULL) {
                BTIF_TRACE_DEBUG("Calling audio_codec_config callback ");
                HAL_CBACK(bt_av_src_vendor_callbacks, audio_codec_config_vendor_cb,
                        &(req.peer_bd), req.codec_type, req.codec_info);
            }
        }
            break;

        default:
            BTIF_TRACE_WARNING("%s : unhandled event:%s", __FUNCTION__,
                                dump_av_sm_event_name(event));
            return FALSE;

    }

    return TRUE;
}
/*****************************************************************************
**
** Function        btif_av_state_opening_handler
**
** Description     Intermediate state managing events during establishment
**                 of avdtp channel
**
** Returns         TRUE if event was processed, FALSE otherwise
**
*******************************************************************************/

static BOOLEAN btif_av_state_opening_handler(btif_sm_event_t event, void *p_data, int index)
{
    int i;
    BTIF_TRACE_IMP("%s event:%s flags %x on index = %d", __FUNCTION__,
                     dump_av_sm_event_name(event), btif_av_cb[index].flags, index);
    switch (event)
    {
        case BTIF_SM_ENTER_EVT:
            /* inform the application that we are entering connecting state */
            if (bt_av_sink_callbacks != NULL)
            {
                HAL_CBACK(bt_av_sink_callbacks, connection_state_cb,
                         BTAV_CONNECTION_STATE_CONNECTING, &(btif_av_cb[index].peer_bda));
            }
            else if (bt_av_src_callbacks != NULL)
            {
                HAL_CBACK(bt_av_src_callbacks, connection_state_cb,
                         BTAV_CONNECTION_STATE_CONNECTING, &(btif_av_cb[index].peer_bda));
            }
            break;

        case BTIF_SM_EXIT_EVT:
            break;

        case BTA_AV_REJECT_EVT:
            BTIF_TRACE_DEBUG(" Received  BTA_AV_REJECT_EVT ");
            btif_report_connection_state(BTAV_CONNECTION_STATE_DISCONNECTED,
                                        &(btif_av_cb[index].peer_bda));
            btif_sm_change_state(btif_av_cb[index].sm_handle, BTIF_AV_STATE_IDLE);
            break;

        case BTA_AV_OPEN_EVT:
        {
            tBTA_AV *p_bta_data = (tBTA_AV*)p_data;
            btav_connection_state_t state;
            btif_sm_state_t av_state;
            BTIF_TRACE_DEBUG("status:%d, edr 0x%x, role: 0x%x",p_bta_data->open.status,
                             p_bta_data->open.edr, p_bta_data->open.role);

            if (p_bta_data->open.status == BTA_AV_SUCCESS)
            {
                 state = BTAV_CONNECTION_STATE_CONNECTED;
                 av_state = BTIF_AV_STATE_OPENED;
                 btif_av_cb[index].edr = p_bta_data->open.edr;
                 if (p_bta_data->open.role == HOST_ROLE_SLAVE)
                 {
                    btif_av_cb[index].is_slave = TRUE;
                 }
                 btif_av_cb[index].peer_sep = p_bta_data->open.sep;
                 btif_a2dp_set_peer_sep(p_bta_data->open.sep);
                 if (p_bta_data->open.edr & BTA_AV_EDR_3MBPS)
                 {
                     BTIF_TRACE_DEBUG("remote supports 3 mbps");
                     btif_av_cb[index].edr_3mbps = TRUE;
                 }
#ifdef BTA_AV_SPLIT_A2DP_ENABLED
                 btif_av_cb[index].channel_id = p_bta_data->open.stream_chnl_id;
                 BTIF_TRACE_DEBUG("streaming channel id updated as : 0x%x",
                        btif_av_cb[index].channel_id);
#endif
            }
            else
            {
                BTIF_TRACE_WARNING("BTA_AV_OPEN_EVT::FAILED status: %d",
                                     p_bta_data->open.status );
                /* Multicast: Check if connected to AVRC only device
                 * disconnect when Dual A2DP/Multicast is supported.
                 */
                BD_ADDR peer_addr;
                if ((btif_rc_get_connected_peer(peer_addr))
                    &&(!bdcmp(btif_av_cb[index].peer_bda.address, peer_addr)))
                {
                    /* Do not disconnect AVRCP connection if A2DP
                     * connection failed due to SDP failure since remote
                     * may not support A2DP. In such case we will keep
                     * AVRCP only connection.
                     */
                    if (p_bta_data->open.status != BTA_AV_FAIL_SDP)
                    {
                        BTIF_TRACE_WARNING("Disconnecting AVRCP ");
                        BTA_AvCloseRc(btif_rc_get_connected_peer_handle(peer_addr));
                    }
                    else
                    {
                        BTIF_TRACE_WARNING("Keep AVRCP only connection");
                    }
                }
                state = BTAV_CONNECTION_STATE_DISCONNECTED;
                av_state  = BTIF_AV_STATE_IDLE;
            }

            /* inform the application of the event */
            btif_report_connection_state(state, &(btif_av_cb[index].peer_bda));
            /* change state to open/idle based on the status */
            btif_sm_change_state(btif_av_cb[index].sm_handle, av_state);
            /* Check if the other connected AV is playing,
            * If YES, trigger DUAL Handoff. */
            if (p_bta_data->open.status == BTA_AV_SUCCESS)
            {
                /* BTIF AV State updated, now check
                 * and update multicast state
                 */
                btif_av_update_multicast_state(index);

                /*This device should be now ready for all next playbacks*/
                btif_av_cb[index].current_playing = TRUE;
                if (enable_multicast == FALSE)
                {
                    for (i = 0; i < btif_max_av_clients; i++)
                    {
                        //Other device is not current playing
                        if (i != index)
                            btif_av_cb[i].current_playing = FALSE;
                    }
                    /* In A2dp Multicast, stack will take care of starting
                     * the stream on newly connected A2dp device. If Handoff
                     * is supported, trigger Handoff here. */
                    if (btif_av_is_playing())
                    {
                        BTIF_TRACE_DEBUG("Trigger Dual A2dp Handoff on %d", index);
                        btif_av_trigger_dual_handoff(TRUE, btif_av_cb[index].peer_bda.address);
                    }
                }
                if (btif_av_cb[index].peer_sep == AVDT_TSEP_SNK)
                {
                    /* if queued PLAY command,  send it now */
                    btif_rc_check_handle_pending_play(p_bta_data->open.bd_addr,
                                (p_bta_data->open.status == BTA_AV_SUCCESS));
                }
                else if (btif_av_cb[index].peer_sep == AVDT_TSEP_SRC)
                {
                    /* if queued PLAY command,  send it now */
                    btif_rc_check_handle_pending_play(p_bta_data->open.bd_addr, FALSE);
                    /* Bring up AVRCP connection too */
                    BTA_AvOpenRc(btif_av_cb[index].bta_handle);
                }
            }
            btif_queue_advance();
        } break;

        case BTIF_AV_SINK_CONFIG_REQ_EVT:
        {
            btif_av_sink_config_req_t req;
            // copy to avoid alignment problems
            memcpy(&req, p_data, sizeof(req));

            BTIF_TRACE_DEBUG("BTIF_AV_SINK_CONFIG_REQ_EVT %d %d", req.sample_rate,
                    req.channel_count);
            if (bt_av_sink_callbacks != NULL) {
                HAL_CBACK(bt_av_sink_callbacks, audio_config_cb, &(btif_av_cb[index].peer_bda),
                        req.sample_rate, req.channel_count);
            }
        } break;

        case BTIF_AV_CONNECT_REQ_EVT:
            // Check for device, if same device which moved to opening then ignore callback
            if (memcmp ((bt_bdaddr_t*)p_data, &(btif_av_cb[index].peer_bda),
                sizeof(btif_av_cb[index].peer_bda)) == 0)
            {
                BTIF_TRACE_DEBUG("%s: Same device moved to Opening state,ignore Connect Req", __func__);
                btif_queue_advance();
                break;
            }
            else
            {
                BTIF_TRACE_DEBUG("%s: Moved from idle by Incoming Connection request", __func__);
                btif_report_connection_state(BTAV_CONNECTION_STATE_DISCONNECTED, (bt_bdaddr_t*)p_data);
                btif_queue_advance();
                break;
            }

        case BTA_AV_PENDING_EVT:
            // Check for device, if same device which moved to opening then ignore callback
            if (memcmp (((tBTA_AV*)p_data)->pend.bd_addr, &(btif_av_cb[index].peer_bda),
                sizeof(btif_av_cb[index].peer_bda)) == 0)
            {
                BTIF_TRACE_DEBUG("%s: Same device moved to Opening state,ignore Pending Req", __func__);
                break;
            }
            else
            {
                BTIF_TRACE_DEBUG("%s: Moved from idle by outgoing Connection request", __func__);
                BTA_AvDisconnect(((tBTA_AV*)p_data)->pend.bd_addr);
                break;
            }

        case BTIF_AV_OFFLOAD_START_REQ_EVT:
            btif_a2dp_on_offload_started(BTA_AV_FAIL);
            BTIF_TRACE_ERROR("BTIF_AV_OFFLOAD_START_REQ_EVT: Stream not Started OPENING");
            break;

        case BTA_AV_CLOSE_EVT:
            /* avdtp link is closed */
            /* Check if any other device is playing
            * and this is not the one.*/
            if (!btif_av_is_playing())
            {
                btif_a2dp_on_stopped(NULL);
            }
            /* inform the application that we are disconnected */
            btif_report_connection_state(BTAV_CONNECTION_STATE_DISCONNECTED,
                    &(btif_av_cb[index].peer_bda));
            btif_sm_change_state(btif_av_cb[index].sm_handle, BTIF_AV_STATE_IDLE);
            break;

        case BTIF_AV_DISCONNECT_REQ_EVT:
            btif_report_connection_state(BTAV_CONNECTION_STATE_DISCONNECTED,
                &(btif_av_cb[index].peer_bda));
            BTA_AvClose(btif_av_cb[index].bta_handle);
            btif_queue_advance();
            btif_sm_change_state(btif_av_cb[index].sm_handle, BTIF_AV_STATE_IDLE);
            break;

        case BTA_AV_RC_OPEN_EVT:
             btif_rc_handler(event, p_data);;
            break;

        case BTA_AV_DELAY_REPORT_EVT:
        {
            tBTA_AV *p_av = (tBTA_AV*)p_data;
            HAL_CBACK(bt_av_src_vendor_callbacks, delay_report_vendor_cb,
            p_av->delay_report.bd_addr, p_av->delay_report.delay_rpt);
        }
            break;

        case BTIF_AV_SRC_CONFIG_REQ_EVT:
        {
            btif_av_src_config_req_t req;
            bdstr_t addr;
            // copy to avoid alignment problems
            /* in this case, L2CAP connection is still up, but bt-app moved to disc state
               so lets move bt-app to connected state first */
            memcpy(&req, p_data, sizeof(req));
            BTIF_TRACE_WARNING("BTIF_AV_SRC_CONFIG_REQ_EVT %s %d",
                    bdaddr_to_string((bt_bdaddr_t *)req.peer_bd,
                    &addr, sizeof(addr)), req.codec_type);

            if (bt_av_src_vendor_callbacks != NULL) {
                BTIF_TRACE_DEBUG("Calling audio_codec_config callback ");
                HAL_CBACK(bt_av_src_vendor_callbacks, audio_codec_config_vendor_cb,
                        &(req.peer_bd), req.codec_type, req.codec_info);
            }
        }
            break;

        CHECK_RC_EVENT(event, p_data);

        default:
            BTIF_TRACE_WARNING("%s : unhandled event:%s", __FUNCTION__,
                                dump_av_sm_event_name(event));
            return FALSE;

   }
   return TRUE;
}

/*****************************************************************************
**
** Function        btif_av_state_closing_handler
**
** Description     Intermediate state managing events during closing
**                 of avdtp channel
**
** Returns         TRUE if event was processed, FALSE otherwise
**
*******************************************************************************/

static BOOLEAN btif_av_state_closing_handler(btif_sm_event_t event, void *p_data, int index)
{
    BTIF_TRACE_IMP("%s event:%s flags %x and index = %d", __FUNCTION__,
                     dump_av_sm_event_name(event), btif_av_cb[index].flags, index);

    switch (event)
    {
        case BTIF_SM_ENTER_EVT:
            if (btif_av_cb[index].peer_sep == AVDT_TSEP_SNK)
            {
                /* Multicast/Soft Hand-off:
                 * If MC/SHO is enabled we need to keep/start playing on
                 * other device.
                 */
                if (btif_av_is_connected_on_other_idx(index))
                {
                    if (btif_av_is_playing())
                    {
                        APPL_TRACE_DEBUG("Keep playing on other device");
                    }
                    else
                    {
                        APPL_TRACE_DEBUG("Not playing on other devie: Set Flush");
                        btif_a2dp_set_tx_flush(TRUE);
                    }
                }
                else
                {
                    /* Single connections scenario:
                     * Immediately stop transmission of frames
                     * wait for audioflinger to stop a2dp
                     */
                    btif_a2dp_set_tx_flush(TRUE);
                }
            }
            if (btif_av_cb[index].peer_sep == AVDT_TSEP_SRC)
            {
                btif_a2dp_set_rx_flush(TRUE);
            }
            break;

        case BTA_AV_STOP_EVT:
        case BTIF_AV_STOP_STREAM_REQ_EVT:
            if (btif_av_cb[index].peer_sep == AVDT_TSEP_SNK)
            {
                /* Dont stop in DUAL A2dp connections, as
                * UIPC will keep waiting for Audio CTRL channel
                * to get closed which is not required in Dual A2dp.
                * We will stop only when only single A2dp conn is present.*/
                if (btif_av_is_connected_on_other_idx(index))
                {
                    if (!btif_av_is_playing())
                    {
                        APPL_TRACE_WARNING("Suspend the AV Data channel");
                        //Flush and close media channel
                        btif_a2dp_set_tx_flush(TRUE);
                        btif_media_task_stop_aa_req();
                    }
                }
                else
                {
                    /* immediately flush any pending tx frames while suspend is pending */
                    APPL_TRACE_WARNING("Stop the AV Data channel");
                    btif_a2dp_set_tx_flush(TRUE);
                    btif_a2dp_on_stopped(NULL);
                }
            }
            if (btif_av_cb[index].peer_sep == AVDT_TSEP_SRC)
            {
                btif_a2dp_set_rx_flush(TRUE);
                btif_a2dp_on_stopped(NULL);
            }
            break;

        case BTIF_SM_EXIT_EVT:
            break;

        case BTA_AV_CLOSE_EVT:

            /* inform the application that we are disconnecting */
            btif_report_connection_state(BTAV_CONNECTION_STATE_DISCONNECTED, &(btif_av_cb[index].peer_bda));

            btif_sm_change_state(btif_av_cb[index].sm_handle, BTIF_AV_STATE_IDLE);
            break;

        /* Handle the RC_CLOSE event for the cleanup */
        case BTA_AV_RC_CLOSE_EVT:
            btif_rc_handler(event, (tBTA_AV*)p_data);
            break;

        case BTIF_AV_OFFLOAD_START_REQ_EVT:
            btif_a2dp_on_offload_started(BTA_AV_FAIL);
            BTIF_TRACE_ERROR("BTIF_AV_OFFLOAD_START_REQ_EVT: Stream not Started Closing");
            break;

        default:
            BTIF_TRACE_WARNING("%s : unhandled event:%s", __FUNCTION__,
                                dump_av_sm_event_name(event));
            return FALSE;
   }
   return TRUE;
}

/*****************************************************************************
**
** Function     btif_av_state_opened_handler
**
** Description  Handles AV events while AVDTP is in OPEN state
**
** Returns      TRUE if event was processed, FALSE otherwise
**
*******************************************************************************/

static BOOLEAN btif_av_state_opened_handler(btif_sm_event_t event, void *p_data, int index)
{
    tBTA_AV *p_av = (tBTA_AV*)p_data;

    BTIF_TRACE_IMP("%s event:%s flags %x and index = %d", __FUNCTION__,
                     dump_av_sm_event_name(event), btif_av_cb[index].flags, index);

    if ((event == BTA_AV_REMOTE_CMD_EVT) &&
         (p_av->remote_cmd.rc_id == BTA_AV_RC_PLAY) )
    {
        for (int i = 0; i < btif_max_av_clients; i++)
        {
            if (btif_av_cb[i].flags & BTIF_AV_FLAG_REMOTE_SUSPEND)
            {
                BTIF_TRACE_EVENT("%s: Resetting remote suspend flag on RC PLAY",
                        __FUNCTION__);
                btif_av_cb[i].flags &= ~BTIF_AV_FLAG_REMOTE_SUSPEND;
            }
        }
    }

    switch (event)
    {
        case BTIF_SM_ENTER_EVT:
            btif_av_cb[index].flags &= ~BTIF_AV_FLAG_PENDING_STOP;
            btif_av_cb[index].flags &= ~BTIF_AV_FLAG_PENDING_START;
            break;

        case BTIF_SM_EXIT_EVT:
            break;

        case BTIF_AV_START_STREAM_REQ_EVT:
            /* update multicast state here if new device is connected
             * after A2dp connection. New A2dp device is connected
             * whlie playing */
            btif_av_update_multicast_state(index);
            if (btif_av_cb[index].peer_sep == AVDT_TSEP_SRC)
            {
                BTA_AvStart(btif_av_cb[index].bta_handle);
                btif_av_cb[index].flags |= BTIF_AV_FLAG_PENDING_START;
                break;
            }
            tBTIF_STATUS status = btif_a2dp_setup_codec(btif_av_cb[index].bta_handle);
            if (status == BTIF_SUCCESS)
            {
                int idx = 0;
                BTA_AvStart(btif_av_cb[index].bta_handle);
                if (enable_multicast == TRUE)
                {
                    /* In A2dp Multicast, DUT initiated stream request
                    * should be true for all connected A2dp devices. */
                    for (; idx < btif_max_av_clients; idx++)
                    {
                        btif_av_cb[idx].flags |= BTIF_AV_FLAG_PENDING_START;
                    }
                }
                else
                {
                    btif_av_cb[index].flags |= BTIF_AV_FLAG_PENDING_START;
                }
            }
            else if (status == BTIF_ERROR_SRV_AV_CP_NOT_SUPPORTED)
            {
#if defined(BTA_AV_DISCONNECT_IF_NO_SCMS_T) && (BTA_AV_DISCONNECT_IF_NO_SCMS_T == TRUE)
                BTIF_TRACE_ERROR0("SCMST enabled, disconnect as remote does not support SCMST");
                BTA_AvDisconnect(btif_av_cb[index].peer_bda.address);
#else
                BTIF_TRACE_WARNING("SCMST enabled, connecting to non SCMST SEP");
                BTA_AvStart(btif_av_cb[index].bta_handle);
                btif_av_cb[index].flags |= BTIF_AV_FLAG_PENDING_START;
#endif
            }
            else
            {
                BTIF_TRACE_ERROR("## AV Disconnect## status : %x",status);
                BTA_AvDisconnect(btif_av_cb[index].peer_bda.address);
            }
            break;

        case BTIF_AV_UPDATE_ENCODER_REQ_EVT:
            btif_a2dp_update_codec();
            break;

        case BTIF_AVK_START_STREAM_REQ_EVT:
            // TODO: check if AVRCP connection is there, send AVRCP_PLAY, otherwise send AVDTP_START
            // check if rc connected
            if(btif_rc_get_connected_peer_handle(btif_av_cb[index].peer_bda.address))
            {
                // send avrcp play
                btif_rc_ctrl_send_play(&btif_av_cb[index].peer_bda);
            }
            else
            {
                // send AVDTP START
                btif_av_cb[index].flags |= BTIF_AV_FLAG_PENDING_START;
                BTA_AvStart(btif_av_cb[index].bta_handle);
            }
            break;
        case BTA_AV_START_EVT:
        {
            BTIF_TRACE_DEBUG("BTA_AV_START_EVT status %d, suspending %d, init %d",
                p_av->start.status, p_av->start.suspending, p_av->start.initiator);
            BTIF_TRACE_DEBUG("BTA_AV_START_EVT role: %d", p_av->start.role);
            if (p_av->start.role == HOST_ROLE_SLAVE)
            {
                btif_av_cb[index].is_slave = TRUE;
            }
            else
            {
                // update if we are master after role switch before start
                btif_av_cb[index].is_slave = FALSE;
            }
            /* There can be role switch after device is connected,
             * hence check for role before starting multicast, and
             * disable if we are in slave role for any connection
             */
            btif_av_update_multicast_state(index);

            if ((p_av->start.status == BTA_SUCCESS) && (p_av->start.suspending == TRUE))
                return TRUE;

            // TODO: SRC, check for hf_is_call_idle
            /* if remote tries to start a2dp when DUT is a2dp source
             * then suspend. In case a2dp is sink and call is active
             * then disconnect the AVDTP channel
             */
            if (!(btif_av_cb[index].flags & BTIF_AV_FLAG_PENDING_START))
            {
                if (btif_av_cb[index].peer_sep == AVDT_TSEP_SNK)
                {
                    if (enable_multicast)
                    {
                        /* Stack will start the playback on newly connected
                         * A2dp device, if the playback is already happening on
                         * other connected device.*/
                        if (btif_av_is_playing())
                        {
                            /* when HS2 is connected during HS1 playing, stack directly
                             * sends start event hence update encoder so that least L2CAP
                             *  MTU is selected.*/
                            BTIF_TRACE_DEBUG("%s: A2dp Multicast playback",
                                    __FUNCTION__);
                        }
                        /* initiate suspend if start is initiate by remote and multicast
                         * is enabled.
                         * Avoid suspend if stream is started as quick suspend-start
                         * creates IOT issue, seen with SBH50.
                         */

                        if (!p_av->start.initiator && !btif_av_is_playing())
                        {
                            BTIF_TRACE_DEBUG("initiate suspend for remote start");
                            btif_dispatch_sm_event(BTIF_AV_SUSPEND_STREAM_REQ_EVT, NULL, 0);
                        }
                    }
                    else
                    {
                        if ((btif_av_cb[index].flags & BTIF_AV_FLAG_REMOTE_SUSPEND))
                        {
                            BTIF_TRACE_DEBUG("%s: clear remote suspend flag on remote start",
                                __FUNCTION__);
                            btif_av_cb[index].flags &= ~BTIF_AV_FLAG_REMOTE_SUSPEND;
                        }
                        else
                        {
                            BTIF_TRACE_DEBUG("%s: trigger suspend as remote initiated!!",
                                __FUNCTION__);
                            btif_dispatch_sm_event(BTIF_AV_SUSPEND_STREAM_REQ_EVT, NULL, 0);
                        }
                    }
                }
            }

            /* remain in open state if status failed */
            /* Multicast-soft Handoff:
             * START failed, cleanup Handoff flag.
             */
            if (p_av->start.status != BTA_AV_SUCCESS)
            {
                int i;

                /* In case peer is A2DP SRC we do not want to ack commands on UIPC */
                if (btif_av_cb[index].peer_sep == AVDT_TSEP_SNK)
                {
                    if (btif_a2dp_on_started(&p_av->start,
                        ((btif_av_cb[index].flags & BTIF_AV_FLAG_PENDING_START) != 0),
                        btif_av_cb[index].bta_handle))
                    {
                        /* only clear pending flag after acknowledgement */
                        btif_av_cb[index].flags &= ~BTIF_AV_FLAG_PENDING_START;
                    }
                }
                /* Clear dual handoff flag */
                for (i = 0; i < btif_max_av_clients; i++)
                {
                    btif_av_cb[i].dual_handoff = FALSE;
                }
                return FALSE;
            }

#ifndef AVK_BACKPORT
            if (btif_av_cb[index].peer_sep == AVDT_TSEP_SRC)
            {
                btif_a2dp_set_rx_flush(FALSE); /*  remove flush state, ready for streaming*/
            }
#endif

            btif_sm_change_state(btif_av_cb[index].sm_handle, BTIF_AV_STATE_STARTED);

        } break;

        case BTIF_AV_DISCONNECT_REQ_EVT:
            BTA_AvClose(btif_av_cb[index].bta_handle);
            if (btif_av_cb[index].peer_sep == AVDT_TSEP_SRC) {
                BTA_AvCloseRc(btif_av_cb[index].bta_handle);
            }

            /* inform the application that we are disconnecting */
            btif_report_connection_state(BTAV_CONNECTION_STATE_DISCONNECTING, &(btif_av_cb[index].peer_bda));
            break;

        case BTA_AV_CLOSE_EVT:
             /* avdtp link is closed */
             /*Dont close the A2dp when Dual playback is happening*/
             if (btif_av_is_connected_on_other_idx(index))
             {
                 APPL_TRACE_WARNING("Conn is closing,close AV data channel");
                 if (!btif_av_is_playing())
                 {
                     APPL_TRACE_WARNING("Suspend the AV Data channel");
                     /* ensure tx frames are immediately suspended */
                     btif_a2dp_set_tx_flush(TRUE);
                     btif_media_task_stop_aa_req();
                 }
             }
             else
             {
                 APPL_TRACE_WARNING("Stop the AV Data channel");
                 btif_a2dp_on_stopped(NULL);
             }

            /* inform the application that we are disconnected */
            btif_report_connection_state(BTAV_CONNECTION_STATE_DISCONNECTED,
                                        &(btif_av_cb[index].peer_bda));

            /* change state to idle, send acknowledgement if start is pending */
            if (btif_av_cb[index].flags & BTIF_AV_FLAG_PENDING_START) {
                btif_a2dp_ack_fail();
                btif_av_cb[index].flags &= ~BTIF_AV_FLAG_PENDING_START;
            }

            btif_sm_change_state(btif_av_cb[index].sm_handle, BTIF_AV_STATE_IDLE);
            break;

        case BTA_AV_RECONFIG_EVT:
            if((btif_av_cb[index].flags & BTIF_AV_FLAG_PENDING_START) &&
                (p_av->reconfig.status == BTA_AV_SUCCESS))
            {
               APPL_TRACE_WARNING("reconfig done BTA_AVstart()");
               BTA_AvStart(btif_av_cb[index].bta_handle);
            }
            else if(btif_av_cb[index].flags & BTIF_AV_FLAG_PENDING_START)
            {
               btif_av_cb[index].flags &= ~BTIF_AV_FLAG_PENDING_START;
               btif_a2dp_ack_fail();
            }
            break;

        case BTIF_AV_CONNECT_REQ_EVT:
            if (memcmp ((bt_bdaddr_t*)p_data, &(btif_av_cb[index].peer_bda),
                sizeof(btif_av_cb[index].peer_bda)) == 0)
            {
                BTIF_TRACE_DEBUG("%s: Ignore BTIF_AV_CONNECT_REQ_EVT for same device", __func__);
            }
            else
            {
                BTIF_TRACE_DEBUG("%s: Moved to opened by Other Incoming Conn req", __func__);
                btif_report_connection_state(BTAV_CONNECTION_STATE_DISCONNECTED,
                        (bt_bdaddr_t*)p_data);
            }
            btif_queue_advance();
            break;

        case BTIF_AV_OFFLOAD_START_REQ_EVT:
            btif_a2dp_on_offload_started(BTA_AV_FAIL);
            BTIF_TRACE_ERROR("BTIF_AV_OFFLOAD_START_REQ_EVT: Stream not Started Opened");
            break;

        case BTA_AV_RC_OPEN_EVT:
            btif_av_check_rc_connection_priority(p_data);
            break;

        case BTA_AV_DELAY_REPORT_EVT:
            HAL_CBACK(bt_av_src_vendor_callbacks, delay_report_vendor_cb,
            p_av->delay_report.bd_addr, p_av->delay_report.delay_rpt);
            break;

        case BTIF_AV_SRC_CONFIG_REQ_EVT:
        {
            btif_av_src_config_req_t req;
            bdstr_t addr;
            // copy to avoid alignment problems
            /* in this case, L2CAP connection is still up, but bt-app moved to disc state
               so lets move bt-app to connected state first */
            memcpy(&req, p_data, sizeof(req));
            BTIF_TRACE_WARNING("BTIF_AV_SRC_CONFIG_REQ_EVT %s %d",
                    bdaddr_to_string((bt_bdaddr_t *)req.peer_bd,
                    &addr, sizeof(addr)), req.codec_type);

            if (bt_av_src_vendor_callbacks != NULL) {
                BTIF_TRACE_DEBUG("Calling audio_codec_config callback ");
                HAL_CBACK(bt_av_src_vendor_callbacks, audio_codec_config_vendor_cb,
                        &(req.peer_bd), req.codec_type, req.codec_info);
            }
        }
            break;

        CHECK_RC_EVENT(event, p_data);

        default:
            BTIF_TRACE_WARNING("%s : unhandled event:%s", __FUNCTION__,
                               dump_av_sm_event_name(event));
            return FALSE;

    }
    return TRUE;
}

/*****************************************************************************
**
** Function     btif_av_state_started_handler
**
** Description  Handles AV events while A2DP stream is started
**
** Returns      TRUE if event was processed, FALSE otherwise
**
*******************************************************************************/

static BOOLEAN btif_av_state_started_handler(btif_sm_event_t event, void *p_data, int index)
{
    tBTA_AV *p_av = (tBTA_AV*)p_data;
    btif_sm_state_t state = BTIF_AV_STATE_IDLE;
    int i;

    BTIF_TRACE_IMP("%s event:%s flags %x  index =%d", __FUNCTION__,
                     dump_av_sm_event_name(event), btif_av_cb[index].flags, index);

    switch (event)
    {
        case BTIF_SM_ENTER_EVT:
            /*Ack from entry point of started handler instead of open state to avoid race condition*/
            if (btif_av_cb[index].peer_sep == AVDT_TSEP_SNK)
            {
                if (btif_a2dp_on_started(&p_av->start,
                    ((btif_av_cb[index].flags & BTIF_AV_FLAG_PENDING_START) != 0),
                      btif_av_cb[index].bta_handle))
                {
                    /* only clear pending flag after acknowledgement */
                    btif_av_cb[index].flags &= ~BTIF_AV_FLAG_PENDING_START;
                }
            }

            /* Already changed state to started, send acknowledgement if start is pending */
            if (btif_av_cb[index].flags & BTIF_AV_FLAG_PENDING_START) {
                if (btif_av_cb[index].peer_sep == AVDT_TSEP_SNK)
                    btif_a2dp_on_started(NULL, TRUE, btif_av_cb[index].bta_handle);
                btif_av_cb[index].flags &= ~BTIF_AV_FLAG_PENDING_START;
            }

            /* we are again in started state, clear any remote suspend flags */
            btif_av_cb[index].flags &= ~BTIF_AV_FLAG_REMOTE_SUSPEND;

            btif_report_audio_state(BTAV_AUDIO_STATE_STARTED, &(btif_av_cb[index].peer_bda));
            btif_av_cb[index].is_device_playing = TRUE;

            /* increase the a2dp consumer task priority temporarily when start
            ** audio playing, to avoid overflow the audio packet queue. */
            adjust_priority_a2dp(TRUE);
#ifdef AVK_BACKPORT
            if (btif_av_cb[index].peer_sep == AVDT_TSEP_SRC)
            {
                btif_av_request_audio_focus(TRUE);
            }
#endif
            //Clear Dual Handoff for all SCBs
            for (i = 0; i < btif_max_av_clients; i++)
            {
                btif_av_cb[i].dual_handoff = FALSE;
                //Other device is not current playing
                if (i != index)
                    btif_av_cb[i].current_playing = FALSE;
            }
            //This is latest device to play now
            btif_av_cb[index].current_playing = TRUE;
            break;

        case BTIF_SM_EXIT_EVT:
            /* restore the a2dp consumer task priority when stop audio playing. */
            adjust_priority_a2dp(FALSE);

            break;

        case BTIF_AV_START_STREAM_REQ_EVT:
            /* we were remotely started, just ack back the local request */
            if (btif_av_cb[index].peer_sep == AVDT_TSEP_SNK)
                btif_a2dp_on_started(NULL, TRUE, btif_av_cb[index].bta_handle);
            break;

        case BTIF_AV_UPDATE_ENCODER_REQ_EVT:
            btif_a2dp_update_codec();
            break;

        /* fixme -- use suspend = true always to work around issue with BTA AV */
        case BTIF_AV_STOP_STREAM_REQ_EVT:
        case BTIF_AV_SUSPEND_STREAM_REQ_EVT:

            /* set pending flag to ensure btif task is not trying to restart
             * stream while suspend is in progress.
             * Multicast: If streaming is happening on both devices, we need
             * to update flag for both connections as SUSPEND request will
             * be sent to only one stream as internally BTA takes care of
             * suspending both streams.
             */
            for(i = 0; i < btif_max_av_clients; i++)
            {
                state = btif_sm_get_state(btif_av_cb[i].sm_handle);
                if (state == BTIF_AV_STATE_STARTED)
                {
                    btif_av_cb[i].flags |= BTIF_AV_FLAG_LOCAL_SUSPEND_PENDING;
                }
            }

            /* if we were remotely suspended but suspend locally, local suspend
               always overrides */
            btif_av_cb[index].flags &= ~BTIF_AV_FLAG_REMOTE_SUSPEND;

            if (btif_av_cb[index].peer_sep == AVDT_TSEP_SNK)
            {
            /* immediately stop transmission of frames while suspend is pending */
                btif_a2dp_set_tx_flush(TRUE);
            }

            if (btif_av_cb[index].peer_sep == AVDT_TSEP_SRC) {
                btif_a2dp_set_rx_flush(TRUE);
                btif_a2dp_on_stopped(NULL);
            }

            BTA_AvStop(TRUE, btif_av_cb[index].bta_handle);
            break;

        case BTIF_AVK_SUSPEND_STREAM_REQ_EVT:
            /* this will be sent only in case we are A2DP SINK */
            // check if rc connected
            if(btif_rc_get_connected_peer_handle(btif_av_cb[index].peer_bda.address))
            {
                // send avrcp pause
                btif_av_cb[index].flags &= ~BTIF_AV_FLAG_REMOTE_SUSPEND;
                btif_av_cb[index].flags |= BTIF_AV_FLAG_LOCAL_SUSPEND_PENDING;
                btif_rc_ctrl_send_pause(&btif_av_cb[index].peer_bda);
            }
            else
            {
                // send AVDTP SUSPEND
                btif_av_cb[index].flags &= ~BTIF_AV_FLAG_REMOTE_SUSPEND;
                btif_av_cb[index].flags |= BTIF_AV_FLAG_LOCAL_SUSPEND_PENDING;
                BTA_AvStop(TRUE, btif_av_cb[index].bta_handle);
            }
            break;
        case BTIF_AVK_START_STREAM_REQ_EVT:
            BTIF_TRACE_EVENT("BTIF_AVK_START_STREAM_REQ_EVT  already in started state");
            break;
        case BTIF_AV_DISCONNECT_REQ_EVT:

            //Now it is not the current playing
            btif_av_cb[index].current_playing = FALSE;
            btif_av_update_current_playing_device(index);
            btif_rc_clear_priority(btif_av_cb[index].peer_bda.address);
            if (bt_split_a2dp_enabled && btif_av_is_connected_on_other_idx(index))
            {
               /*Fake handoff state to switch streaming to other coddeced
                  device */
                btif_av_cb[index].dual_handoff = TRUE;
            }
            /* request avdtp to close */
            BTA_AvClose(btif_av_cb[index].bta_handle);
            if (btif_av_cb[index].peer_sep == AVDT_TSEP_SRC) {
                BTA_AvCloseRc(btif_av_cb[index].bta_handle);
            }

            /* inform the application that we are disconnecting */
            btif_report_connection_state(BTAV_CONNECTION_STATE_DISCONNECTING, &(btif_av_cb[index].peer_bda));

            /* wait in closing state until fully closed */
            btif_sm_change_state(btif_av_cb[index].sm_handle, BTIF_AV_STATE_CLOSING);
            if (bt_split_a2dp_enabled &&
                btif_av_is_connected_on_other_idx(index))
            {
                BTIF_TRACE_DEBUG("%s: Notify framework to reconfig",__func__);
                int idx = btif_av_get_other_connected_idx(index);
                /* Fix for below Klockwork Issue
                 * Array 'btif_av_cb' of size 2 may use index value(s) -1 */
                if ((idx != INVALID_INDEX) && (bt_av_src_vendor_callbacks != NULL))
                {
                    HAL_CBACK(bt_av_src_vendor_callbacks, reconfig_a2dp_trigger_cb, 1,
                                                    &(btif_av_cb[idx].peer_bda));
                }
            }
            break;

        case BTA_AV_SUSPEND_EVT:

            BTIF_TRACE_EVENT("BTA_AV_SUSPEND_EVT status %d, init %d",
                 p_av->suspend.status, p_av->suspend.initiator);
            //Check if this suspend is due to DUAL_Handoff
            if ((btif_av_cb[index].dual_handoff) &&
                (p_av->suspend.status == BTA_AV_SUCCESS))
            {
                BTIF_TRACE_EVENT("BTA_AV_SUSPEND_EVT: Dual handoff");
                btif_dispatch_sm_event(BTIF_AV_START_STREAM_REQ_EVT, NULL, 0);
            }
            if (p_av->suspend.initiator != TRUE)
            {
                /* remote suspend, notify HAL and await audioflinger to
                 * suspend/stop stream
                 * set remote suspend flag to block media task from restarting
                 * stream only if we did not already initiate a local suspend
                 * set remote suspend flag before suspending stream as in race conditions
                 * when stream is suspended, but flag is things ge tossed up
                 */
                BTIF_TRACE_EVENT("Clear before suspending");
                if ((btif_av_cb[index].flags & BTIF_AV_FLAG_LOCAL_SUSPEND_PENDING) == 0)
                    btif_av_cb[index].flags |= BTIF_AV_FLAG_REMOTE_SUSPEND;
                for (int i = 0; i < btif_max_av_clients; i++)
                {
                    if ((i != index) && btif_av_get_ongoing_multicast())
                    {
                        multicast_disabled = TRUE;
                        btif_av_update_multicast_state(index);
                        BTIF_TRACE_EVENT("Initiate suspend for other HS also");
                        btif_sm_dispatch(btif_av_cb[i].sm_handle,
                                BTIF_AV_SUSPEND_STREAM_REQ_EVT, NULL);
                    }
                }
            }

            /* a2dp suspended, stop media task until resumed */
            /* Multicast: If streaming on other device, don't call onsuspended
             * as it unblocks the audio process and audio process may send
             * subsequent commands and create problem during the time where we
             * still did not receive response for SUSPEND sent to other device.
             * Keep the suspend failure handling untouched and handle
             * only success case to check and avoid calling onsuspended.
             */
            if ((p_av->suspend.status != BTA_AV_SUCCESS) ||
                !btif_av_is_playing_on_other_idx(index))
            {
                btif_a2dp_on_suspended(&p_av->suspend);
            }
            else if(btif_av_is_playing_on_other_idx(index))
            {
                BTIF_TRACE_DEBUG("Other device not suspended, don't ack the suspend");
            }

            /* if not successful, remain in current state */
            if (p_av->suspend.status != BTA_AV_SUCCESS)
            {
                btif_av_cb[index].flags &= ~BTIF_AV_FLAG_LOCAL_SUSPEND_PENDING;

               if (btif_av_cb[index].peer_sep == AVDT_TSEP_SNK)
               {
                /* suspend failed, reset back tx flush state */
                    btif_a2dp_set_tx_flush(FALSE);
               }
                return FALSE;
            }

            if (p_av->suspend.initiator != TRUE)
            {
                btif_report_audio_state(BTAV_AUDIO_STATE_REMOTE_SUSPEND, &(btif_av_cb[index].peer_bda));
            }
            else
            {
                btif_report_audio_state(BTAV_AUDIO_STATE_REMOTE_SUSPEND, &(btif_av_cb[index].peer_bda));
            }
            btif_av_cb[index].is_device_playing = FALSE;
            btif_sm_change_state(btif_av_cb[index].sm_handle, BTIF_AV_STATE_OPENED);

            /* suspend completed and state changed, clear pending status */
            btif_av_cb[index].flags &= ~BTIF_AV_FLAG_LOCAL_SUSPEND_PENDING;
            break;

#ifdef USE_AUDIO_TRACK
            case BTIF_AV_SINK_FOCUS_REQ_EVT:
                HAL_CBACK(bt_av_sink_vendor_callbacks, audio_focus_request_vendor_cb,
                                                   &(btif_av_cb[index].peer_bda));
            break;
#endif

        case BTA_AV_STOP_EVT:

            btif_av_cb[index].flags |= BTIF_AV_FLAG_PENDING_STOP;
            btif_av_cb[index].current_playing = FALSE;
            if (btif_av_is_connected_on_other_idx(index))
            {
                if (enable_multicast == FALSE)
                {
                    APPL_TRACE_WARNING("other Idx is connected, move to SUSPENDED");
                    if (!bt_split_a2dp_enabled) {
                        btif_rc_send_pause_command();
                    }
                    btif_a2dp_on_stopped(&p_av->suspend);
                }
            }
            else
            {
                APPL_TRACE_WARNING("Stop the AV Data channel as no connection is present");
                btif_a2dp_on_stopped(&p_av->suspend);
            }
            btif_av_cb[index].is_device_playing = FALSE;


            btif_report_audio_state(BTAV_AUDIO_STATE_STOPPED, &(btif_av_cb[index].peer_bda));
            /* if stop was successful, change state to open */
            if (p_av->suspend.status == BTA_AV_SUCCESS)
                btif_sm_change_state(btif_av_cb[index].sm_handle, BTIF_AV_STATE_OPENED);

            if (bt_split_a2dp_enabled &&
                btif_av_is_connected_on_other_idx(index))
            {
               /*Fake handoff state to switch streaming to other coddeced
                  device */
                btif_av_cb[index].dual_handoff = TRUE;
                BTIF_TRACE_DEBUG("%s: Notify framework to reconfig",__func__);
                int idx = btif_av_get_other_connected_idx(index);
                /* Fix for below Klockwork Issue
                 * Array 'btif_av_cb' of size 2 may use index value(s) -1 */
                if ((idx != INVALID_INDEX) && (bt_av_src_vendor_callbacks != NULL))
                {
                    HAL_CBACK(bt_av_src_vendor_callbacks, reconfig_a2dp_trigger_cb, 1,
                                                    &(btif_av_cb[idx].peer_bda));
                }
            }

            break;

        case BTA_AV_CLOSE_EVT:

             btif_av_cb[index].flags |= BTIF_AV_FLAG_PENDING_STOP;

            /* avdtp link is closed */
            APPL_TRACE_WARNING("Stop the AV Data channel");
            btif_a2dp_on_stopped(NULL);

            /* inform the application that we are disconnected */
            btif_report_connection_state(BTAV_CONNECTION_STATE_DISCONNECTED,
                                        &(btif_av_cb[index].peer_bda));

            btif_sm_change_state(btif_av_cb[index].sm_handle, BTIF_AV_STATE_IDLE);
            break;

        case BTA_AV_RC_OPEN_EVT:
            btif_av_check_rc_connection_priority(p_data);
            break;

        case BTIF_AV_OFFLOAD_START_REQ_EVT:
            BTA_AvOffloadStart(btif_av_cb[index].bta_handle);
            break;

        case BTA_AV_OFFLOAD_START_RSP_EVT:
            btif_a2dp_on_offload_started(p_av->status);
            break;

        case BTA_AV_DELAY_REPORT_EVT:
            HAL_CBACK(bt_av_src_vendor_callbacks, delay_report_vendor_cb,
            p_av->delay_report.bd_addr, p_av->delay_report.delay_rpt);
            break;

        CHECK_RC_EVENT(event, p_data);

        default:
            BTIF_TRACE_WARNING("%s : unhandled event:%s", __FUNCTION__,
                                 dump_av_sm_event_name(event));
            return FALSE;

    }
    return TRUE;
}

void btif_av_event_deep_copy(UINT16 event, char *p_dest, char *p_src)
{
    tBTA_AV *av_src = (tBTA_AV *)p_src;
    tBTA_AV *av_dest = (tBTA_AV *)p_dest;

    // First copy the structure
    maybe_non_aligned_memcpy(av_dest, av_src, sizeof(*av_src));

    switch (event)
    {
        case BTA_AV_META_MSG_EVT:
            if (av_src->meta_msg.p_data && av_src->meta_msg.len)
            {
                av_dest->meta_msg.p_data = osi_calloc(av_src->meta_msg.len);
                memcpy(av_dest->meta_msg.p_data, av_src->meta_msg.p_data,
                       av_src->meta_msg.len);
            }

            if (av_src->meta_msg.p_msg)
            {
                av_dest->meta_msg.p_msg = osi_calloc(sizeof(tAVRC_MSG));
                memcpy(av_dest->meta_msg.p_msg, av_src->meta_msg.p_msg,
                       sizeof(tAVRC_MSG));

                if (av_src->meta_msg.p_msg->vendor.p_vendor_data &&
                    av_src->meta_msg.p_msg->vendor.vendor_len)
                {
                    av_dest->meta_msg.p_msg->vendor.p_vendor_data = osi_calloc(
                        av_src->meta_msg.p_msg->vendor.vendor_len);
                    memcpy(av_dest->meta_msg.p_msg->vendor.p_vendor_data,
                        av_src->meta_msg.p_msg->vendor.p_vendor_data,
                        av_src->meta_msg.p_msg->vendor.vendor_len);
                }
            }
            break;
        case BTA_AV_BROWSE_MSG_EVT:
            if (av_src->browse_msg.p_msg)
            {
                av_dest->browse_msg.p_msg = osi_calloc(sizeof(tAVRC_MSG));
                assert(av_dest->browse_msg.p_msg);
                memcpy(av_dest->browse_msg.p_msg, av_src->browse_msg.p_msg, sizeof(tAVRC_MSG));

                if (av_src->browse_msg.p_msg->browse.p_browse_data &&
                    av_src->browse_msg.p_msg->browse.browse_len)
                {
                    av_dest->browse_msg.p_msg->browse.p_browse_data = osi_calloc(
                        av_src->browse_msg.p_msg->browse.browse_len);
                    assert(av_dest->browse_msg.p_msg->browse.p_browse_data);
                    memcpy(av_dest->browse_msg.p_msg->browse.p_browse_data,
                        av_src->browse_msg.p_msg->browse.p_browse_data,
                        av_src->browse_msg.p_msg->browse.browse_len);
                }
            }
            break;

        default:
            break;
    }
}

static void btif_av_event_free_data(btif_sm_event_t event, void *p_data)
{
    switch (event)
    {
        case BTA_AV_META_MSG_EVT:
            {
                tBTA_AV *av = (tBTA_AV *)p_data;
                osi_free_and_reset((void **)&av->meta_msg.p_data);

                if (av->meta_msg.p_msg) {
                    osi_free(av->meta_msg.p_msg->vendor.p_vendor_data);
                    osi_free_and_reset((void **)&av->meta_msg.p_msg);
                }
            }
            break;
        case BTA_AV_BROWSE_MSG_EVT:
            {
                tBTA_AV *av = (tBTA_AV*)p_data;

                if (av->browse_msg.p_msg)
                {
                    if (av->browse_msg.p_msg->browse.p_browse_data)
                        osi_free(av->browse_msg.p_msg->browse.p_browse_data);
                    osi_free(av->browse_msg.p_msg);
                }
            }
            break;

        default:
            break;
    }
}

/*****************************************************************************
**  Local event handlers
******************************************************************************/

static void btif_av_handle_event(UINT16 event, char* p_param)
{
    int index = 0;
    tBTA_AV *p_bta_data = (tBTA_AV*)p_param;
    bt_bdaddr_t * bt_addr, bt_addr1;
    UINT8 role;
    int uuid;
    btif_av_src_config_req_t config_req;

    switch (event)
    {
        case BTIF_AV_INIT_REQ_EVT:
            BTIF_TRACE_IMP("%s: BTIF_AV_INIT_REQ_EVT", __FUNCTION__);
            if(btif_a2dp_start_media_task())
                btif_a2dp_on_init();
            break;
        /*events from Upper layer and Media Task*/
        case BTIF_AV_CLEANUP_REQ_EVT: /*Clean up to be called on default index*/
            BTIF_TRACE_IMP("%s: BTIF_AV_CLEANUP_REQ_EVT", __FUNCTION__);
            uuid = (int)*p_param;
            if (uuid == BTA_A2DP_SOURCE_SERVICE_ID)
            {
                if (bt_av_src_callbacks)
                {
                    bt_av_src_callbacks = NULL;
                    if (bt_av_sink_callbacks != NULL)
                        break;
                }
            }
            else
            {
                if (bt_av_sink_callbacks)
                {
                    bt_av_sink_callbacks = NULL;
                    if (bt_av_src_callbacks != NULL)
                        break;
                }
            }

            btif_a2dp_stop_media_task();
            return;
        case BTIF_AV_CONNECT_REQ_EVT:
            break;
        case BTIF_AV_DISCONNECT_REQ_EVT:
            /*Bd address passed should help us in getting the handle*/
            bt_addr = (bt_bdaddr_t *)p_param;
            index = btif_av_idx_by_bdaddr(bt_addr->address);
#ifdef BTA_AV_SPLIT_A2DP_ENABLED
            if (bt_split_a2dp_enabled && (btif_av_get_current_playing_dev_idx() == index))
            {
                BTIF_TRACE_DEBUG("%s:Disconnecting playing device,send VS STOP",__func__);
                btif_media_on_stop_vendor_command();
            }
#endif
            break;
        case BTIF_AV_UPDATE_ENCODER_REQ_EVT:
        case BTIF_AV_START_STREAM_REQ_EVT:
            /* Get the last connected device on which START can be issued
            * Get the Dual A2dp Handoff Device first, if none is present,
            * go for lastest connected.
            * In A2dp Multicast, the index selected can be any of the
            * connected device. Stack will ensure to START the steaming
            * on both the devices. */
            index = btif_get_latest_device_idx_to_start();
            break;
        case BTIF_AV_STOP_STREAM_REQ_EVT:
        case BTIF_AV_SUSPEND_STREAM_REQ_EVT:
            /*Should be handled by current STARTED*/
#ifdef BTA_AV_SPLIT_A2DP_ENABLED
            if (bt_split_a2dp_enabled)
                btif_media_on_stop_vendor_command();
#endif
            index = btif_get_latest_playing_device_idx();
            break;
        /*Events from the stack, BTA*/
        case BTA_AV_ENABLE_EVT:
            index = 0;
            break;
        case BTA_AV_REGISTER_EVT:
            index = HANDLE_TO_INDEX(p_bta_data->registr.hndl);
            break;
        case BTA_AV_OPEN_EVT:
            index = HANDLE_TO_INDEX(p_bta_data->open.hndl);
            break;
        case BTA_AV_ROLE_CHANGED_EVT:
            index = HANDLE_TO_INDEX(p_bta_data->role_changed.hndl);
            role = p_bta_data->role_changed.new_role;
            BTIF_TRACE_EVENT("Role change: 0x%x: new role: %s",
                p_bta_data->role_changed.hndl, (role == HOST_ROLE_SLAVE) ? "Slave" : "Master");
            if (index >= 0 && index < btif_max_av_clients)
            {
                btif_av_cb[index].is_slave = (role == HOST_ROLE_SLAVE) ? TRUE : FALSE;
                btif_av_update_multicast_state(index);
            }
            else
            {
                BTIF_TRACE_ERROR("%s: Invalid index for connection", __FUNCTION__);
            }
            return;
        case BTA_AV_PENDING_EVT:
            index = HANDLE_TO_INDEX(p_bta_data->pend.hndl);
            break;
        case BTA_AV_REJECT_EVT:
            index = HANDLE_TO_INDEX(p_bta_data->reject.hndl);
            break;
        case BTA_AV_STOP_EVT:
            index = HANDLE_TO_INDEX(p_bta_data->suspend.hndl);
            break;
        case BTA_AV_CLOSE_EVT:
            index = HANDLE_TO_INDEX(p_bta_data->close.hndl);
            break;
        case BTA_AV_START_EVT:
            index = HANDLE_TO_INDEX(p_bta_data->start.hndl);
            break;
        case BTA_AV_RECONFIG_EVT:
            index = HANDLE_TO_INDEX(p_bta_data->reconfig.hndl);
            break;
        case BTA_AV_SUSPEND_EVT:
            index = HANDLE_TO_INDEX(p_bta_data->suspend.hndl);
            break;

        /* Handle all RC events on default index. RC handling should take
         * care of the events. All events come with BD Address
         * Handled well in AV Opening, opened and started state
         * AV Idle handler needs to take care of this event properly.
         */
        case BTA_AV_RC_OPEN_EVT:
            index = btif_av_get_valid_idx_for_rc_events(p_bta_data->rc_open.peer_addr,
                    p_bta_data->rc_open.rc_handle);
            break;
        case BTA_AV_RC_CLOSE_EVT:
        /* If there is no entry in the connection table
         * RC handler has to be called for cleanup.
         * Directly call the RC handler as we cannot
         * associate any AV handle to it.
         */
            index = btif_av_idx_by_bdaddr(p_bta_data->rc_open.peer_addr);
            if (index == btif_max_av_clients)
            {
                btif_rc_handler(event, p_bta_data);
            }
            break;
        /* Let the RC handler decide on these passthrough cmds
         * Use rc_handle to get the active AV device and use that mapping.
         */
        case BTA_AV_REMOTE_CMD_EVT:
        case BTA_AV_VENDOR_CMD_EVT:
        case BTA_AV_META_MSG_EVT:
        case BTA_AV_RC_FEAT_EVT:
        case BTA_AV_BROWSE_MSG_EVT:
            index = 0;
            BTIF_TRACE_EVENT("RC events: on index = %d", index);
            break;
        case BTIF_AV_SRC_CONFIG_REQ_EVT:
            // copy to avoid alignment problems
            memcpy(&config_req, p_param, sizeof(config_req));
            memcpy(&bt_addr1, &(config_req.peer_bd), sizeof(bt_bdaddr_t));
            index = btif_av_idx_by_bdaddr(&bt_addr1.address);
            break;
        default:
            BTIF_TRACE_ERROR("Unhandled event = %d", event);
            break;
    }
    BTIF_TRACE_DEBUG("Handle the AV event = %x on index = %d", event, index);
    if (index >= 0 && index < btif_max_av_clients)
        btif_sm_dispatch(btif_av_cb[index].sm_handle, event, (void*)p_param);
    else
        BTIF_TRACE_ERROR("Unhandled Index = %d", index);
    btif_av_event_free_data(event, p_param);

}

/*******************************************************************************
**
** Function         btif_av_get_valid_idx
**
** Description      Check the validity of the current index for the connection
**
** Returns          Boolean
**
*******************************************************************************/

static BOOLEAN btif_av_get_valid_idx(int idx)
{
    btif_sm_state_t state = btif_sm_get_state(btif_av_cb[idx].sm_handle);
    return ((state == BTIF_AV_STATE_OPENED) ||
            (state ==  BTIF_AV_STATE_STARTED) ||
            (state == BTIF_AV_STATE_OPENING));
}

/*******************************************************************************
**
** Function         btif_av_idx_by_bdaddr
**
** Description      Get the index corresponding to BD addr
**
** Returns          UNIT8
**
*******************************************************************************/

static UINT8 btif_av_idx_by_bdaddr(BD_ADDR bd_addr)
{
    int i;
    for (i = 0; i < btif_max_av_clients; i++)
    {
        if ((bdcmp(bd_addr,
                  btif_av_cb[i].peer_bda.address) == 0))
            return i;
    }
    return i;
}

BOOLEAN btif_av_is_current_device(BD_ADDR address)
{
    UINT8 index;

    index = btif_av_idx_by_bdaddr(address);
    if((index < btif_max_av_clients) && btif_av_cb[index].current_playing)
    {
        return TRUE;
    }
    return FALSE;
}

/*******************************************************************************
**
** Function         btif_get_latest_device_idx_to_start
**
** Description      Get the index of the AV where streaming is to be started
**
** Returns          int
**
*******************************************************************************/

static int btif_get_latest_device_idx_to_start()
{
    int i, j;
    BD_ADDR playing_address;

    /* Get the device which sent PLAY command
     * If found, START on that index.
     */
    memset(playing_address, 0, sizeof(BD_ADDR));
    btif_rc_get_playing_device(playing_address);
    if (bdcmp(playing_address, bd_addr_null) != 0)
    {
        /* Got some valid Playing device.
         * Get the AV index for this device.
         */
        i = btif_av_idx_by_bdaddr(playing_address);
        if (i == btif_max_av_clients)
            return btif_max_av_clients;
        BTIF_TRACE_EVENT("Got some valid Playing device; %d", i);
        /*Clear the Current playing device*/
        for (j = 0; j < btif_max_av_clients; j++)
        {
            if (j != i)
              btif_av_cb[j].current_playing = FALSE;
        }
        /*Clear the Play command in RC*/
        btif_rc_clear_playing_state(FALSE);
        return i;
    }

    /*No playing device, get the latest*/
    for (i = 0; i < btif_max_av_clients; i++)
    {
        if (btif_av_cb[i].current_playing)
            break;
    }
    if (i == btif_max_av_clients)
    {
        BTIF_TRACE_ERROR("Play on default");
        i = 0; /*play on default*/
    }
    return i;
}

/*******************************************************************************
**
** Function         btif_get_latest_playing_device_idx
**
** Description      Get the index of AV where streaming is happening
**
** Returns          int
**
*******************************************************************************/

int btif_get_latest_playing_device_idx()
{
    int i;
    btif_sm_state_t state;
    for (i = 0; i < btif_max_av_clients; i++)
    {
        state = btif_sm_get_state(btif_av_cb[i].sm_handle);
        if (state == BTIF_AV_STATE_STARTED)
        {
            break;
        }
    }
    return i;
}

/*******************************************************************************
**
** Function         btif_av_is_playing
**
** Description      Is AV in streaming state
**
** Returns          BOOLEAN
**
*******************************************************************************/

BOOLEAN btif_av_is_playing()
{
    int i;
    for (i = 0; i < btif_max_av_clients; i++)
    {
        btif_av_cb[i].state = btif_sm_get_state(btif_av_cb[i].sm_handle);
        if (btif_av_cb[i].state == BTIF_AV_STATE_STARTED)
        {
            BTIF_TRACE_EVENT("btif_av_is_playing on index= %d", i);
            return TRUE;
        }
    }
    return FALSE;
}

/*******************************************************************************
**
** Function         btif_get_conn_state_of_device
**
** Description      Returns the state of AV scb
**
** Returns          int
**
*******************************************************************************/

static int btif_get_conn_state_of_device(BD_ADDR address)
{
    btif_sm_state_t state = BTIF_AV_STATE_IDLE;
    int i;
    for (i = 0; i < btif_max_av_clients; i++)
    {
        if ((bdcmp(address,
            btif_av_cb[i].peer_bda.address) == 0))
        {
            state = btif_sm_get_state(btif_av_cb[i].sm_handle);
            BTIF_TRACE_EVENT("BD Found: %02X %02X %02X %02X %02X %02X :state: %s",
                address[5], address[4], address[3],
                address[2], address[1], address[0],
                dump_av_sm_state_name(state));
        }
    }
    return state;
}

/*******************************************************************************
**
** Function         btif_av_get_valid_idx_for_rc_events
**
** Description      gets th valid index for the RC event address
**
** Returns          int
**
*******************************************************************************/

static int btif_av_get_valid_idx_for_rc_events(BD_ADDR bd_addr, int rc_handle)
{
    int index = 0;
    /* First try to find if it is first event in AV IF
    * both the handles would be in IDLE state, pick the first
    * If we get second RC event while processing the priority
    * for the first, reject the second connection. */

    /*Get the index from connected SCBs*/
    index = btif_av_idx_by_bdaddr(bd_addr);
    if (index == btif_max_av_clients)
    {
        /* None of the SCBS matched
        * Allocate free SCB, null address SCB*/
        index = btif_av_idx_by_bdaddr(bd_null);
        BTIF_TRACE_EVENT("btif_av_get_valid_idx_for_rc_events is %d", index);
        if (index >= btif_max_av_clients)
        {
            BTIF_TRACE_EVENT("disconnect only AVRCP device rc_handle %d", rc_handle);
            BTA_AvCloseRc(rc_handle);
        }
    }
    return index;
}

/*******************************************************************************
**
** Function         btif_av_check_rc_connection_priority
**
** Description      Handles Priority callback for RC connections
**
** Returns          void
**
*******************************************************************************/

static void btif_av_check_rc_connection_priority(void *p_data)
{
    bt_bdaddr_t peer_bda;

    /*Check if it is for same AV device*/
    if (btif_av_is_device_connected(((tBTA_AV*)p_data)->rc_open.peer_addr))
    {
        /*AV is connected */
        BTIF_TRACE_DEBUG("AV is connected, process RC connect event");
        btif_rc_handler(BTA_AV_RC_OPEN_EVT, (tBTA_AV*)p_data);
        return;
    }
    BTIF_TRACE_DEBUG("btif_av_check_rc_connection_priority");
    bdcpy(peer_bda.address, ((tBTA_AV*)p_data)->rc_open.peer_addr);

    if (idle_rc_event != 0)
    {
        BTIF_TRACE_DEBUG("Processing another RC Event ");
        return;
    }
    idle_rc_event = BTA_AV_RC_OPEN_EVT;
    memcpy(&idle_rc_data, ((tBTA_AV*)p_data), sizeof(tBTA_AV));
    if (((tBTA_AV*)p_data)->rc_open.status == BTA_AV_SUCCESS)
    {
        BTIF_TRACE_DEBUG("RC conn is success ");
        if (bt_av_src_vendor_callbacks != NULL)
        {
            BTIF_TRACE_DEBUG(" Check Device priority");
            HAL_CBACK(bt_av_src_vendor_callbacks, connection_priority_vendor_cb,
                    &peer_bda);
        }
    }
    else
    {
        idle_rc_event = 0;
        memset(&idle_rc_data, 0, sizeof(tBTA_AV));
    }
    return;
}


static void bte_av_callback(tBTA_AV_EVT event, tBTA_AV *p_data)
{
    btif_transfer_context(btif_av_handle_event, event,
                          (char*)p_data, sizeof(tBTA_AV), btif_av_event_deep_copy);
}

static void bte_av_media_callback(tBTA_AV_EVT event, tBTA_AV_MEDIA *p_data)
{
    btif_sm_state_t state;
    UINT8 que_len;
    tA2D_STATUS a2d_status;
    tA2D_SBC_CIE sbc_cie;
    btif_av_sink_config_req_t config_req;
    btif_av_src_config_req_t config_src_req;
    int index =0;

    if (event == BTA_AV_MEDIA_DATA_EVT)/* Switch to BTIF_MEDIA context */
    {
        state= btif_sm_get_state(btif_av_cb[index].sm_handle);
        if ( (state == BTIF_AV_STATE_STARTED) || /* send SBC packets only in Started State */
             (state == BTIF_AV_STATE_OPENED) )
        {
            que_len = btif_media_sink_enque_buf((BT_HDR *)p_data);
            BTIF_TRACE_DEBUG(" Packets in Que %d",que_len);
        }
        else
            return;
    }

    if (event == BTA_AV_MEDIA_SINK_CFG_EVT) {
        /* send a command to BT Media Task */
        btif_reset_decoder((UINT8*)(p_data->avk_config.codec_info));
        a2d_status = A2D_ParsSbcInfo(&sbc_cie, (UINT8 *)(p_data->avk_config.codec_info), FALSE);
        if (a2d_status == A2D_SUCCESS) {
            /* Switch to BTIF context */
            config_req.sample_rate = btif_a2dp_get_track_frequency(sbc_cie.samp_freq);
            config_req.channel_count = btif_a2dp_get_track_channel_count(sbc_cie.ch_mode);
            memcpy(&config_req.peer_bd,(UINT8*)(p_data->avk_config.bd_addr),
                                                              sizeof(config_req.peer_bd));
            btif_transfer_context(btif_av_handle_event, BTIF_AV_SINK_CONFIG_REQ_EVT,
                                     (char*)&config_req, sizeof(config_req), NULL);
        } else {
            APPL_TRACE_ERROR("ERROR dump_codec_info A2D_ParsSbcInfo fail:%d", a2d_status);
        }
    }
    if (event == BTA_AV_MEDIA_SRC_CFG_EVT) {
        UINT8* config = (UINT8*)(p_data->avk_config.codec_info);
        UINT8 codec_type = config[2];
        tA2D_SBC_CIE sbc_cie;
        tA2D_APTX_CIE aptx_cie;
        /* send a command to BT Media Task */
        config_src_req.codec_type = codec_type;
        memcpy(config_src_req.peer_bd,(UINT8*)(p_data->avk_config.bd_addr),
                                                          sizeof(config_src_req.peer_bd));
        switch(codec_type)
        {
        case BTIF_AV_CODEC_SBC:
            a2d_status = A2D_ParsSbcInfo(&sbc_cie, (UINT8 *)(p_data->avk_config.codec_info), FALSE);
            if (a2d_status == A2D_SUCCESS) {
                /* Switch to BTIF context */
                config_src_req.codec_info.sbc_config.samp_freq = sbc_cie.samp_freq;
                config_src_req.codec_info.sbc_config.ch_mode = sbc_cie.ch_mode;
                config_src_req.codec_info.sbc_config.block_len = sbc_cie.block_len;
                config_src_req.codec_info.sbc_config.alloc_mthd = sbc_cie.alloc_mthd;
                config_src_req.codec_info.sbc_config.max_bitpool = sbc_cie.max_bitpool;
                config_src_req.codec_info.sbc_config.min_bitpool = sbc_cie.min_bitpool;
                config_src_req.codec_info.sbc_config.num_subbands = sbc_cie.num_subbands;
                btif_transfer_context(btif_av_handle_event, BTIF_AV_SRC_CONFIG_REQ_EVT,
                                         (char*)&config_src_req, sizeof(config_src_req), NULL);
            }
            else
            {
                APPL_TRACE_ERROR("ERROR dump_codec_info A2D_ParsSbcInfo fail:%d", a2d_status);
            }
            break;

        case A2D_NON_A2DP_MEDIA_CT:
            a2d_status = A2D_ParsAptxInfo(&aptx_cie, (UINT8 *)(p_data->avk_config.codec_info), FALSE);
            if (a2d_status == A2D_SUCCESS) {
                /* Switch to BTIF context */
                /* setting APTX for Vendor specific codec right now */
                config_src_req.codec_type = A2DP_SOURCE_AUDIO_CODEC_APTX;
                config_src_req.codec_info.aptx_config.vendor_id = aptx_cie.vendorId;
                config_src_req.codec_info.aptx_config.codec_id = aptx_cie.codecId;
                config_src_req.codec_info.aptx_config.sampling_freq = aptx_cie.sampleRate;
                config_src_req.codec_info.aptx_config.channel_count = aptx_cie.channelMode;
                btif_transfer_context(btif_av_handle_event, BTIF_AV_SRC_CONFIG_REQ_EVT,
                                     (char*)&config_src_req, sizeof(config_src_req), NULL);
            } else {
                APPL_TRACE_ERROR("ERROR dump_codec_info A2D_ParsAptxInfo fail:%d", a2d_status);
            }
            break;
        }
    }}

/******************************************************************************
** Function       a2dp_offload_codec_cap_parser
**
** Description    Parse the offload supported codec capability during init
**
** Returns
*****************************************************************************/
static void a2dp_offload_codec_cap_parser(const char *value)
{
    char *tok = NULL;
    char *tmp_token = NULL;
    /* Fix for below Klockwork Issue
     * 'strtok' has been deprecated; replace it with a safe function. */
    tok = strtok_r((char*)value, "-", &tmp_token);
    while (tok != NULL)
    {
        if (strcmp(tok,"sbc") == 0)
        {
            BTIF_TRACE_ERROR("%s: SBC offload supported",__func__);
            btif_av_codec_offload.sbc_offload = TRUE;
        }
        else if (strcmp(tok,"aptx") == 0)
        {
            BTIF_TRACE_ERROR("%s: aptX offload supported",__func__);
            btif_av_codec_offload.aptx_offload = TRUE;
        }
        else if (strcmp(tok,"aac") == 0)
        {
            BTIF_TRACE_ERROR("%s: AAC offload supported",__func__);
            btif_av_codec_offload.aac_offload = TRUE;
        }
        else if (strcmp(tok,"aptxhd") == 0)
        {
            BTIF_TRACE_ERROR("%s: APTXHD offload supported",__func__);
            btif_av_codec_offload.aptxhd_offload = TRUE;
        }
        tok = strtok_r(NULL, "-", &tmp_token);
    };
}

/******************************************************************************
** Function       get_offload_codec_capabilities
**
** Description    Read offload supported codecs
**                To set offload capabilities:
**                adb shell setprop persist.bt.a2dp_offload_cap "sbc-aptx"
**
** Returns
*****************************************************************************/
static void get_offload_codec_capabilities(const char* codec_cap)
{
    BTIF_TRACE_DEBUG("%s",__func__);
    a2dp_offload_codec_cap_parser(codec_cap);
    return;
}
/*******************************************************************************
**
** Function         btif_av_init
**
** Description      Initializes btif AV if not already done
**
** Returns          bt_status_t
**
*******************************************************************************/

bt_status_t btif_av_init(int service_id)
{
    int i;
    if (btif_av_cb[0].sm_handle == NULL)
    {
        alarm_free(av_open_on_rc_timer);
        av_open_on_rc_timer = alarm_new("btif_av.av_open_on_rc_timer");
        BTIF_TRACE_DEBUG("%s", __FUNCTION__);
        if(!btif_a2dp_is_media_task_stopped())
            return BT_STATUS_FAIL;
        btif_av_cb[0].service = service_id;

        /* Also initialize the AV state machine */
        for (i = 0; i < btif_max_av_clients; i++)
        {
            btif_av_cb[i].sm_handle = btif_sm_init((const btif_sm_handler_t*)btif_av_state_handlers,
                                                    BTIF_AV_STATE_IDLE, i);
        }

        btif_transfer_context(btif_av_handle_event, BTIF_AV_INIT_REQ_EVT,
                (char*)&service_id, sizeof(int), NULL);

        btif_enable_service(service_id);
    }

    return BT_STATUS_SUCCESS;
}

/*******************************************************************************
**
** Function         init_src
**
** Description      Initializes the AV interface for source mode
**
** Returns          bt_status_t
**
*******************************************************************************/

static bt_status_t init_src(btav_callbacks_t* callbacks)
{
    bt_status_t status;

    BTIF_TRACE_EVENT("%s ", __FUNCTION__);

    if (bt_av_sink_callbacks != NULL) {
        // already did btif_av_init()
        status = BT_STATUS_SUCCESS;
    }
    else
    {
        /* initializing mutex for src codec */
        pthread_mutex_init(&src_codec_q_lock, NULL);
        pthread_mutex_lock(&src_codec_q_lock);
        BTIF_TRACE_EVENT("%s p_bta_av_codec_pri_list = %x", __FUNCTION__, p_bta_av_codec_pri_list);
        if (p_bta_av_codec_pri_list == NULL) {
            int i = 0;
            bta_av_num_codec_configs = BTIF_SV_AV_AA_SRC_SEP_INDEX;
            p_bta_av_codec_pri_list = osi_calloc(bta_av_num_codec_configs *
                sizeof(tBTA_AV_CO_CODEC_CAP_LIST));
            if (p_bta_av_codec_pri_list != NULL) {
                /* Set default priorty order as APTX (Classic) > SBC */
                p_bta_av_codec_pri_list[i].codec_type = A2D_NON_A2DP_MEDIA_CT;
                BTIF_TRACE_EVENT("%s copying aptx codec at index %d", __FUNCTION__, i);
                memcpy(&p_bta_av_codec_pri_list[i++].codec_cap.aptx_caps,
                    &bta_av_co_aptx_caps, sizeof(tA2D_APTX_CIE));
                memcpy(&bta_av_supp_codec_cap[BTIF_SV_AV_AA_APTX_INDEX].codec_cap.aptx_caps,
                    &bta_av_co_aptx_caps, sizeof(tA2D_APTX_CIE));
                p_bta_av_codec_pri_list[i].codec_type = A2D_MEDIA_CT_SBC;
                BTIF_TRACE_EVENT("%s copying sbc codec at index %d", __FUNCTION__, i);
                memcpy(&p_bta_av_codec_pri_list[i++].codec_cap.sbc_caps,
                    &bta_av_co_sbc_caps, sizeof(tA2D_SBC_CIE));
                memcpy(&bta_av_supp_codec_cap[BTIF_SV_AV_AA_SBC_INDEX].codec_cap.sbc_caps,
                    &bta_av_co_sbc_caps, sizeof(tA2D_SBC_CIE));
            }
        }
        pthread_mutex_unlock(&src_codec_q_lock);
        status = btif_av_init(BTA_A2DP_SOURCE_SERVICE_ID);
    }

    if (status == BT_STATUS_SUCCESS) {
        bt_av_src_callbacks = callbacks;
    }

    return status;
}

/*******************************************************************************
**
** Function         init_sink
**
** Description      Initializes the AV interface for sink mode
**
** Returns          bt_status_t
**
*******************************************************************************/

static bt_status_t init_sink(btav_callbacks_t* callbacks)
{
    bt_status_t status;

    BTIF_TRACE_EVENT("%s", __FUNCTION__);

    if (bt_av_src_callbacks != NULL) {
        // already did btif_av_init()
        status = BT_STATUS_SUCCESS;
    }
    else
    {
        status = btif_av_init(BTA_A2DP_SINK_SERVICE_ID);
    }

    if (status == BT_STATUS_SUCCESS) {
        bt_av_sink_callbacks = callbacks;
        //BTA_AvEnable_Sink(TRUE);
    }

    return status;
}

static bool get_src_codec_config(uint8_t * codecinfo, uint8_t *codectype)
{
    UINT16 min_mtu;
    if (btif_av_stream_started_ready() == FALSE)
    {
        BTIF_TRACE_ERROR("A2DP_CTRL_GET_CODEC_CONFIG: stream not started");
        //return false;
    }
    *codectype = bta_av_co_get_current_codec();
    if(*codectype == BTIF_AV_CODEC_NONE)
        return false;
    else if(*codectype == BTIF_AV_CODEC_SBC)
        bta_av_co_audio_get_sbc_config(codecinfo,&min_mtu);
    else
        memcpy(codecinfo,bta_av_co_get_current_codecInfo(),AVDT_CODEC_SIZE);
    return true;
}

/*******************************************************************************
**
** Function         init_src_vendor
**
** Description      Initializes the AV interface for source vendor mode
**
** Returns          bt_status_t
**
*******************************************************************************/

static bt_status_t init_src_vendor(btav_vendor_callbacks_t* callbacks, int max_a2dp_connections,
                            int a2dp_multicast_state, uint8_t streaming_prarm, const char* offload_cap)
{
    bt_status_t status;

    BTIF_TRACE_EVENT("%s with max conn = %d", __FUNCTION__, max_a2dp_connections);

    if (bt_av_sink_callbacks != NULL) {
        // already did btif_av_init()
        status = BT_STATUS_SUCCESS;
    }
    else
    {
        if (a2dp_multicast_state)
        {
            is_multicast_supported = TRUE;
        }
        if (offload_cap)
        {
            bt_split_a2dp_enabled = TRUE;
            get_offload_codec_capabilities(offload_cap);
            is_multicast_supported = FALSE; //Disable multicast in Split A2dp mode
        }
        btif_max_av_clients = max_a2dp_connections;
        if (bt_av_src_callbacks != NULL)
            status = BT_STATUS_SUCCESS;
    }

    enable_delay_reporting = streaming_prarm & A2DP_SRC_ENABLE_DELAY_REPORTING;
    BTIF_TRACE_DEBUG(" ~~ enable_delay_reporting = %d", enable_delay_reporting);
    if (status == BT_STATUS_SUCCESS) {
        bt_av_src_vendor_callbacks = callbacks;
    }

    return status;
}

/*******************************************************************************
**
** Function         init_sink_vendor
**
** Description      Initializes the AV interface for sink vendor mode
**
** Returns          bt_status_t
**
*******************************************************************************/
static bt_status_t init_sink_vendor(btav_vendor_callbacks_t* callbacks, int max,
                             int a2dp_multicast_state)
{
    bt_status_t status;

    BTIF_TRACE_EVENT("%s", __FUNCTION__);

    if (bt_av_src_callbacks != NULL) {
        // already did btif_av_init()
        status = BT_STATUS_SUCCESS;
    }
    else
    {
        enable_multicast = FALSE; // Clear multicast flag for sink
        if (max > 1)
        {
            BTIF_TRACE_ERROR("Only one Sink can be initialized");
            max = 1;
        }
        btif_max_av_clients = max; //Should be 1
        if (bt_av_sink_callbacks != NULL)
            status = BT_STATUS_SUCCESS;
    }

    if (status == BT_STATUS_SUCCESS) {
        bt_av_sink_vendor_callbacks = callbacks;
        //BTA_AvEnable_Sink(TRUE);
    }

    /* initializing mutex for sink */
    pthread_mutex_init(&pcm_queue_lock, NULL);

    return status;
}

#ifdef USE_AUDIO_TRACK
/*******************************************************************************
**
** Function         audio_focus_status_vendor
**
** Description      Updates the final focus state reported by components calling
**                  this module.
**
** Returns          None
**
*******************************************************************************/
void audio_focus_status_vendor(int state)
{
    BTIF_TRACE_DEBUG("%s state %d ",__func__, state);
    btif_a2dp_set_audio_focus_state(state);
}

/*******************************************************************************
**
** Function         update_audio_track_gain
**
** Description      Updates the track gain (used for ducking).
**
** Returns          None
**
*******************************************************************************/
void update_audio_track_gain(float gain)
{
    BTIF_TRACE_DEBUG("%s gain %f ",__func__, gain);
    btif_a2dp_set_audio_track_gain(gain);
}
#endif


void btif_get_latest_playing_device(BD_ADDR address)
{
    int index;
    index = btif_get_latest_playing_device_idx();
    if (index < btif_max_av_clients)
    {
        //copy bdaddrsss
        bdcpy(address, btif_av_cb[index].peer_bda.address);
    }
    else
    {
        bdcpy(address, bd_null);
    }
}

BOOLEAN btif_av_is_device_connected(BD_ADDR address)
{
    btif_sm_state_t state = btif_get_conn_state_of_device(address);

    if ((state == BTIF_AV_STATE_OPENED) ||
        (state == BTIF_AV_STATE_STARTED))
        return TRUE;
    else
        return FALSE;
}

/*This function will trigger remote suspend for currently
* playing device and then initiate START on Handoff device
* whose address is passed as an argument. */
/*******************************************************************************
**
** Function         btif_av_trigger_dual_handoff
**
** Description      Trigger the DUAL HANDOFF
**
** Returns          void
**
*******************************************************************************/

void btif_av_trigger_dual_handoff(BOOLEAN handoff, BD_ADDR address)
{
    int index,next_idx;
    /*Get the current playing device*/
    BTIF_TRACE_DEBUG("%s", __FUNCTION__);
    index = btif_get_latest_playing_device_idx();
    if (index != btif_max_av_clients)
    {
        btif_av_cb[index].dual_handoff = handoff; /*Initiate Handoff*/
        if (bt_split_a2dp_enabled)
            btif_media_on_stop_vendor_command();
        /*Initiate SUSPEND for this device*/
        BTIF_TRACE_DEBUG("Initiate SUSPEND for this device on index = %d", index);
        btif_sm_dispatch(btif_av_cb[index].sm_handle, BTIF_AV_SUSPEND_STREAM_REQ_EVT, NULL);
    }
    else
    {
        BTIF_TRACE_ERROR("Handoff on invalid index");
    }
    if (bt_split_a2dp_enabled)
    {
        btif_media_send_reset_vendor_state();
        next_idx = btif_av_get_other_connected_idx(index);
        /* Fix for below Klockwork Issue
        Array 'btif_av_cb' of size 2 may use index value(s) -1 */
        if (next_idx != INVALID_INDEX && next_idx != btif_max_av_clients)
        {
            HAL_CBACK(bt_av_src_vendor_callbacks, reconfig_a2dp_trigger_cb, 1,
                                    &(btif_av_cb[next_idx].peer_bda));
        }
    }
}

/*******************************************************************************
**
** Function         btif_av_trigger_suspend
**
** Description      Trigger suspend when multicast is ongoing for tuch tones
**                  and new ACL is created.
**
** Returns          void
**
*******************************************************************************/

void btif_av_trigger_suspend()
{
    int index;
    /*Get the current playing device*/
    BTIF_TRACE_DEBUG("%s", __FUNCTION__);
    index = btif_get_latest_playing_device_idx();
    if (index <= btif_max_av_clients)
    {
        /*Initiate SUSPEND for this device*/
        BTIF_TRACE_DEBUG("Initiate SUSPEND for this device on index = %d", index);
        btif_sm_dispatch(btif_av_cb[index].sm_handle, BTIF_AV_SUSPEND_STREAM_REQ_EVT, NULL);
    }
    else
    {
        BTIF_TRACE_ERROR("suspend on invalid index");
    }
}

/*******************************************************************************
**
** Function         connect
**
** Description      Establishes the AV signalling channel with the remote headset
**
** Returns          bt_status_t
**
*******************************************************************************/

static bt_status_t connect_int(bt_bdaddr_t *bd_addr, uint16_t uuid)
{
    btif_av_connect_req_t connect_req;
    int i;
    connect_req.target_bda = bd_addr;
    connect_req.uuid = uuid;
    BTIF_TRACE_EVENT("%s", __FUNCTION__);

    for (i = 0; i < btif_max_av_clients;)
    {
        if(btif_av_get_valid_idx(i))
        {
            if (bdcmp(bd_addr->address, btif_av_cb[i].peer_bda.address) == 0)
            {
                BTIF_TRACE_ERROR("Attempting connection for non idle device.. back off ");
                btif_queue_advance();
                return BT_STATUS_SUCCESS;
            }
            i++;
        }
        else
            break;
    }
    if (i == btif_max_av_clients)
    {
        UINT8 rc_handle;

        BTIF_TRACE_ERROR("%s: All indexes are full", __FUNCTION__);

        /* Multicast: Check if AV slot is available for connection
         * If not available, AV got connected to different devices.
         * Disconnect this RC connection without AV connection.
         */
        rc_handle = btif_rc_get_connected_peer_handle(bd_addr->address);
        if (rc_handle != BTIF_RC_HANDLE_NONE)
        {
            BTA_AvCloseRc(rc_handle);
        }
        btif_queue_advance();
        return BT_STATUS_FAIL;
    }

    btif_sm_dispatch(btif_av_cb[i].sm_handle, BTIF_AV_CONNECT_REQ_EVT, (char*)&connect_req);


    return BT_STATUS_SUCCESS;
}

static bt_status_t src_connect_sink(bt_bdaddr_t *bd_addr)
{
    BTIF_TRACE_EVENT("%s", __FUNCTION__);
    CHECK_BTAV_INIT();

    return btif_queue_connect(UUID_SERVCLASS_AUDIO_SOURCE, bd_addr, connect_int);
}

static bt_status_t sink_connect_src(bt_bdaddr_t *bd_addr)
{
    BTIF_TRACE_EVENT("%s", __FUNCTION__);
    CHECK_BTAV_INIT();

    return btif_queue_connect(UUID_SERVCLASS_AUDIO_SINK, bd_addr, connect_int);
}

/*******************************************************************************
**
** Function         disconnect
**
** Description      Tears down the AV signalling channel with the remote headset
**
** Returns          bt_status_t
**
*******************************************************************************/
static bt_status_t disconnect(bt_bdaddr_t *bd_addr)
{
    BTIF_TRACE_EVENT("%s", __FUNCTION__);

    CHECK_BTAV_INIT();

    /* Switch to BTIF context */
    return btif_transfer_context(btif_av_handle_event, BTIF_AV_DISCONNECT_REQ_EVT,
                                 (char*)bd_addr, sizeof(bt_bdaddr_t), NULL);
}

/*******************************************************************************
**
** Function         cleanup
**
** Description      Shuts down the AV interface and does the cleanup
**
** Returns          None
**
*******************************************************************************/
static void cleanup(int service_uuid)
{
    BTIF_TRACE_IMP("AV %s", __FUNCTION__);

    btif_transfer_context(btif_av_handle_event, BTIF_AV_CLEANUP_REQ_EVT,
            (char*)&service_uuid, sizeof(int), NULL);

    btif_disable_service(service_uuid);
}

static void cleanup_src(void) {
    BTIF_TRACE_EVENT("%s", __FUNCTION__);
    cleanup(BTA_A2DP_SOURCE_SERVICE_ID);
    pthread_mutex_lock(&src_codec_q_lock);
    if (p_bta_av_codec_pri_list != NULL) {
        osi_free(p_bta_av_codec_pri_list);
        p_bta_av_codec_pri_list = NULL;
    }
    pthread_mutex_unlock(&src_codec_q_lock);
    pthread_mutex_destroy(&src_codec_q_lock);
    BTIF_TRACE_EVENT("%s completed", __FUNCTION__);
}

static void cleanup_sink(void) {
    BTIF_TRACE_EVENT("%s", __FUNCTION__);
    cleanup(BTA_A2DP_SINK_SERVICE_ID);
    pthread_mutex_destroy(&pcm_queue_lock);
}

static void cleanup_src_vendor(void) {
    BTIF_TRACE_EVENT("%s", __FUNCTION__);
    if (bt_av_src_vendor_callbacks)
    {
        bt_av_src_vendor_callbacks = NULL;
    }
    BTIF_TRACE_EVENT("%s completed", __FUNCTION__);
}

static void is_value_to_be_updated(void *ptr1, void *ptr2)
{
    char *p1 = (char*)ptr1;
    char *p2 = (char*)ptr2;
    if (!(*p1 & *p2)) {
        *p1 |= *p2;
    }
}

/*******************************************************************************
**
** Function         update_supported_codecs_param_vendor
**
** Description      Updates the codecs supported by Source as requested by APPS
**
** Returns          bt_status_t
**
*******************************************************************************/
static bt_status_t update_supported_codecs_param_vendor(btav_codec_configuration_t
        *p_codec_config_list, uint8_t num_codec_configs)
{
    int i, j;

    if (num_codec_configs == 0 || num_codec_configs > MAX_NUM_CODEC_CONFIGS) {
        BTIF_TRACE_ERROR(" %s Invalid num_codec_configs = %d",
            __func__, num_codec_configs);
        return BT_STATUS_PARM_INVALID;
    }

    if (!p_codec_config_list) {
        BTIF_TRACE_ERROR(" %s codec list is NULL", __func__);
        return BT_STATUS_PARM_INVALID;
    }

    // Check if the codec params sent by upper layers are valid or not.
    for (i = 0; i < num_codec_configs; i ++) {
        switch (p_codec_config_list[i].codec_type) {
            case A2DP_SOURCE_AUDIO_CODEC_SBC:
                switch (p_codec_config_list[i].codec_config.sbc_config.samp_freq) {
                    case SBC_SAMP_FREQ_16:
                    case SBC_SAMP_FREQ_32:
                    case SBC_SAMP_FREQ_44:
                    case SBC_SAMP_FREQ_48:
                        break;
                    default:
                        BTIF_TRACE_ERROR(" %s Invalid SBC freq = %d",
                            __func__, p_codec_config_list[i].codec_config.sbc_config.samp_freq);
                        return BT_STATUS_PARM_INVALID;
                }
                switch (p_codec_config_list[i].codec_config.sbc_config.ch_mode) {
                    case SBC_CH_MONO:
                    case SBC_CH_DUAL:
                    case SBC_CH_STEREO:
                    case SBC_CH_JOINT:
                        break;
                    default:
                        BTIF_TRACE_ERROR(" %s Invalid SBC channel mode = %d",
                            __func__, p_codec_config_list[i].codec_config.sbc_config.ch_mode);
                        return BT_STATUS_PARM_INVALID;
                }
                switch (p_codec_config_list[i].codec_config.sbc_config.block_len) {
                    case SBC_BLOCKS_4:
                    case SBC_BLOCKS_8:
                    case SBC_BLOCKS_12:
                    case SBC_BLOCKS_16:
                        break;
                    default:
                        BTIF_TRACE_ERROR(" %s Invalid SBC block len = %d",
                            __func__, p_codec_config_list[i].codec_config.sbc_config.block_len);
                        return BT_STATUS_PARM_INVALID;
                }
                switch (p_codec_config_list[i].codec_config.sbc_config.num_subbands) {
                    case SBC_SUBBAND_4:
                    case SBC_SUBBAND_8:
                        break;
                    default:
                        BTIF_TRACE_ERROR(" %s Invalid SBC subbands = %d",
                            __func__, p_codec_config_list[i].codec_config.sbc_config.num_subbands);
                        return BT_STATUS_PARM_INVALID;
                }
                switch (p_codec_config_list[i].codec_config.sbc_config.alloc_mthd) {
                    case SBC_ALLOC_SNR:
                    case SBC_ALLOC_LOUDNESS:
                        break;
                    default:
                        BTIF_TRACE_ERROR(" %s Invalid SBC allocation method = %d",
                            __func__, p_codec_config_list[i].codec_config.sbc_config.alloc_mthd);
                        return BT_STATUS_PARM_INVALID;
                }
                if (p_codec_config_list[i].codec_config.sbc_config.min_bitpool <
                    A2D_SBC_IE_MIN_BITPOOL) {
                    BTIF_TRACE_ERROR(" %s Invalid SBC Min Bitpool = %d",
                        __func__, p_codec_config_list[i].codec_config.sbc_config.min_bitpool);
                    return BT_STATUS_PARM_INVALID;
                }
                if (p_codec_config_list[i].codec_config.sbc_config.max_bitpool >
                    A2D_SBC_IE_MAX_BITPOOL) {
                    BTIF_TRACE_ERROR(" %s Invalid SBC Max Bitpool = %d",
                        __func__, p_codec_config_list[i].codec_config.sbc_config.max_bitpool);
                    return BT_STATUS_PARM_INVALID;
                }
                if (p_codec_config_list[i].codec_config.sbc_config.min_bitpool >
                    p_codec_config_list[i].codec_config.sbc_config.max_bitpool) {
                    BTIF_TRACE_ERROR(" %s Min Bitpool (%d) > Max Bitpool (%d)",
                        __func__, p_codec_config_list[i].codec_config.sbc_config.min_bitpool,
                        p_codec_config_list[i].codec_config.sbc_config.max_bitpool);
                    return BT_STATUS_PARM_INVALID;
                }                break;

            case A2DP_SOURCE_AUDIO_CODEC_APTX:
                switch (p_codec_config_list[i].codec_config.aptx_config.sampling_freq) {
                    case APTX_SAMPLERATE_44100:
                    case APTX_SAMPLERATE_48000:
                        break;
                    default:
                        BTIF_TRACE_ERROR(" %s Invalid APTX freq = %d",
                            __func__, p_codec_config_list[i].codec_config.aptx_config.sampling_freq);
                        return BT_STATUS_PARM_INVALID;
                }
                switch (p_codec_config_list[i].codec_config.aptx_config.channel_count) {
                    case APTX_CHANNELS_STEREO:
                    case APTX_CHANNELS_MONO:
                        break;
                    default:
                        BTIF_TRACE_ERROR(" %s Invalid APTX channel mode = %d",
                            __func__, p_codec_config_list[i].codec_config.aptx_config.channel_count);
                        return BT_STATUS_PARM_INVALID;
                }
                break;
            default:
                BTIF_TRACE_ERROR(" %s Invalid codec type = %d",
                    __func__, p_codec_config_list[i].codec_type);
                return BT_STATUS_PARM_INVALID;
        }
    }

    pthread_mutex_lock(&src_codec_q_lock);
    if (p_bta_av_codec_pri_list == NULL) {
        BTIF_TRACE_ERROR(" %s p_bta_av_codec_pri_list is NULL returning!!", __func__);
        pthread_mutex_unlock(&src_codec_q_lock);
        return BT_STATUS_NOT_READY;
    }

    /* Copy the codec parameters passed from application layer to create a pointer to
     * preferred codec list for outgoing connection */
    tA2D_SBC_CIE sbc_supported_cap;
    tA2D_APTX_CIE aptx_supported_cap;
    UINT8 codec_info[BTIF_SV_AV_AA_SRC_SEP_INDEX][AVDT_CODEC_SIZE];

    /* Free the memory already allocated and reallocate fresh memory */
    osi_free(p_bta_av_codec_pri_list);
    p_bta_av_codec_pri_list = osi_calloc((num_codec_configs +
        BTIF_SV_AV_AA_SRC_SEP_INDEX) * sizeof(tBTA_AV_CO_CODEC_CAP_LIST));
    if (p_bta_av_codec_pri_list == NULL) {
        BTIF_TRACE_ERROR(" %s p_bta_av_codec_pri_list is NULL returning!!", __func__);
        pthread_mutex_unlock(&src_codec_q_lock);
        return BT_STATUS_NOMEM;
    }
    /* Set codec supported capabilities to mandatory capabilities for each codec */
    memcpy(&sbc_supported_cap, &bta_av_co_sbc_caps, sizeof(tA2D_SBC_CIE));
    memcpy(&aptx_supported_cap, &bta_av_co_aptx_caps, sizeof(tA2D_APTX_CIE));
    for (i = 0; i < num_codec_configs; i ++) {
        p_bta_av_codec_pri_list[i].codec_type =
            p_codec_config_list[i].codec_type;
        switch (p_codec_config_list[i].codec_type) {
            case A2DP_SINK_AUDIO_CODEC_SBC:
                /* Copy SBC codec parameters  as per application layer */
                memcpy(&p_bta_av_codec_pri_list[i].codec_cap.sbc_caps,
                    &p_codec_config_list[i].codec_config.sbc_config,
                    sizeof(tA2D_SBC_CIE));
                /* Check if supported capability needs to be updated */
                is_value_to_be_updated(&sbc_supported_cap.samp_freq,
                    &p_codec_config_list[i].codec_config.sbc_config.samp_freq);
                is_value_to_be_updated(&sbc_supported_cap.ch_mode,
                    &p_codec_config_list[i].codec_config.sbc_config.ch_mode);
                is_value_to_be_updated(&sbc_supported_cap.block_len,
                    &p_codec_config_list[i].codec_config.sbc_config.block_len);
                is_value_to_be_updated(&sbc_supported_cap.num_subbands,
                    &p_codec_config_list[i].codec_config.sbc_config.num_subbands);
                is_value_to_be_updated(&sbc_supported_cap.alloc_mthd,
                    &p_codec_config_list[i].codec_config.sbc_config.alloc_mthd);
                if (sbc_supported_cap.min_bitpool >
                    p_codec_config_list[i].codec_config.sbc_config.min_bitpool) {
                    sbc_supported_cap.min_bitpool =
                        p_codec_config_list[i].codec_config.sbc_config.min_bitpool;
                }
                if (sbc_supported_cap.max_bitpool <
                    p_codec_config_list[i].codec_config.sbc_config.max_bitpool) {
                    sbc_supported_cap.max_bitpool =
                        p_codec_config_list[i].codec_config.sbc_config.max_bitpool;
                }
                break;

            case A2DP_SOURCE_AUDIO_CODEC_APTX:
                /* Update Codec Type for APTX */
                p_bta_av_codec_pri_list[i].codec_type = A2D_NON_A2DP_MEDIA_CT;
                /* Copy Mandatory APTX codec parameters */
                memcpy(&p_bta_av_codec_pri_list[i].codec_cap.aptx_caps,
                    &bta_av_co_aptx_caps, sizeof(tA2D_APTX_CIE));
                /* Update sampling frequency as per Application layer */
                p_bta_av_codec_pri_list[i].codec_cap.aptx_caps.sampleRate =
                p_codec_config_list[i].codec_config.aptx_config.sampling_freq;
                /* Update channel mode as per Application layer */
                p_bta_av_codec_pri_list[i].codec_cap.aptx_caps.channelMode =
                p_codec_config_list[i].codec_config.aptx_config.channel_count;
                /* Check if supported capability needs to be updated */
                is_value_to_be_updated(&aptx_supported_cap.sampleRate,
                    &p_codec_config_list[i].codec_config.aptx_config.sampling_freq);
                is_value_to_be_updated(&aptx_supported_cap.channelMode,
                    &p_codec_config_list[i].codec_config.aptx_config.channel_count);
                break;
        }
    }

    uint8_t codec_type_list[BTIF_SV_AV_AA_SRC_SEP_INDEX];
    uint8_t vnd_id_list[BTIF_SV_AV_AA_SRC_SEP_INDEX];
    uint8_t codec_id_list[BTIF_SV_AV_AA_SRC_SEP_INDEX];
    uint8_t codec_type_added[MAX_NUM_CODEC_CONFIGS];
    memset(codec_type_list, A2DP_SOURCE_AUDIO_CODEC_SBC, BTIF_SV_AV_AA_SRC_SEP_INDEX);
    memset(vnd_id_list, 0, BTIF_SV_AV_AA_SRC_SEP_INDEX);
    memset(codec_id_list, 0, BTIF_SV_AV_AA_SRC_SEP_INDEX);
    memset(codec_type_added, 0, MAX_NUM_CODEC_CONFIGS);

    /* Remove duplicate codecs from list. This will be used for hiding/showing codecs
     * for response to AVDTP discover command */
    j = 0;
    for (i = 0; i < num_codec_configs; i ++) {
        if (!codec_type_added[p_codec_config_list[i].codec_type]) {
            if (p_codec_config_list[i].codec_type ==
                A2DP_SOURCE_AUDIO_CODEC_APTX) {
                codec_type_list[j] = A2D_NON_A2DP_MEDIA_CT;
                vnd_id_list[j] = A2D_APTX_VENDOR_ID;
                codec_id_list[j] = A2D_APTX_CODEC_ID_BLUETOOTH;
            }
            else {
                codec_type_list[j] = p_codec_config_list[i].codec_type;
            }
            codec_type_added[p_codec_config_list[i].codec_type] = 1;
            j ++;
            if (j >= BTIF_SV_AV_AA_SRC_SEP_INDEX) {
                BTIF_TRACE_ERROR(" %s num of different codecs(%d) exceeds max limit",
                    __func__, j);
                break;
            }
        }
    }

    /* Add mandatory codec for all supported codec in the end of priority list to handle
         * case if the codec parameters sent by upper layers are not capable of creating connection.
         * In that case, use the below parameters to create connection. in order of priority of
         * APTX > SBC */
    if (codec_type_added[A2DP_SOURCE_AUDIO_CODEC_APTX]) {
        p_bta_av_codec_pri_list[num_codec_configs].codec_type
            = A2D_NON_A2DP_MEDIA_CT;
        /* Copy Mandatory APTX codec parameters */
        memcpy(&p_bta_av_codec_pri_list[num_codec_configs ++]
            .codec_cap.aptx_caps, &bta_av_co_aptx_caps,
            sizeof(tA2D_APTX_CIE));
        BTIF_TRACE_DEBUG(" %s Added Mandatory APTX codec at index %d",
            __func__, num_codec_configs - 1);
    }
    p_bta_av_codec_pri_list[num_codec_configs].codec_type
        = A2DP_SOURCE_AUDIO_CODEC_SBC;
    /* Copy Mandatory SBC codec parameters */
    memcpy(&p_bta_av_codec_pri_list[num_codec_configs ++]
        .codec_cap.sbc_caps, &bta_av_co_sbc_caps, sizeof(tA2D_SBC_CIE));
    BTIF_TRACE_DEBUG(" %s Added Mandatory SBC codec at index %d",
        __func__, num_codec_configs - 1);

    j = 0;
    /* Create Codec Config array for supported types as per application layer */
    memset(codec_info, 0, BTIF_SV_AV_AA_SRC_SEP_INDEX * AVDT_CODEC_SIZE);
    A2D_BldSbcInfo(AVDT_MEDIA_AUDIO, &sbc_supported_cap, codec_info[j ++]);
    memcpy(&bta_av_supp_codec_cap[BTIF_SV_AV_AA_SBC_INDEX].codec_cap.sbc_caps,
        &sbc_supported_cap, sizeof(tA2D_SBC_CIE));
    if (codec_type_added[A2DP_SOURCE_AUDIO_CODEC_APTX]) {
        A2D_BldAptxInfo(AVDT_MEDIA_AUDIO, &aptx_supported_cap, codec_info[j ++]);
        memcpy(&bta_av_supp_codec_cap[BTIF_SV_AV_AA_APTX_INDEX].codec_cap.aptx_caps,
            &aptx_supported_cap, sizeof(tA2D_APTX_CIE));
    }
    bta_av_num_codec_configs = num_codec_configs;
    BTIF_TRACE_DEBUG(" %s Num_codec_configs = %d", __func__, num_codec_configs);
    for (i = 0; i < j; i ++) {
        BTIF_TRACE_VERBOSE(" %s %d %d %d %d %d %d %d %d %d", __func__,
            codec_info[i][0], codec_info[i][1], codec_info[i][2], codec_info[i][3],
            codec_info[i][4], codec_info[i][5], codec_info[i][6], codec_info[i][7],
            codec_info[i][8]);
    }
    /* Update the codec config supported paratmers so that correct response can be
     * sent for AVDTP discover and get capabilities command from remote device */
    BTA_AvUpdateCodecSupport(codec_type_list, vnd_id_list, codec_id_list,
        codec_info, j);

    pthread_mutex_unlock(&src_codec_q_lock);

    return BT_STATUS_SUCCESS;
}

static void cleanup_sink_vendor(void) {
    BTIF_TRACE_EVENT("%s", __FUNCTION__);
    if (bt_av_sink_vendor_callbacks)
    {
        bt_av_sink_vendor_callbacks = NULL;
    }
    BTIF_TRACE_EVENT("%s completed", __FUNCTION__);
}

static void allow_connection_vendor(int is_valid, bt_bdaddr_t *bd_addr)
{
    int index = 0;
    BTIF_TRACE_DEBUG(" %s isValid is %d event %d", __FUNCTION__,is_valid,idle_rc_event);
    switch (idle_rc_event)
    {
        case BTA_AV_RC_OPEN_EVT:
            if (is_valid)
            {
                BTIF_TRACE_DEBUG("allowconn for RC connection");
                alarm_set_on_queue(av_open_on_rc_timer,
                          BTIF_TIMEOUT_AV_OPEN_ON_RC_MS,
                          btif_initiate_av_open_timer_timeout, NULL,
                          btu_general_alarm_queue);
                btif_rc_handler(idle_rc_event, &idle_rc_data);
            }
            else
            {
                UINT8 rc_handle =  idle_rc_data.rc_open.rc_handle;
                BTA_AvCloseRc(rc_handle);
            }
            break;

        case BTA_AV_PENDING_EVT:
            if (is_valid)
            {
                index = btif_av_idx_by_bdaddr(bd_addr->address);
                if (index >= btif_max_av_clients)
                {
                    BTIF_TRACE_DEBUG("Invalid index for device");
                    break;
                }
                BTIF_TRACE_DEBUG("The connection is allowed for the device at index = %d", index);
                BTA_AvOpen(btif_av_cb[index].peer_bda.address, btif_av_cb[index].bta_handle,
                       TRUE, BTA_SEC_NONE, UUID_SERVCLASS_AUDIO_SOURCE);
            }
            else
            {
                BTA_AvDisconnect(idle_rc_data.pend.bd_addr);
            }
            break;

        default:
            BTIF_TRACE_DEBUG("%s : unhandled event:%s", __FUNCTION__,
                                dump_av_sm_event_name(idle_rc_event));
    }
    idle_rc_event = 0;
    memset(&idle_rc_data, 0, sizeof(tBTA_AV));
}

static const btav_interface_t bt_av_src_interface = {
    sizeof(btav_interface_t),
    init_src,
    src_connect_sink,
    disconnect,
    cleanup_src,
    NULL,
    NULL,
};

static const btav_interface_t bt_av_sink_interface = {
    sizeof(btav_interface_t),
    init_sink,
    sink_connect_src,
    disconnect,
    cleanup_sink,
    NULL,
    NULL,
};

static const btav_vendor_interface_t bt_av_src_vendor_interface = {
    sizeof(btav_vendor_interface_t),
    init_src_vendor,
    allow_connection_vendor,
    NULL,
    get_src_codec_config,
    NULL,
    cleanup_src_vendor,
    update_supported_codecs_param_vendor,
};

static const btav_vendor_interface_t bt_av_sink_vendor_interface = {
    sizeof(btav_vendor_interface_t),
    init_sink_vendor,
    NULL,
#ifdef USE_AUDIO_TRACK
    audio_focus_status_vendor,
#else
    NULL,
#endif
    NULL,
    cleanup_sink_vendor,
};

/*******************************************************************************
**
** Function         btif_av_get_sm_handle
**
** Description      Fetches current av SM handle
**
** Returns          None
**
*******************************************************************************/
/* Media task uses this info
* But dont use it. */
btif_sm_handle_t btif_av_get_sm_handle(void)
{
    return btif_av_cb[0].sm_handle;
}

/*******************************************************************************
**
** Function         btif_av_get_addr
**
** Description      Fetches current AV BD address
**
** Returns          BD address
**
*******************************************************************************/

bt_bdaddr_t btif_av_get_addr(BD_ADDR address)
{
    int i;
    bt_bdaddr_t not_found ;
    memset (&not_found, 0, sizeof(bt_bdaddr_t));
    for (i = 0; i < btif_max_av_clients; i++)
    {
        if (bdcmp(btif_av_cb[i].peer_bda.address, address) == 0)
            return btif_av_cb[i].peer_bda;
    }
    return not_found;
}

/*******************************************************************************
** Function         btif_av_is_sink_enabled
**
** Description      Checks if A2DP Sink is enabled or not
**
** Returns          TRUE if A2DP Sink is enabled, false otherwise
**
*******************************************************************************/

BOOLEAN btif_av_is_sink_enabled(void)
{
    return (bt_av_sink_callbacks != NULL) ? TRUE : FALSE;
}

/*******************************************************************************
**
** Function         btif_av_stream_ready
**
** Description      Checks whether AV is ready for starting a stream
**
** Returns          None
**
*******************************************************************************/

BOOLEAN btif_av_stream_ready(void)
{
    int i;
    BOOLEAN status = FALSE;
    /* also make sure main adapter is enabled */
    if (btif_is_enabled() == 0)
    {
        BTIF_TRACE_EVENT("main adapter not enabled");
        return FALSE;
    }

    for (i = 0; i < btif_max_av_clients; i++)
    {
        BTIF_TRACE_DEBUG("btif_av_stream_ready flags: %d", btif_av_cb[i].flags);
        btif_av_cb[i].state = btif_sm_get_state(btif_av_cb[i].sm_handle);
        /* Multicast:
         * If any of the stream is in pending suspend state when
         * we initiate start, it will result in inconsistent behavior
         * Check the pending SUSPEND flag and return failure
         * if suspend is in progress.
         */
        if (btif_av_cb[i].dual_handoff ||
            (btif_av_cb[i].flags & BTIF_AV_FLAG_LOCAL_SUSPEND_PENDING))
        {
            status = FALSE;
            break;
        }
        else if (btif_av_cb[i].flags &
            (BTIF_AV_FLAG_REMOTE_SUSPEND|BTIF_AV_FLAG_PENDING_STOP))
        {
            status = FALSE;
            break;
        }
        else if (btif_av_cb[i].state == BTIF_AV_STATE_OPENED)
        {
            status = TRUE;
        }
    }
    BTIF_TRACE_DEBUG("btif_av_stream_ready: %d", status);
    return status;
}

/*******************************************************************************
**
** Function         btif_av_stream_started_ready
**
** Description      Checks whether AV ready for media start in streaming state
**
** Returns          None
**
*******************************************************************************/

BOOLEAN btif_av_stream_started_ready(void)
{
    int i;
    BOOLEAN status = FALSE;

    for (i = 0; i < btif_max_av_clients; i++)
    {
        btif_av_cb[i].state = btif_sm_get_state(btif_av_cb[i].sm_handle);
        if (btif_av_cb[i].dual_handoff)
        {
            BTIF_TRACE_ERROR("%s: Under Dual handoff ",__FUNCTION__ );
            status = FALSE;
            break;
        } else if (btif_av_cb[i].flags &
            (BTIF_AV_FLAG_LOCAL_SUSPEND_PENDING |
            BTIF_AV_FLAG_REMOTE_SUSPEND |
            BTIF_AV_FLAG_PENDING_STOP))
        {
            status = FALSE;
            break;
        } else if (btif_av_cb[i].state == BTIF_AV_STATE_STARTED)
        {
            status = TRUE;
        }
    }
    BTIF_TRACE_DEBUG("btif_av_stream_started_ready: %d", status);
    return status;
}

/*******************************************************************************
**
** Function         btif_dispatch_sm_event
**
** Description      Send event to AV statemachine
**
** Returns          None
**
*******************************************************************************/

/* used to pass events to AV statemachine from other tasks */
void btif_dispatch_sm_event(btif_av_sm_event_t event, void *p_data, int len)
{
    /* Switch to BTIF context */
    BTIF_TRACE_IMP("%s: event: %d, len: %d", __FUNCTION__, event, len);
    btif_transfer_context(btif_av_handle_event, event,
                          (char*)p_data, len, NULL);
    BTIF_TRACE_IMP("%s: event %d sent", __FUNCTION__, event);
}

/*******************************************************************************
**
** Function         btif_av_execute_service
**
** Description      Initializes/Shuts down the service
**
** Returns          BT_STATUS_SUCCESS on success, BT_STATUS_FAIL otherwise
**
*******************************************************************************/
bt_status_t btif_av_execute_service(BOOLEAN b_enable)
{
    int i;
    btif_sm_state_t state;
    BTIF_TRACE_IMP("%s: enable: %d", __FUNCTION__, b_enable);
    if (b_enable)
    {
        /* TODO: Removed BTA_SEC_AUTHORIZE since the Java/App does not
        * handle this request in order to allow incoming connections to succeed.
        * We need to put this back once support for this is added */

        /* Added BTA_AV_FEAT_NO_SCO_SSPD - this ensures that the BTA does not
        * auto-suspend av streaming on AG events(SCO or Call). The suspend shall
        * be initiated by the app/audioflinger layers */
#ifndef ANDROID
        UINT16 feat_delayrpt;
        if(enable_delay_reporting)
            feat_delayrpt = BTA_AV_FEAT_DELAY_RPT;
        else
            feat_delayrpt = 0x0;
#if (AVRC_METADATA_INCLUDED == TRUE)
        BTA_AvEnable(BTA_SEC_AUTHENTICATE,
            BTA_AV_FEAT_RCTG|BTA_AV_FEAT_METADATA|BTA_AV_FEAT_VENDOR|BTA_AV_FEAT_NO_SCO_SSPD
            |BTA_AV_FEAT_ACP_START | feat_delayrpt 
#if (AVRC_ADV_CTRL_INCLUDED == TRUE)
            |BTA_AV_FEAT_RCCT
            |BTA_AV_FEAT_ADV_CTRL
            |BTA_AV_FEAT_BROWSE
#endif
            ,bte_av_callback);
#else
        BTA_AvEnable(BTA_SEC_AUTHENTICATE, (BTA_AV_FEAT_RCTG | BTA_AV_FEAT_NO_SCO_SSPD
            |BTA_AV_FEAT_ACP_START | feat_delayrpt), bte_av_callback);
#endif
#else
#if (AVRC_METADATA_INCLUDED == TRUE)
        BTA_AvEnable(BTA_SEC_AUTHENTICATE,
            BTA_AV_FEAT_RCTG|BTA_AV_FEAT_METADATA|BTA_AV_FEAT_VENDOR|BTA_AV_FEAT_NO_SCO_SSPD
            |BTA_AV_FEAT_ACP_START
#if (AVRC_ADV_CTRL_INCLUDED == TRUE)
            |BTA_AV_FEAT_RCCT
            |BTA_AV_FEAT_ADV_CTRL
            |BTA_AV_FEAT_BROWSE
#endif
            ,bte_av_callback);
#else
        BTA_AvEnable(BTA_SEC_AUTHENTICATE, (BTA_AV_FEAT_RCTG | BTA_AV_FEAT_NO_SCO_SSPD
            |BTA_AV_FEAT_ACP_START), bte_av_callback);
#endif
#endif
        for (i = 0; i < btif_max_av_clients; i++)
        {
            BTIF_TRACE_DEBUG("%s: BTA_AvRegister : %d", __FUNCTION__, i);
            BTA_AvRegister(BTA_AV_CHNL_AUDIO, BTIF_AV_SERVICE_NAME, 0, bte_av_media_callback,
            UUID_SERVCLASS_AUDIO_SOURCE);
        }
        BTA_AvUpdateMaxAVClient(btif_max_av_clients);
    }
    else
    {
        /* Also shut down the AV state machine */
        for (i = 0; i < btif_max_av_clients; i++ )
        {
            if (btif_av_cb[i].sm_handle != NULL)
            {
                state = btif_sm_get_state(btif_av_cb[i].sm_handle);
                if(state==BTIF_AV_STATE_OPENING)
                {
                    BTIF_TRACE_DEBUG("Moving State from Opening to Idle due to BT ShutDown");
                    btif_sm_change_state(btif_av_cb[i].sm_handle, BTIF_AV_STATE_IDLE);
                    btif_queue_advance();
                }
                btif_sm_shutdown(btif_av_cb[i].sm_handle);
                btif_av_cb[i].sm_handle = NULL;
            }
        }
        for (i = 0; i < btif_max_av_clients; i++)
        {
            BTA_AvDeregister(btif_av_cb[i].bta_handle);
        }
        BTA_AvDisable();
    }
    BTIF_TRACE_IMP("%s: enable: %d completed", __FUNCTION__, b_enable);
    return BT_STATUS_SUCCESS;
}

/*******************************************************************************
**
** Function         btif_av_sink_execute_service
**
** Description      Initializes/Shuts down the service
**
** Returns          BT_STATUS_SUCCESS on success, BT_STATUS_FAIL otherwise
**
*******************************************************************************/
bt_status_t btif_av_sink_execute_service(BOOLEAN b_enable)
{
     int i;
     BTIF_TRACE_IMP("%s: enable: %d", __FUNCTION__, b_enable);

     if (b_enable)
     {
         /* Added BTA_AV_FEAT_NO_SCO_SSPD - this ensures that the BTA does not
          * auto-suspend av streaming on AG events(SCO or Call). The suspend shall
          * be initiated by the app/audioflinger layers */
         BTA_AvEnable(BTA_SEC_AUTHENTICATE, BTA_AV_FEAT_NO_SCO_SSPD|BTA_AV_FEAT_RCCT|
                                            BTA_AV_FEAT_METADATA|BTA_AV_FEAT_VENDOR|
                                            BTA_AV_FEAT_ADV_CTRL|BTA_AV_FEAT_RCTG,
                                                                        bte_av_callback);
         BTA_AvRegister(BTA_AV_CHNL_AUDIO, BTIF_AVK_SERVICE_NAME, 0, bte_av_media_callback,
                                                                UUID_SERVCLASS_AUDIO_SINK);
     }
     else {
         if (btif_av_cb[0].sm_handle != NULL)
         {
             BTIF_TRACE_IMP("%s: shutting down AV SM", __FUNCTION__);
             btif_sm_shutdown(btif_av_cb[0].sm_handle);
             btif_av_cb[0].sm_handle = NULL;
         }
         BTA_AvDeregister(btif_av_cb[0].bta_handle);
         BTA_AvDisable();
     }
     BTIF_TRACE_IMP("%s: enable: %d completed", __FUNCTION__, b_enable);
     return BT_STATUS_SUCCESS;
}

/*******************************************************************************
**
** Function         btif_av_get_src_interface
**
** Description      Get the AV callback interface for A2DP source profile
**
** Returns          btav_interface_t
**
*******************************************************************************/
const btav_interface_t *btif_av_get_src_interface(void)
{
    BTIF_TRACE_EVENT("%s", __FUNCTION__);
    return &bt_av_src_interface;
}

/*******************************************************************************
**
** Function         btif_av_get_sink_interface
**
** Description      Get the AV callback interface for A2DP sink profile
**
** Returns          btav_interface_t
**
*******************************************************************************/
const btav_interface_t *btif_av_get_sink_interface(void)
{
    BTIF_TRACE_EVENT("%s", __FUNCTION__);
    return &bt_av_sink_interface;
}

/*******************************************************************************
**
** Function         btif_av_get_src_vendor_interface
**
** Description      Get the AV callback interface for A2DP source profile
**
** Returns          btav_interface_t
**
*******************************************************************************/
const btav_vendor_interface_t *btif_av_get_src_vendor_interface(void)
{
    BTIF_TRACE_EVENT("%s", __FUNCTION__);
    return &bt_av_src_vendor_interface;
}

/*******************************************************************************
**
** Function         btif_av_get_sink_interface
**
** Description      Get the AV callback interface for A2DP sink profile
**
** Returns          btav_interface_t
**
*******************************************************************************/
const btav_interface_t *btif_av_get_sink_vendor_interface(void)
{
    BTIF_TRACE_EVENT("%s", __FUNCTION__);
    return &bt_av_sink_vendor_interface;
}

/*******************************************************************************
**
** Function         btif_av_is_connected
**
** Description      Checks if av has a connected sink
**
** Returns          BOOLEAN
**
*******************************************************************************/
BOOLEAN btif_av_is_connected(void)
{
    int i;
    BOOLEAN status = FALSE;
    for (i = 0; i < btif_max_av_clients; i++)
    {
        btif_av_cb[i].state = btif_sm_get_state(btif_av_cb[i].sm_handle);
        if ((btif_av_cb[i].state == BTIF_AV_STATE_OPENED) ||
            (btif_av_cb[i].state ==  BTIF_AV_STATE_STARTED))
            status = TRUE;
    }
    return status;
}

/*******************************************************************************
**
** Function         btif_av_is_connected_on_other_idx
**
** Description      Checks if any other AV SCB is connected
**
** Returns          BOOLEAN
**
*******************************************************************************/

BOOLEAN btif_av_is_connected_on_other_idx(int current_index)
{
    //return true if other IDx is connected
    btif_sm_state_t state = BTIF_AV_STATE_IDLE;
    int i;
    for (i = 0; i < btif_max_av_clients; i++)
    {
        if (i != current_index)
        {
            state = btif_sm_get_state(btif_av_cb[i].sm_handle);
            if ((state == BTIF_AV_STATE_OPENED) ||
                (state == BTIF_AV_STATE_STARTED))
                return TRUE;
        }
    }
    return FALSE;
}

/*******************************************************************************
**
** Function         btif_av_get_other_connected_idx
**
** Description      Checks if any AV SCB is connected other than the current
**                  index
**
** Returns          BOOLEAN
**
*******************************************************************************/
int btif_av_get_other_connected_idx(int current_index)
{
    //return true if other IDx is connected
    btif_sm_state_t state = BTIF_AV_STATE_IDLE;
    int i;
    for (i = 0; i < btif_max_av_clients; i++)
    {
        if (i != current_index)
        {
            state = btif_sm_get_state(btif_av_cb[i].sm_handle);
            if ((state == BTIF_AV_STATE_OPENED) ||
                (state == BTIF_AV_STATE_STARTED))
                return i;
        }
    }
    return INVALID_INDEX;
}

/*******************************************************************************
**
** Function         btif_av_is_playing_on_other_idx
**
** Description      Checks if any other AV SCB is connected
**
** Returns          BOOLEAN
**
*******************************************************************************/

BOOLEAN btif_av_is_playing_on_other_idx(int current_index)
{
    //return true if other IDx is playing
    btif_sm_state_t state = BTIF_AV_STATE_IDLE;
    int i;
    for (i = 0; i < btif_max_av_clients; i++)
    {
        if (i != current_index)
        {
            state = btif_sm_get_state(btif_av_cb[i].sm_handle);
            if (state == BTIF_AV_STATE_STARTED)
                return TRUE;
        }
    }
    return FALSE;
}

/*******************************************************************************
**
** Function         btif_av_update_current_playing_device
**
** Description      Update the next connected device as playing
**
** Returns          void
**
*******************************************************************************/

static void btif_av_update_current_playing_device(int index)
{
    int i;
    for (i = 0; i < btif_max_av_clients; i++)
    {
        if (i != index)
            btif_av_cb[i].current_playing = TRUE;
    }
}

/*******************************************************************************
**
** Function         btif_av_is_peer_edr
**
** Description      Check if the connected a2dp device supports
**                  EDR or not. Only when connected this function
**                  will accurately provide a true capability of
**                  remote peer. If not connected it will always be false.
**
** Returns          TRUE if remote device is capable of EDR
**
*******************************************************************************/
BOOLEAN btif_av_is_peer_edr(void)
{
    btif_sm_state_t state;
    BOOLEAN peer_edr = FALSE;

    ASSERTC(btif_av_is_connected(), "No active a2dp connection", 0);

    /* If any of the remote in streaming state is BR
     * return FALSE to ensure proper configuration
     * is used. Ideally, since multicast is not supported
     * if any of the connected device is BR device,
     * we should not see both devices in START state.
     */
    for (int index = 0; index < btif_max_av_clients; index ++)
    {
        state = btif_sm_get_state(btif_av_cb[index].sm_handle);
        if ((btif_av_cb[index].flags & BTIF_AV_FLAG_PENDING_START)
            || (state == BTIF_AV_STATE_STARTED))
        {
            if (btif_av_cb[index].edr)
            {
                peer_edr = TRUE;
            }
            else
            {
                return FALSE;
            }
        }
    }
    return peer_edr;
}

/*******************************************************************************
**
** Function         btif_av_any_br_peer
**
** Description      Check if the any of connected devices is BR device.
**
** Returns          TRUE if connected to any BR device, FALSE otherwise.
**
*******************************************************************************/
BOOLEAN btif_av_any_br_peer(void)
{
    btif_sm_state_t state;
    for (int index = 0; index < btif_max_av_clients; index ++)
    {
        state = btif_sm_get_state(btif_av_cb[index].sm_handle);
        if (state >= BTIF_AV_STATE_OPENED)
        {
            if (!btif_av_cb[index].edr)
            {
                BTIF_TRACE_WARNING("%s : Connected to BR device :", __FUNCTION__);
                return TRUE;
            }
        }
    }
    return FALSE;
}

/*******************************************************************************
**
** Function         btif_av_peer_supports_3mbps
**
** Description      check if the connected a2dp device supports
**                  3mbps edr. Only when connected this function
**                  will accurately provide a true capability of
**                  remote peer. If not connected it will always be false.
**
** Returns          TRUE if remote device is EDR and supports 3mbps
**
*******************************************************************************/
BOOLEAN btif_av_peer_supports_3mbps(void)
{
    btif_sm_state_t state;
    ASSERTC(btif_av_is_connected(), "No active a2dp connection", 0);

    for (int index = 0; index < btif_max_av_clients; index ++)
    {
        state = btif_sm_get_state(btif_av_cb[index].sm_handle);
        if ((btif_av_cb[index].flags & BTIF_AV_FLAG_PENDING_START)
            || (state == BTIF_AV_STATE_STARTED))
        {
            if(btif_av_cb[index].edr_3mbps)
                return TRUE;
        }
    }
    return FALSE;
}

/******************************************************************************
**
** Function        btif_av_clear_remote_suspend_flag
**
** Description     Clears btif_av_cd.flags if BTIF_AV_FLAG_REMOTE_SUSPEND is set
**
** Returns         void
******************************************************************************/
void btif_av_clear_remote_suspend_flag(void)
{
    int i;
    for (i = 0; i < btif_max_av_clients; i++)
    {
        BTIF_TRACE_DEBUG(" flag :%x",btif_av_cb[i].flags);
        btif_av_cb[i].flags  &= ~BTIF_AV_FLAG_REMOTE_SUSPEND;
    }
}

/*******************************************************************************
**
** Function         btif_av_move_idle
**
** Description      Opening state is intermediate state. It cannot handle
**                  incoming/outgoing connect/disconnect requests.When ACL
**                  is disconnected and we are in opening state then move back
**                  to idle state which is proper to handle connections.
**
** Returns          Void
**
*******************************************************************************/
void btif_av_move_idle(bt_bdaddr_t bd_addr)
{
    int index =0;
    if (btif_av_cb[0].sm_handle == NULL) return;
    /* inform the application that ACL is disconnected and move to idle state */
    index = btif_av_idx_by_bdaddr(bd_addr.address);
    if (index == btif_max_av_clients)
    {
        BTIF_TRACE_DEBUG("btif_av_move_idle: Already in IDLE");
        return;
    }
    btif_sm_state_t state = btif_sm_get_state(btif_av_cb[index].sm_handle);
    BTIF_TRACE_DEBUG("ACL Disconnected state %d  is same device %d",state,
            memcmp (&bd_addr, &(btif_av_cb[index].peer_bda), sizeof(bd_addr)));
    if (state == BTIF_AV_STATE_OPENING &&
            (memcmp (&bd_addr, &(btif_av_cb[index].peer_bda), sizeof(bd_addr)) == 0))
    {
        BTIF_TRACE_DEBUG("Moving BTIF State from Opening to Idle due to ACL disconnect");
        btif_report_connection_state(BTAV_CONNECTION_STATE_DISCONNECTED, &(btif_av_cb[index].peer_bda));
        BTA_AvClose(btif_av_cb[index].bta_handle);
        btif_sm_change_state(btif_av_cb[index].sm_handle, BTIF_AV_STATE_IDLE);
        btif_queue_advance();
    }
}
/******************************************************************************
**
** Function        btif_av_get_num_playing_devices
**
** Description     Return number of A2dp playing devices
**
** Returns         int
******************************************************************************/
UINT16 btif_av_get_num_playing_devices(void)
{
    UINT16 i;
    UINT16 playing_devices = 0;
    for (i = 0; i < btif_max_av_clients; i++)
    {
        btif_av_cb[i].state = btif_sm_get_state(btif_av_cb[i].sm_handle);
        if (btif_av_cb[i].state ==  BTIF_AV_STATE_STARTED)
        {
            playing_devices++;
        }
    }
    BTIF_TRACE_DEBUG("AV devices playing: %d", playing_devices);

    return playing_devices;
}
/*******************************************************************************
**
** Function        btif_av_get_num_connected_devices
**
** Description     Return number of A2dp connected devices
**
** Returns         int
******************************************************************************/
UINT16 btif_av_get_num_connected_devices(void)
{
    UINT16 i;
    UINT16 connected_devies = 0;
    for (i = 0; i < btif_max_av_clients; i++)
    {
        btif_av_cb[i].state = btif_sm_get_state(btif_av_cb[i].sm_handle);
        if ((btif_av_cb[i].state == BTIF_AV_STATE_OPENED) ||
            (btif_av_cb[i].state ==  BTIF_AV_STATE_STARTED))
        {
            connected_devies++;
        }
    }
    BTIF_TRACE_DEBUG("AV Connection count: %d", connected_devies);

    return connected_devies;
}

/******************************************************************************
**
** Function        btif_av_update_multicast_state
**
** Description     Enable Multicast only if below conditions are satisfied
**                 1. Connected to only 2 EDR HS.
**                 2. Connected to both HS as master.
**                 3. Connected to 2 EDR HS and one BLE device
**                 Multicast will fall back to soft handsoff in below conditions
**                 1. Number of ACL links is more than 2,like connected to HID
**                    initiating connection for HS1 and HS2.
**                 2. Connected to BR and EDR HS.
**                 3. Connected to more then 1 BLE device
**
** Returns         void
******************************************************************************/
void btif_av_update_multicast_state(int index)
{
    UINT16 num_connected_br_edr_devices = 0;
    UINT16 num_connected_le_devices = 0;
    UINT16 num_av_connected = 0;
    UINT16 i = 0;
    BOOLEAN is_slave = FALSE;
    BOOLEAN is_br_hs_connected = FALSE;
    BOOLEAN prev_multicast_state = enable_multicast;

    if (!is_multicast_supported)
    {
        BTIF_TRACE_DEBUG("%s Multicast is Disabled", __FUNCTION__);
        return;
    }

    if (multicast_disabled == TRUE)
    {
        multicast_disabled = FALSE;
        enable_multicast = FALSE;
        BTA_AvEnableMultiCast(FALSE, btif_av_cb[index].bta_handle);
        return;
    }

    BTIF_TRACE_DEBUG("%s Multicast previous state : %s", __FUNCTION__,
        enable_multicast ? "Enabled" : "Disabled" );

    num_connected_br_edr_devices = btif_dm_get_br_edr_links();
    num_connected_le_devices = btif_dm_get_le_links();
    num_av_connected = btif_av_get_num_connected_devices();
    is_br_hs_connected = btif_av_any_br_peer();

    for (i = 0; i < btif_max_av_clients; i++)
    {
        if (btif_av_cb[i].is_slave == TRUE)
        {
            BTIF_TRACE_WARNING("Conected as slave");
            is_slave = TRUE;
            break;
        }
    }

    if ((num_av_connected <= 2) && (is_br_hs_connected != TRUE) &&
        (is_slave == FALSE) && ((num_connected_br_edr_devices <= 2) &&
        (num_connected_le_devices <= 1)))
    {
        enable_multicast = TRUE;
    }
    else
    {
        enable_multicast = FALSE;
    }

    BTIF_TRACE_DEBUG("%s Multicast current state : %s", __FUNCTION__,
        enable_multicast ? "Enabled" : "Disabled" );

    if (prev_multicast_state != enable_multicast)
    {
        BTA_AvEnableMultiCast(enable_multicast,
                btif_av_cb[index].bta_handle);
        HAL_CBACK(bt_av_src_vendor_callbacks, multicast_state_vendor_cb,
              enable_multicast);
    }
}
/******************************************************************************
**
** Function        btif_av_get_multicast_state
**
** Description     Returns TRUE if multicast is enabled else false
**
** Returns         BOOLEAN
******************************************************************************/
BOOLEAN btif_av_get_multicast_state()
{
    return enable_multicast;
}
/******************************************************************************
**
** Function        btif_av_get_ongoing_multicast
**
** Description     Returns TRUE if multicast is ongoing
**
** Returns         BOOLEAN
******************************************************************************/
BOOLEAN btif_av_get_ongoing_multicast()
{
    int i = 0, j = 0;
    if (!is_multicast_supported)
    {
        BTIF_TRACE_DEBUG("Multicast is Disabled");
        return FALSE;
    }
    for (i = 0; i < btif_max_av_clients; i++)
    {
        if (btif_av_cb[i].is_device_playing)
        {
            j++;
        }
    }
    if (j == btif_max_av_clients)
    {
        return TRUE;
    }
    else
    {
        return FALSE;
    }
}

/******************************************************************************
**
** Function        btif_av_is_multicast_supported
**
** Description     Returns TRUE if multicast is supported
**
** Returns         BOOLEAN
******************************************************************************/
BOOLEAN btif_av_is_multicast_supported()
{
    return is_multicast_supported;
}

/******************************************************************************
**
** Function        btif_av_is_offload_supported
**
** Description     Returns split mode status
**
** Returns         TRUE if split mode is enabled, FALSE otherwise
********************************************************************************/
BOOLEAN btif_av_is_offload_supported()
{
    return bt_split_a2dp_enabled;
}

#ifdef BTA_AV_SPLIT_A2DP_ENABLED
int btif_av_get_current_playing_dev_idx(void)
{
    int i;

    for (i = 0; i < btif_max_av_clients; i++)
    {
        if (btif_av_cb[i].current_playing == TRUE)
        {
            BTIF_TRACE_DEBUG("current playing on index = %d",i);
            return i;
        }
    }
    return -1;
}
/******************************************************************************
**
** Function         btif_av_get_streaming_channel_id
**
** Description     Returns streaming channel id
**
** Returns          channel id
********************************************************************************/
UINT16 btif_av_get_streaming_channel_id(void)
{
    int index;

    index = btif_av_get_current_playing_dev_idx();
    if (index != -1)
    {
        BTIF_TRACE_DEBUG("btif_av_get_streaming_channel_id: %u",
                        btif_av_cb[index].channel_id);
        return btif_av_cb[index].channel_id;
    }
    return 0;
}

/******************************************************************************
**
** Function         btif_av_get_peer_addr
**
** Description     Returns peer device address.
**
** Returns          peer address
********************************************************************************/
void btif_av_get_peer_addr(bt_bdaddr_t *peer_bda)
{
    btif_sm_state_t state = BTIF_AV_STATE_IDLE;
    int i;
    memset(peer_bda, 0, sizeof(bt_bdaddr_t));
    for (i = 0; i < btif_max_av_clients; i++)
    {
        state = btif_sm_get_state(btif_av_cb[i].sm_handle);
        if ((state == BTIF_AV_STATE_OPENED) ||
            (state == BTIF_AV_STATE_STARTED))
        {
            BTIF_TRACE_DEBUG("btif_av_get_peer_addr: %u",
                    btif_av_cb[i].peer_bda);
            memcpy(peer_bda, &btif_av_cb[i].peer_bda,
                                    sizeof(bt_bdaddr_t));
        }
    }
}

/******************************************************************************
**
** Function         btif_av_get_playing_device_hdl
**
** Description      Returns current playing device's bta handle
**
** Returns         BTA HANDLE
********************************************************************************/
tBTA_AV_HNDL btif_av_get_playing_device_hdl()
{
    int i;
    btif_sm_state_t state = BTIF_AV_STATE_IDLE;

    for (i = 0; i < btif_max_av_clients; i++)
    {
        state = btif_sm_get_state(btif_av_cb[i].sm_handle);
        if (state == BTIF_AV_STATE_STARTED)
        {
            return btif_av_cb[i].bta_handle;
        }
    }
    return 0;
}

/******************************************************************************
**
** Function         btif_av_get_av_hdl_from_idx
**
** Description      Returns bta handle from the device index
**
** Returns         BTA HANDLE
********************************************************************************/
tBTA_AV_HNDL btif_av_get_av_hdl_from_idx(UINT8 idx)
{
    if (idx == btif_max_av_clients)
    {
        BTIF_TRACE_ERROR("%s: Invalid handle",__func__);
        return -1;
    }
    return btif_av_cb[idx].bta_handle;
}

/******************************************************************************
**
** Function         btif_av_is_codec_offload_supported
**
** Description     check if the correpsonding codec is supported in offload
**
** Returns         TRUE if supported, FALSE otherwise
********************************************************************************/
BOOLEAN btif_av_is_codec_offload_supported(int codec)
{
    BOOLEAN ret = FALSE;
    BTIF_TRACE_DEBUG("btif_av_is_codec_offload_supported = %s",dump_av_codec_name(codec));
    switch(codec)
    {
        case SBC:
            ret = btif_av_codec_offload.sbc_offload;
            break;
        case APTX:
            ret = btif_av_codec_offload.aptx_offload;
            break;
        case AAC:
            ret = btif_av_codec_offload.aac_offload;
            break;
        case APTXHD:
            ret = btif_av_codec_offload.aptxhd_offload;
            break;
        default:
            ret = FALSE;
    }
    BTIF_TRACE_DEBUG("btif_av_is_codec_offload_supported %s codec supported = %d",dump_av_codec_name(codec),ret);
    return ret;
}

/******************************************************************************
**
** Function         btif_av_is_under_handoff
**
** Description     check if AV state is under handoff
**
** Returns         TRUE if handoff is triggered, FALSE otherwise
********************************************************************************/
BOOLEAN btif_av_is_under_handoff()
{
    int i;
    btif_sm_state_t state = BTIF_AV_STATE_IDLE;

    BTIF_TRACE_DEBUG("btif_av_is_under_handoff");

    for (i = 0; i < btif_max_av_clients; i++)
    {
        state = btif_sm_get_state(btif_av_cb[i].sm_handle);
        if (btif_av_cb[i].dual_handoff &&
            (state == BTIF_AV_STATE_STARTED || state == BTIF_AV_STATE_OPENED))
        {
            /* If a2dp reconfigure is triggered when playing device disconnect is
             * initiated locally then return false, otherwise wait till the suspend cfm
             * is received from the remote.
             */
            return TRUE;
        }
    }
    return FALSE;
}
#endif

