/******************************************************************************
 *
 *  Copyright (C) 2001-2012 Broadcom Corporation
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

#define LOG_TAG "bt_bte"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <sys/time.h>
#include <time.h>
#include <dlfcn.h>

#include "avrc_api.h"
#include "bta_api.h"
#include "bte.h"
#include "btm_api.h"
#include "btu.h"
#include "gap_api.h"
#include "bt_common.h"
#include "l2c_api.h"
#include "osi/include/config.h"
#include "osi/include/log.h"
#include "osi/include/log.h"
#include "port_api.h"
#include "sdp_api.h"
#include "stack_config.h"

#if (AVDT_INCLUDED==TRUE)
#include "avdt_api.h"
#endif
#if (A2D_INCLUDED==TRUE)
#include "a2d_api.h"
#endif
#if (BNEP_INCLUDED==TRUE)
#include "bnep_api.h"
#endif
#if (PAN_INCLUDED==TRUE)
#include "pan_api.h"
#endif
#if (BLE_INCLUDED==TRUE)
#include "gatt_api.h"
#include "smp_api.h"
#endif

#ifdef USE_ANDROID_LOGGING
#include <utils/Log.h>
#define LOGI0 logi
#define LOGD0 logd
#define LOGW0 logw
#define LOGE0 loge
#endif

#ifndef DEFAULT_CONF_TRACE_LEVEL
#define DEFAULT_CONF_TRACE_LEVEL BT_TRACE_LEVEL_WARNING
#endif

#ifndef BTE_LOG_BUF_SIZE
#define BTE_LOG_BUF_SIZE  1024
#endif

#define VND_PKT_SIZE_BLOCKS 4
#define VND_PKT_HEADER_SIZE 5 //(VND_PKT_SIZE_BLOCKS + 1)
#define VND_PKT_BODY_SIZE 1021

#define BTE_LOG_MAX_SIZE  (BTE_LOG_BUF_SIZE - 12)

#define MSG_BUFFER_OFFSET 0

/* LayerIDs for BTA, currently everything maps onto appl_trace_level */
static const char * const bt_layer_tags[] = {
  "bt_btif",
  "bt_usb",
  "bt_serial",
  "bt_socket",
  "bt_rs232",
  "bt_lc",
  "bt_lm",
  "bt_hci",
  "bt_l2cap",
  "bt_rfcomm",
  "bt_sdp",
  "bt_tcs",
  "bt_obex",
  "bt_btm",
  "bt_gap",
  "UNUSED",
  "UNUSED",
  "bt_icp",
  "bt_hsp2",
  "bt_spp",
  "bt_ctp",
  "bt_bpp",
  "bt_hcrp",
  "bt_ftp",
  "bt_opp",
  "bt_btu",
  "bt_gki",                             /* OBSOLETED */
  "bt_bnep",
  "bt_pan",
  "bt_hfp",
  "bt_hid",
  "bt_bip",
  "bt_avp",
  "bt_a2d",
  "bt_sap",
  "bt_amp",
  "bt_mca",
  "bt_att",
  "bt_smp",
  "bt_nfc",
  "bt_nci",
  "bt_idep",
  "bt_ndep",
  "bt_llcp",
  "bt_rw",
  "bt_ce",
  "bt_snep",
  "bt_ndef",
  "bt_nfa",
};
static uint8_t BTAPP_SetTraceLevel(uint8_t new_level);
static uint8_t BTIF_SetTraceLevel(uint8_t new_level);
static uint8_t BTU_SetTraceLevel(uint8_t new_level);
static uint8_t AUDIO_Latency_SetTraceLevel(uint8_t new_level);

