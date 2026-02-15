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

/*******************************************************************************
 *
 *  Filename:      btif_av.h
 *
 *  Description:   Main API header file for all BTIF AV functions accessed
 *                 from internal stack.
 *
 *******************************************************************************/

#ifndef BTIF_AV_H
#define BTIF_AV_H

#include "btif_common.h"
#include "btif_sm.h"
#include "bta_av_api.h"

#define BTIF_RC_HANDLE_NONE 0xFF

/*******************************************************************************
**  Type definitions for callback functions
********************************************************************************/

typedef enum {
    /* Reuse BTA_AV_XXX_EVT - No need to redefine them here */
    BTIF_AV_CONNECT_REQ_EVT = BTA_AV_MAX_EVT,
    BTIF_AV_DISCONNECT_REQ_EVT,
    BTIF_AV_START_STREAM_REQ_EVT,
    BTIF_AVK_START_STREAM_REQ_EVT,
    BTIF_AV_STOP_STREAM_REQ_EVT,
    BTIF_AV_SUSPEND_STREAM_REQ_EVT,
    BTIF_AVK_SUSPEND_STREAM_REQ_EVT,
    BTIF_AV_SINK_CONFIG_REQ_EVT,
    BTIF_AV_OFFLOAD_START_REQ_EVT,
    BTIF_AV_SINK_FOCUS_REQ_EVT,
    BTIF_AV_CLEANUP_REQ_EVT,
    BTIF_AV_UPDATE_ENCODER_REQ_EVT,
    BTIF_AV_INIT_REQ_EVT,
    BTIF_AV_SRC_CONFIG_REQ_EVT,
} btif_av_sm_event_t;


/*******************************************************************************
**  BTIF AV API
********************************************************************************/

/*******************************************************************************
**
** Function         btif_av_get_sm_handle
**
** Description      Fetches current av SM handle
**
** Returns          None
**
*******************************************************************************/

btif_sm_handle_t btif_av_get_sm_handle(void);

/*******************************************************************************
**
** Function         btif_av_get_addr
**
** Description      Fetches current AV BD address
**
** Returns          BD address
**
*******************************************************************************/

bt_bdaddr_t btif_av_get_addr(BD_ADDR address);

/*******************************************************************************
** Function         btif_av_is_sink_enabled
**
** Description      Checks if A2DP Sink is enabled or not
**
** Returns          TRUE if A2DP Sink is enabled, false otherwise
**
*******************************************************************************/

BOOLEAN btif_av_is_sink_enabled(void);

/*******************************************************************************
**
** Function         btif_av_stream_ready
**
** Description      Checks whether AV is ready for starting a stream
**
** Returns          None
**
*******************************************************************************/

BOOLEAN btif_av_stream_ready(void);

/*******************************************************************************
**
** Function         btif_av_stream_started_ready
**
** Description      Checks whether AV ready for media start in streaming state
**
** Returns          None
**
*******************************************************************************/

BOOLEAN btif_av_stream_started_ready(void);

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
void btif_dispatch_sm_event(btif_av_sm_event_t event, void *p_data, int len);

/*******************************************************************************
**
** Function         btif_av_init
**
** Description      Initializes btif AV if not already done
**
** Returns          bt_status_t
**
*******************************************************************************/

bt_status_t btif_av_init(int service_id);

/*******************************************************************************
**
** Function         btif_av_is_connected
**
** Description      Checks if av has a connected sink
**
** Returns          BOOLEAN
**
*******************************************************************************/

BOOLEAN btif_av_is_connected(void);


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

BOOLEAN btif_av_is_peer_edr(void);

#ifdef USE_AUDIO_TRACK
/*******************************************************************************
**
** Function         audio_focus_status
**
** Description      Update Audio Focus State
**
** Returns          None
**
*******************************************************************************/
void audio_focus_status(int state);

/*******************************************************************************
**
** Function         btif_queue_focus_request
**
** Description      This is used to move context to btif and
**                  queue audio_focus_request
**
** Returns          none
**
*******************************************************************************/
void btif_queue_focus_request(void);
#endif

/*******************************************************************************
 **
 ** Function         btif_media_enque_pcm_data
 **
 ** Description      queues PCM data
 **
 ** Returns          void
 **
 *******************************************************************************/
void btif_media_enque_pcm_data(UINT8 *data, UINT16 size);

/*******************************************************************************
 **
 ** Function         btif_media_avk_fetch_pcm_data
 **
 ** Description      fetch PCM data
 **
 ** Returns          size of data read
 **
 *******************************************************************************/
UINT32 btif_media_avk_fetch_pcm_data(UINT8 *data, UINT32 size);

/******************************************************************************
**
** Function         btif_av_clear_remote_suspend_flag
**
** Description      Clears remote suspended flag
**
** Returns          Void
********************************************************************************/
void btif_av_clear_remote_suspend_flag(void);

/*******************************************************************************
**
** Function         btif_av_any_br_peer
**
** Description      Check if the any of connected devices is BR device.
**
** Returns          TRUE if connected to any BR device, FALSE otherwise.
**
*******************************************************************************/

BOOLEAN btif_av_any_br_peer(void);

/*******************************************************************************
**
** Function         btif_av_peer_supports_3mbps
**
** Description      Check if the connected A2DP device supports
**                  3 Mbps EDR. This function will only work while connected.
**                  If not connected it will always return false.
**
** Returns          TRUE if remote device is EDR and supports 3 Mbps
**
*******************************************************************************/
BOOLEAN btif_av_peer_supports_3mbps(void);

#ifdef BTA_AV_SPLIT_A2DP_ENABLED
/******************************************************************************
**
** Function         btif_av_get_streaming_channel_id
**
** Description      Returns streaming channel id
**
** Returns          channel id
********************************************************************************/
UINT16 btif_av_get_streaming_channel_id(void);

/******************************************************************************
**
** Function         btif_av_get_peer_addr
**
** Description     Returns peer device address
**
** Returns          peer address
********************************************************************************/
void btif_av_get_peer_addr(bt_bdaddr_t *peer_bda);

#else
#define btif_av_get_streaming_channel_id() (0)
#define btif_av_get_peer_addr(peer_bda) (0)
#endif

/*******************************************************************************
**
** Function         btif_av_get_multicast_state
**
** Description      Check if A2DP multicast is enabled
**
** Returns          TRUE if a2dp multicast is enabled
**
*******************************************************************************/
BOOLEAN btif_av_get_multicast_state();

/*******************************************************************************
**
** Function         btif_av_is_multicast_supported
**
** Description      Check if A2DP multicast is supported
**
** Returns          TRUE if a2dp multicast is supported
**
*******************************************************************************/
BOOLEAN btif_av_is_multicast_supported();

#endif /* BTIF_AV_H */