/* make sure list is order by increasing layer id!!! */
static tBTTRC_FUNC_MAP bttrc_set_level_map[] = {
  {BTTRC_ID_STK_BTU, BTTRC_ID_STK_HCI, BTU_SetTraceLevel, "TRC_HCI", DEFAULT_CONF_TRACE_LEVEL},
  {BTTRC_ID_STK_L2CAP, BTTRC_ID_STK_L2CAP, L2CA_SetTraceLevel, "TRC_L2CAP", DEFAULT_CONF_TRACE_LEVEL},
  {BTTRC_ID_STK_RFCOMM, BTTRC_ID_STK_RFCOMM_DATA, PORT_SetTraceLevel, "TRC_RFCOMM", DEFAULT_CONF_TRACE_LEVEL},
#if (AVDT_INCLUDED==TRUE)
  {BTTRC_ID_STK_AVDT, BTTRC_ID_STK_AVDT, AVDT_SetTraceLevel, "TRC_AVDT", DEFAULT_CONF_TRACE_LEVEL},
#endif
  {BTTRC_ID_STK_AVRC, BTTRC_ID_STK_AVRC, AVRC_SetTraceLevel, "TRC_AVRC", DEFAULT_CONF_TRACE_LEVEL},
#if (AVDT_INCLUDED==TRUE)
  //{BTTRC_ID_AVDT_SCB, BTTRC_ID_AVDT_CCB, NULL, "TRC_AVDT_SCB", DEFAULT_CONF_TRACE_LEVEL},
#endif
#if (A2D_INCLUDED==TRUE)
  {BTTRC_ID_STK_A2D, BTTRC_ID_STK_A2D, A2D_SetTraceLevel, "TRC_A2D", DEFAULT_CONF_TRACE_LEVEL},
#endif
#if (BNEP_INCLUDED==TRUE)
  {BTTRC_ID_STK_BNEP, BTTRC_ID_STK_BNEP, BNEP_SetTraceLevel, "TRC_BNEP", DEFAULT_CONF_TRACE_LEVEL},
#endif
  {BTTRC_ID_STK_BTM_ACL, BTTRC_ID_STK_BTM_SEC, BTM_SetTraceLevel, "TRC_BTM", DEFAULT_CONF_TRACE_LEVEL},
  {BTTRC_ID_STK_GAP, BTTRC_ID_STK_GAP, GAP_SetTraceLevel, "TRC_GAP", DEFAULT_CONF_TRACE_LEVEL},
#if (PAN_INCLUDED==TRUE)
  {BTTRC_ID_STK_PAN, BTTRC_ID_STK_PAN, PAN_SetTraceLevel, "TRC_PAN", DEFAULT_CONF_TRACE_LEVEL},
#endif
  {BTTRC_ID_STK_SDP, BTTRC_ID_STK_SDP, SDP_SetTraceLevel, "TRC_SDP", DEFAULT_CONF_TRACE_LEVEL},
#if (BLE_INCLUDED==TRUE)
  {BTTRC_ID_STK_GATT, BTTRC_ID_STK_GATT, GATT_SetTraceLevel, "TRC_GATT", DEFAULT_CONF_TRACE_LEVEL},
  {BTTRC_ID_STK_SMP, BTTRC_ID_STK_SMP, SMP_SetTraceLevel, "TRC_SMP", DEFAULT_CONF_TRACE_LEVEL},
#endif
#if (BT_TRACE_LATENCY_AUDIO == TRUE)
    {BTTRC_ID_LATENCY_AUDIO, BTTRC_ID_LATENCY_AUDIO, AUDIO_Latency_SetTraceLevel, "TRC_LATENCY_AUDIO", DEFAULT_CONF_TRACE_LEVEL},
#endif

  /* LayerIDs for BTA, currently everything maps onto appl_trace_level.
   */
  {BTTRC_ID_BTA_ACC, BTTRC_ID_BTAPP, BTAPP_SetTraceLevel, "TRC_BTAPP", DEFAULT_CONF_TRACE_LEVEL},
  {BTTRC_ID_BTA_ACC, BTTRC_ID_BTAPP, BTIF_SetTraceLevel, "TRC_BTIF", DEFAULT_CONF_TRACE_LEVEL},

  {0, 0, NULL, NULL, DEFAULT_CONF_TRACE_LEVEL}
};

#ifndef ANDROID
void LogMsg(uint32_t trace_set_mask, const char *fmt_str, ...) {
  char buffer[BTE_LOG_BUF_SIZE];
  int offset = MSG_BUFFER_OFFSET;
  int trace_layer = TRACE_GET_LAYER(trace_set_mask);
  if (trace_layer >= TRACE_LAYER_MAX_NUM)
    trace_layer = 0;

#ifndef USE_ANDROID_LOGGING
  offset += strlen("bt_stack : ") +
            strlen(bt_layer_tags[trace_layer]) + strlen(" : ");
  snprintf(buffer, offset+1 , "bt_stack : %s : ",
        bt_layer_tags[trace_layer]);
#else
  offset += strlen(bt_layer_tags[trace_layer]) + strlen(" : ");
  snprintf(buffer, offset+1 , "%s : ",
        bt_layer_tags[trace_layer]);
#endif

  va_list ap;
  va_start(ap, fmt_str);
  vsnprintf(&buffer[offset], BTE_LOG_MAX_SIZE - offset, fmt_str, ap);
  va_end(ap);

  switch ( TRACE_GET_TYPE(trace_set_mask) ) {
    case TRACE_TYPE_ERROR:
      LOGE0(buffer);
      break;
    case TRACE_TYPE_WARNING:
      LOGW0(buffer);
      break;
    case TRACE_TYPE_API:
    case TRACE_TYPE_EVENT:
      LOGI0(buffer);
      break;
    case TRACE_TYPE_DEBUG:
      LOGD0(buffer);
      break;
    default:
      LOGE0(buffer);
      break;
    }
}
#else
void LogMsg(uint32_t trace_set_mask, const char *fmt_str, ...) {
  static char buffer[BTE_LOG_BUF_SIZE];
  int trace_layer = TRACE_GET_LAYER(trace_set_mask);
  if (trace_layer >= TRACE_LAYER_MAX_NUM)
    trace_layer = 0;

  va_list ap;
  va_start(ap, fmt_str);
  vsnprintf(&buffer[MSG_BUFFER_OFFSET], BTE_LOG_MAX_SIZE, fmt_str, ap);
  va_end(ap);

  switch ( TRACE_GET_TYPE(trace_set_mask) ) {
    case TRACE_TYPE_ERROR:
      LOG_ERROR(bt_layer_tags[trace_layer], "%s", buffer);
      break;
    case TRACE_TYPE_WARNING:
      LOG_WARN(bt_layer_tags[trace_layer], "%s", buffer);
      break;
    case TRACE_TYPE_API:
    case TRACE_TYPE_EVENT:
      LOG_INFO(bt_layer_tags[trace_layer], "%s", buffer);
      break;
    case TRACE_TYPE_DEBUG:
      LOG_DEBUG(bt_layer_tags[trace_layer], "%s", buffer);
      break;
    default:
      LOG_ERROR(bt_layer_tags[trace_layer], "%s", buffer);      /* we should never get this */
      break;
  }
}
#endif

void vnd_LogMsg(uint32_t trace_set_mask, const char *fmt_str, ...) {
  int trace_layer = TRACE_GET_LAYER(trace_set_mask);
  const char *tag;
  if (trace_layer >= TRACE_LAYER_MAX_NUM)
    trace_layer = 0;

  tag = bt_layer_tags[trace_layer];

  va_list ap;
  va_start(ap, fmt_str);
  if(logger_interface)
    logger_interface->send_log_msg(tag, fmt_str, ap);
  va_end(ap);
}

/* this function should go into BTAPP_DM for example */
static uint8_t BTAPP_SetTraceLevel(uint8_t new_level) {
  if (new_level != 0xFF)
    appl_trace_level = new_level;

  return appl_trace_level;
}

static uint8_t BTIF_SetTraceLevel(uint8_t new_level) {
  if (new_level != 0xFF)
    btif_trace_level = new_level;

  return btif_trace_level;
}

static uint8_t AUDIO_Latency_SetTraceLevel( uint8_t new_level )
{
    if (new_level != 0xFF)
    audio_latency_trace_level = new_level;

    return (audio_latency_trace_level);
}


static uint8_t BTU_SetTraceLevel(uint8_t new_level) {
  if (new_level != 0xFF)
    btu_trace_level = new_level;

  return btu_trace_level;
}

static void load_levels_from_config(const config_t *config) {
  assert(config != NULL);

  for (tBTTRC_FUNC_MAP *functions = &bttrc_set_level_map[0]; functions->trc_name; ++functions) {
    LOG_INFO(LOG_TAG, "BTE_InitTraceLevels -- %s", functions->trc_name);
    int value = config_get_int(config, CONFIG_DEFAULT_SECTION, functions->trc_name, -1);
    if (value != -1)
      functions->trace_level = value;

    if (functions->p_f)
      functions->p_f(functions->trace_level);
  }
}

static future_t *init(void) {
  const stack_config_t *stack_config = stack_config_get_interface();
  if (!stack_config->get_trace_config_enabled()) {
    LOG_INFO(LOG_TAG, "using compile default trace settings");
    return NULL;
  }
  load_levels_from_config(stack_config->get_all());
  return NULL;
}

//TODO: Fix this
#ifndef ANDROID
#define EXPORT_SYMBOL   __attribute__((visibility("default")))
#endif

EXPORT_SYMBOL const module_t bte_logmsg_module = {
  .name = BTE_LOGMSG_MODULE,
  .init = init,
  .start_up = NULL,
  .shut_down = NULL,
  .clean_up = NULL,
  .dependencies = {
    STACK_CONFIG_MODULE,
    NULL
  }
};

/********************************************************************************
 **
 **    Function Name:    BTA_setStackLog
 **
 **    Purpose:          Set the trace level of the different layers of stack
                         based on the stack layer and level as input
 **
 **    Input Parameters: const char* log_layer, int log_level
                         Example : TRC_HCI, 5
 **
 **    Returns:          void
 **
 *********************************************************************************/

void BTA_setStackLog( const char* log_layer, int log_level)
{
    const tBTTRC_FUNC_MAP *p_f_map;
    int new_level = 0;
    int layer_found = 0;
    p_f_map = &bttrc_set_level_map[0];

    while ( 0 != p_f_map->layer_id_start )
    {

        if( (NULL != p_f_map->p_f) && !strcmp( p_f_map->trc_name, log_layer))
        {
            new_level = p_f_map->p_f(log_level);
            LOG_INFO("BTA_setStackLog: New trace level set for layer %s is %d", log_layer, new_level);
            layer_found = 1;
            break;
        }
        p_f_map++;
    }

    if ( layer_found == 0 )
    {
        LOG_INFO("BTA_setStackLog: Unable to set Layer %s with level %d. Layer not found", log_layer, log_level);
    }
}
