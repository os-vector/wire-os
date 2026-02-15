/******************************************************************************
 *
 *  Copyright (C) 1999-2012 Broadcom Corporation
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

/******************************************************************************
 *
 *  This file contains function of the HCIC unit to format and send HCI
 *  commands.
 *
 ******************************************************************************/

#include "bt_target.h"
#include "bt_common.h"
#include "hcidefs.h"
#include "hcimsgs.h"
#include "hcidefs.h"
#include "btu.h"

#include <stddef.h>
#include <string.h>

#if (defined BLE_INCLUDED) && (BLE_INCLUDED == TRUE)

BOOLEAN btsnd_hcic_ble_set_local_used_feat (UINT8 feat_set[8])
{
    BT_HDR *p = (BT_HDR *)osi_malloc(HCI_CMD_BUF_SIZE);
    UINT8 *pp = (UINT8 *)(p + 1);

    p->len    = HCIC_PREAMBLE_SIZE + HCIC_PARAM_SIZE_SET_USED_FEAT_CMD;
    p->offset = 0;

    UINT16_TO_STREAM (pp, HCI_BLE_WRITE_LOCAL_SPT_FEAT);
    ARRAY_TO_STREAM (pp, feat_set, HCIC_PARAM_SIZE_SET_USED_FEAT_CMD);

    btu_hcif_send_cmd (LOCAL_BR_EDR_CONTROLLER_ID,  p);
    return (TRUE);
}

BOOLEAN btsnd_hcic_ble_set_random_addr (BD_ADDR random_bda)
{
    BT_HDR *p = (BT_HDR *)osi_malloc(HCI_CMD_BUF_SIZE);
    UINT8 *pp = (UINT8 *)(p + 1);

    p->len    = HCIC_PREAMBLE_SIZE + HCIC_PARAM_SIZE_WRITE_RANDOM_ADDR_CMD;
    p->offset = 0;

    UINT16_TO_STREAM (pp, HCI_BLE_WRITE_RANDOM_ADDR);
    UINT8_TO_STREAM  (pp,  HCIC_PARAM_SIZE_WRITE_RANDOM_ADDR_CMD);

    BDADDR_TO_STREAM (pp, random_bda);

    btu_hcif_send_cmd (LOCAL_BR_EDR_CONTROLLER_ID,  p);
    return (TRUE);
}

BOOLEAN btsnd_hcic_ble_write_adv_params (UINT16 adv_int_min, UINT16 adv_int_max,
                                       UINT8 adv_type, UINT8 addr_type_own,
                                       UINT8 addr_type_dir, BD_ADDR direct_bda,
                                       UINT8 channel_map, UINT8 adv_filter_policy)
{
    BT_HDR *p = (BT_HDR *)osi_malloc(HCI_CMD_BUF_SIZE);
    UINT8 *pp = (UINT8 *)(p + 1);

    p->len    = HCIC_PREAMBLE_SIZE + HCIC_PARAM_SIZE_BLE_WRITE_ADV_PARAMS ;
    p->offset = 0;

    UINT16_TO_STREAM (pp, HCI_BLE_WRITE_ADV_PARAMS);
    UINT8_TO_STREAM  (pp, HCIC_PARAM_SIZE_BLE_WRITE_ADV_PARAMS );

    UINT16_TO_STREAM (pp, adv_int_min);
    UINT16_TO_STREAM (pp, adv_int_max);
    UINT8_TO_STREAM (pp, adv_type);
    UINT8_TO_STREAM (pp, addr_type_own);
    UINT8_TO_STREAM (pp, addr_type_dir);
    BDADDR_TO_STREAM (pp, direct_bda);
    UINT8_TO_STREAM (pp, channel_map);
    UINT8_TO_STREAM (pp, adv_filter_policy);

    btu_hcif_send_cmd (LOCAL_BR_EDR_CONTROLLER_ID,  p);
    return (TRUE);
}
BOOLEAN btsnd_hcic_ble_read_adv_chnl_tx_power (void)
{
    BT_HDR *p = (BT_HDR *)osi_malloc(HCI_CMD_BUF_SIZE);
    UINT8 *pp = (UINT8 *)(p + 1);

    p->len    = HCIC_PREAMBLE_SIZE + HCIC_PARAM_SIZE_READ_CMD;
    p->offset = 0;

    UINT16_TO_STREAM (pp, HCI_BLE_READ_ADV_CHNL_TX_POWER);
    UINT8_TO_STREAM  (pp, HCIC_PARAM_SIZE_READ_CMD);

    btu_hcif_send_cmd (LOCAL_BR_EDR_CONTROLLER_ID,  p);
    return (TRUE);

}

BOOLEAN btsnd_hcic_ble_set_adv_data (UINT8 data_len, UINT8 *p_data)
{
    BT_HDR *p = (BT_HDR *)osi_malloc(HCI_CMD_BUF_SIZE);
    UINT8 *pp = (UINT8 *)(p + 1);

    p->len    = HCIC_PREAMBLE_SIZE + HCIC_PARAM_SIZE_BLE_WRITE_ADV_DATA + 1;
    p->offset = 0;

    UINT16_TO_STREAM (pp, HCI_BLE_WRITE_ADV_DATA);
    UINT8_TO_STREAM  (pp, HCIC_PARAM_SIZE_BLE_WRITE_ADV_DATA + 1);

    memset(pp, 0, HCIC_PARAM_SIZE_BLE_WRITE_ADV_DATA);

    if (p_data != NULL && data_len > 0)
    {
        if (data_len > HCIC_PARAM_SIZE_BLE_WRITE_ADV_DATA)
            data_len = HCIC_PARAM_SIZE_BLE_WRITE_ADV_DATA;

        UINT8_TO_STREAM (pp, data_len);

        ARRAY_TO_STREAM (pp, p_data, data_len);
    }
    btu_hcif_send_cmd (LOCAL_BR_EDR_CONTROLLER_ID,  p);

    return (TRUE);
}
BOOLEAN btsnd_hcic_ble_set_scan_rsp_data (UINT8 data_len, UINT8 *p_scan_rsp)
{
    BT_HDR *p = (BT_HDR *)osi_malloc(HCI_CMD_BUF_SIZE);
    UINT8 *pp = (UINT8 *)(p + 1);

    p->len    = HCIC_PREAMBLE_SIZE + HCIC_PARAM_SIZE_BLE_WRITE_SCAN_RSP + 1;
    p->offset = 0;

    UINT16_TO_STREAM (pp, HCI_BLE_WRITE_SCAN_RSP_DATA);
    UINT8_TO_STREAM  (pp, HCIC_PARAM_SIZE_BLE_WRITE_SCAN_RSP + 1);

    memset(pp, 0, HCIC_PARAM_SIZE_BLE_WRITE_SCAN_RSP);

    if (p_scan_rsp != NULL && data_len > 0)
    {

        if (data_len > HCIC_PARAM_SIZE_BLE_WRITE_SCAN_RSP )
            data_len = HCIC_PARAM_SIZE_BLE_WRITE_SCAN_RSP;

        UINT8_TO_STREAM (pp, data_len);

        ARRAY_TO_STREAM (pp, p_scan_rsp, data_len);
    }

    btu_hcif_send_cmd (LOCAL_BR_EDR_CONTROLLER_ID,  p);

    return (TRUE);
}

BOOLEAN btsnd_hcic_ble_set_adv_enable (UINT8 adv_enable)
{
    BT_HDR *p = (BT_HDR *)osi_malloc(HCI_CMD_BUF_SIZE);
    UINT8 *pp = (UINT8 *)(p + 1);

    p->len    = HCIC_PREAMBLE_SIZE + HCIC_PARAM_SIZE_WRITE_ADV_ENABLE;
    p->offset = 0;

    UINT16_TO_STREAM (pp, HCI_BLE_WRITE_ADV_ENABLE);
    UINT8_TO_STREAM  (pp, HCIC_PARAM_SIZE_WRITE_ADV_ENABLE);

    UINT8_TO_STREAM (pp, adv_enable);

    btu_hcif_send_cmd (LOCAL_BR_EDR_CONTROLLER_ID,  p);
    return (TRUE);
}
BOOLEAN btsnd_hcic_ble_set_scan_params (UINT8 scan_type,
                                          UINT16 scan_int, UINT16 scan_win,
                                          UINT8 addr_type_own, UINT8 scan_filter_policy)
{
    BT_HDR *p = (BT_HDR *)osi_malloc(HCI_CMD_BUF_SIZE);
    UINT8 *pp = (UINT8 *)(p + 1);

    p->len    = HCIC_PREAMBLE_SIZE + HCIC_PARAM_SIZE_BLE_WRITE_SCAN_PARAM;
    p->offset = 0;

    UINT16_TO_STREAM (pp, HCI_BLE_WRITE_SCAN_PARAMS);
    UINT8_TO_STREAM  (pp, HCIC_PARAM_SIZE_BLE_WRITE_SCAN_PARAM);

    UINT8_TO_STREAM (pp, scan_type);
    UINT16_TO_STREAM (pp, scan_int);
    UINT16_TO_STREAM (pp, scan_win);
    UINT8_TO_STREAM (pp, addr_type_own);
    UINT8_TO_STREAM (pp, scan_filter_policy);

    btu_hcif_send_cmd (LOCAL_BR_EDR_CONTROLLER_ID,  p);
    return (TRUE);
}

BOOLEAN btsnd_hcic_ble_set_scan_enable (UINT8 scan_enable, UINT8 duplicate)
{
    BT_HDR *p = (BT_HDR *)osi_malloc(HCI_CMD_BUF_SIZE);
    UINT8 *pp = (UINT8 *)(p + 1);

    p->len    = HCIC_PREAMBLE_SIZE + HCIC_PARAM_SIZE_BLE_WRITE_SCAN_ENABLE;
    p->offset = 0;

    UINT16_TO_STREAM (pp, HCI_BLE_WRITE_SCAN_ENABLE);
    UINT8_TO_STREAM  (pp, HCIC_PARAM_SIZE_BLE_WRITE_SCAN_ENABLE);

    UINT8_TO_STREAM (pp, scan_enable);
    UINT8_TO_STREAM (pp, duplicate);

    btu_hcif_send_cmd (LOCAL_BR_EDR_CONTROLLER_ID,  p);
    return (TRUE);
}

BOOLEAN btsnd_hcic_ble_set_extended_scan_params (UINT8 scan_phys,
                                          UINT8 scan_type,
                                          UINT16 scan_int, UINT16 scan_win,
                                          UINT16 scan_int_coded, UINT16 scan_win_coded,
                                          UINT8 addr_type_own, UINT8 scan_filter_policy)
{
    BT_HDR *p = (BT_HDR *)osi_malloc(HCI_CMD_BUF_SIZE);
    UINT8 *pp = (UINT8 *)(p + 1);

    if(scan_phys == 0)
        scan_phys = HCIC_SCAN_PHY_LE_1M;

    p->offset = 0;

    UINT16_TO_STREAM (pp, HCI_BLE_WRITE_EXT_SCAN_PARAMS);

    if((scan_phys == HCIC_SCAN_PHY_LE_1M) || (scan_phys == HCIC_SCAN_PHY_LE_CODED))
    {
        p->len = HCIC_PREAMBLE_SIZE + HCIC_PARAM_SIZE_BLE_WRITE_EXT_SCAN_PARAM;
        UINT8_TO_STREAM (pp, HCIC_PARAM_SIZE_BLE_WRITE_EXT_SCAN_PARAM);
    }
    else if(scan_phys == HCIC_SCAN_PHY_LE_1M_CODED)
    {
        p->len = HCIC_PREAMBLE_SIZE + HCIC_PARAM_SIZE_BLE_WRITE_EXT_SCAN_PARAM + 5;
        UINT8_TO_STREAM (pp, (HCIC_PARAM_SIZE_BLE_WRITE_EXT_SCAN_PARAM + 5));
    }
    UINT8_TO_STREAM (pp, addr_type_own);
    UINT8_TO_STREAM (pp, scan_filter_policy);
    UINT8_TO_STREAM (pp, scan_phys);
    UINT8_TO_STREAM (pp, scan_type);
    UINT16_TO_STREAM (pp, scan_int);
    UINT16_TO_STREAM (pp, scan_win);


    if(scan_phys == HCIC_SCAN_PHY_LE_1M_CODED)
    {
        UINT8_TO_STREAM (pp, scan_type);
        UINT16_TO_STREAM (pp, scan_int_coded);
        UINT16_TO_STREAM (pp, scan_win_coded);
    }

    btu_hcif_send_cmd (LOCAL_BR_EDR_CONTROLLER_ID,  p);
    return (TRUE);
}

BOOLEAN btsnd_hcic_ble_set_extended_scan_enable (UINT8 scan_enable, UINT8 duplicate,
                                                 UINT16 duraton, UINT16 period)
{
    BT_HDR *p = (BT_HDR *)osi_malloc(HCI_CMD_BUF_SIZE);
    UINT8 *pp = (UINT8 *)(p + 1);

    p->len    = HCIC_PREAMBLE_SIZE + HCIC_PARAM_SIZE_BLE_WRITE_EXT_SCAN_ENABLE;
    p->offset = 0;

    UINT16_TO_STREAM (pp, HCI_BLE_WRITE_EXT_SCAN_ENABLE);
    UINT8_TO_STREAM  (pp, HCIC_PARAM_SIZE_BLE_WRITE_EXT_SCAN_ENABLE);

    UINT8_TO_STREAM (pp, scan_enable);
    UINT8_TO_STREAM (pp, duplicate);
    UINT16_TO_STREAM (pp, duraton);
    UINT16_TO_STREAM (pp, period);

    btu_hcif_send_cmd (LOCAL_BR_EDR_CONTROLLER_ID,  p);
    return (TRUE);
}

/* link layer connection management commands */
BOOLEAN btsnd_hcic_ble_create_ll_conn (UINT16 scan_int, UINT16 scan_win,
                                       UINT8 init_filter_policy,
                                       UINT8 addr_type_peer, BD_ADDR bda_peer,
                                       UINT8 addr_type_own,
                                       UINT16 conn_int_min, UINT16 conn_int_max,
                                       UINT16 conn_latency, UINT16 conn_timeout,
                                       UINT16 min_ce_len, UINT16 max_ce_len)
{
    BT_HDR *p = (BT_HDR *)osi_malloc(HCI_CMD_BUF_SIZE);
    UINT8 *pp = (UINT8 *)(p + 1);

    p->len    = HCIC_PREAMBLE_SIZE + HCIC_PARAM_SIZE_BLE_CREATE_LL_CONN;
    p->offset = 0;

    UINT16_TO_STREAM (pp, HCI_BLE_CREATE_LL_CONN);
    UINT8_TO_STREAM  (pp, HCIC_PARAM_SIZE_BLE_CREATE_LL_CONN);

    UINT16_TO_STREAM (pp, scan_int);
    UINT16_TO_STREAM (pp, scan_win);
    UINT8_TO_STREAM (pp, init_filter_policy);

    UINT8_TO_STREAM (pp, addr_type_peer);
    BDADDR_TO_STREAM (pp, bda_peer);
    UINT8_TO_STREAM (pp, addr_type_own);

    UINT16_TO_STREAM (pp, conn_int_min);
    UINT16_TO_STREAM (pp, conn_int_max);
    UINT16_TO_STREAM (pp, conn_latency);
    UINT16_TO_STREAM (pp, conn_timeout);

    UINT16_TO_STREAM (pp, min_ce_len);
    UINT16_TO_STREAM (pp, max_ce_len);

    btu_hcif_send_cmd (LOCAL_BR_EDR_CONTROLLER_ID,  p);
    return (TRUE);
}

BOOLEAN btsnd_hcic_ble_ext_create_ll_conn (UINT8 ini_phy, UINT16 scan_int, UINT16 scan_win,
                                       UINT8 init_filter_policy,
                                       UINT8 addr_type_peer, BD_ADDR bda_peer,
                                       UINT8 addr_type_own,
                                       UINT16 conn_int_min, UINT16 conn_int_max,
                                       UINT16 conn_latency, UINT16 conn_timeout,
                                       UINT16 min_ce_len, UINT16 max_ce_len)
{
    BT_HDR *p = (BT_HDR *)osi_malloc(HCI_CMD_BUF_SIZE);
    UINT8 *pp = (UINT8 *)(p + 1);

    p->len    = HCIC_PREAMBLE_SIZE + HCIC_PARAM_SIZE_BLE_EXT_CREATE_LL_CONN;
    p->offset = 0;

    UINT16_TO_STREAM (pp, HCI_BLE_EXT_CREATE_LL_CONN);
    if(ini_phy == HCIC_SCAN_PHY_LE_1M || ini_phy == HCIC_SCAN_PHY_LE_CODED)
    {
        p->len    = HCIC_PREAMBLE_SIZE + HCIC_PARAM_SIZE_BLE_EXT_CREATE_LL_CONN;
        UINT8_TO_STREAM  (pp, HCIC_PARAM_SIZE_BLE_EXT_CREATE_LL_CONN);
    }
    else if(ini_phy == HCIC_SCAN_PHY_LE_1M_CODED)
    {
        p->len    = HCIC_PREAMBLE_SIZE + HCIC_PARAM_SIZE_BLE_EXT_CREATE_LL_CONN + 16;
        UINT8_TO_STREAM  (pp, HCIC_PARAM_SIZE_BLE_EXT_CREATE_LL_CONN + 16);
    }

    UINT8_TO_STREAM (pp, init_filter_policy);
    UINT8_TO_STREAM (pp, addr_type_own);
    UINT8_TO_STREAM (pp, addr_type_peer);
    BDADDR_TO_STREAM (pp, bda_peer);
    UINT8_TO_STREAM (pp, ini_phy);

    UINT16_TO_STREAM (pp, scan_int);
    UINT16_TO_STREAM (pp, scan_win);
    UINT16_TO_STREAM (pp, conn_int_min);
    UINT16_TO_STREAM (pp, conn_int_max);
    UINT16_TO_STREAM (pp, conn_latency);
    UINT16_TO_STREAM (pp, conn_timeout);
    UINT16_TO_STREAM (pp, min_ce_len);
    UINT16_TO_STREAM (pp, max_ce_len);

    if(ini_phy == HCIC_SCAN_PHY_LE_1M_CODED)
    {
        UINT16_TO_STREAM (pp, scan_int);
        UINT16_TO_STREAM (pp, scan_win);
        UINT16_TO_STREAM (pp, conn_int_min);
        UINT16_TO_STREAM (pp, conn_int_max);
        UINT16_TO_STREAM (pp, conn_latency);
        UINT16_TO_STREAM (pp, conn_timeout);
        UINT16_TO_STREAM (pp, min_ce_len);
        UINT16_TO_STREAM (pp, max_ce_len);
    }

    btu_hcif_send_cmd (LOCAL_BR_EDR_CONTROLLER_ID,  p);
    return (TRUE);
}

BOOLEAN btsnd_hcic_ble_create_conn_cancel (void)
{
    BT_HDR *p = (BT_HDR *)osi_malloc(HCI_CMD_BUF_SIZE);
    UINT8 *pp = (UINT8 *)(p + 1);

    p->len    = HCIC_PREAMBLE_SIZE + HCIC_PARAM_SIZE_BLE_CREATE_CONN_CANCEL;
    p->offset = 0;

    UINT16_TO_STREAM (pp, HCI_BLE_CREATE_CONN_CANCEL);
    UINT8_TO_STREAM  (pp, HCIC_PARAM_SIZE_BLE_CREATE_CONN_CANCEL);

    btu_hcif_send_cmd (LOCAL_BR_EDR_CONTROLLER_ID,  p);
    return (TRUE);
}

BOOLEAN btsnd_hcic_ble_clear_white_list (void)
{
    BT_HDR *p = (BT_HDR *)osi_malloc(HCI_CMD_BUF_SIZE);
    UINT8 *pp = (UINT8 *)(p + 1);

    p->len    = HCIC_PREAMBLE_SIZE + HCIC_PARAM_SIZE_CLEAR_WHITE_LIST;
    p->offset = 0;

    UINT16_TO_STREAM (pp, HCI_BLE_CLEAR_WHITE_LIST);
    UINT8_TO_STREAM  (pp, HCIC_PARAM_SIZE_CLEAR_WHITE_LIST);

    btu_hcif_send_cmd (LOCAL_BR_EDR_CONTROLLER_ID,  p);
    return (TRUE);
}

BOOLEAN btsnd_hcic_ble_add_white_list (UINT8 addr_type, BD_ADDR bda)
{
    BT_HDR *p = (BT_HDR *)osi_malloc(HCI_CMD_BUF_SIZE);
    UINT8 *pp = (UINT8 *)(p + 1);

    p->len    = HCIC_PREAMBLE_SIZE + HCIC_PARAM_SIZE_ADD_WHITE_LIST;
    p->offset = 0;

    UINT16_TO_STREAM (pp, HCI_BLE_ADD_WHITE_LIST);
    UINT8_TO_STREAM  (pp, HCIC_PARAM_SIZE_ADD_WHITE_LIST);

    UINT8_TO_STREAM (pp, addr_type);
    BDADDR_TO_STREAM (pp, bda);

    btu_hcif_send_cmd (LOCAL_BR_EDR_CONTROLLER_ID,  p);
    return (TRUE);
}

BOOLEAN btsnd_hcic_ble_remove_from_white_list (UINT8 addr_type, BD_ADDR bda)
{
    BT_HDR *p = (BT_HDR *)osi_malloc(HCI_CMD_BUF_SIZE);
    UINT8 *pp = (UINT8 *)(p + 1);

    p->len    = HCIC_PREAMBLE_SIZE + HCIC_PARAM_SIZE_REMOVE_WHITE_LIST;
    p->offset = 0;

    UINT16_TO_STREAM (pp, HCI_BLE_REMOVE_WHITE_LIST);
    UINT8_TO_STREAM  (pp, HCIC_PARAM_SIZE_REMOVE_WHITE_LIST);

    UINT8_TO_STREAM (pp, addr_type);
    BDADDR_TO_STREAM (pp, bda);

    btu_hcif_send_cmd (LOCAL_BR_EDR_CONTROLLER_ID,  p);
    return (TRUE);
}

BOOLEAN btsnd_hcic_ble_upd_ll_conn_params (UINT16 handle,
                                           UINT16 conn_int_min, UINT16 conn_int_max,
                                           UINT16 conn_latency, UINT16 conn_timeout,
                                           UINT16 min_ce_len, UINT16 max_ce_len)
{
    BT_HDR *p = (BT_HDR *)osi_malloc(HCI_CMD_BUF_SIZE);
    UINT8 *pp = (UINT8 *)(p + 1);

    p->len    = HCIC_PREAMBLE_SIZE + HCIC_PARAM_SIZE_BLE_UPD_LL_CONN_PARAMS;
    p->offset = 0;

    UINT16_TO_STREAM (pp, HCI_BLE_UPD_LL_CONN_PARAMS);
    UINT8_TO_STREAM  (pp, HCIC_PARAM_SIZE_BLE_UPD_LL_CONN_PARAMS);

    UINT16_TO_STREAM (pp, handle);

    UINT16_TO_STREAM (pp, conn_int_min);
    UINT16_TO_STREAM (pp, conn_int_max);
    UINT16_TO_STREAM (pp, conn_latency);
    UINT16_TO_STREAM (pp, conn_timeout);
    UINT16_TO_STREAM (pp, min_ce_len);
    UINT16_TO_STREAM (pp, max_ce_len);

    btu_hcif_send_cmd (LOCAL_BR_EDR_CONTROLLER_ID,  p);
    return (TRUE);
}

BOOLEAN btsnd_hcic_ble_set_host_chnl_class (UINT8  chnl_map[HCIC_BLE_CHNL_MAP_SIZE])
{
    BT_HDR *p = (BT_HDR *)osi_malloc(HCI_CMD_BUF_SIZE);
    UINT8 *pp = (UINT8 *)(p + 1);

    p->len    = HCIC_PREAMBLE_SIZE + HCIC_PARAM_SIZE_SET_HOST_CHNL_CLASS;
    p->offset = 0;

    UINT16_TO_STREAM (pp, HCI_BLE_SET_HOST_CHNL_CLASS);
    UINT8_TO_STREAM  (pp, HCIC_PARAM_SIZE_SET_HOST_CHNL_CLASS);

    ARRAY_TO_STREAM (pp, chnl_map, HCIC_BLE_CHNL_MAP_SIZE);

    btu_hcif_send_cmd (LOCAL_BR_EDR_CONTROLLER_ID,  p);
    return (TRUE);
}

BOOLEAN btsnd_hcic_ble_read_chnl_map (UINT16 handle)
{
    BT_HDR *p = (BT_HDR *)osi_malloc(HCI_CMD_BUF_SIZE);
    UINT8 *pp = (UINT8 *)(p + 1);

    p->len    = HCIC_PREAMBLE_SIZE + HCIC_PARAM_SIZE_READ_CHNL_MAP;
    p->offset = 0;

    UINT16_TO_STREAM (pp, HCI_BLE_READ_CHNL_MAP);
    UINT8_TO_STREAM  (pp, HCIC_PARAM_SIZE_READ_CHNL_MAP);

    UINT16_TO_STREAM (pp, handle);

    btu_hcif_send_cmd (LOCAL_BR_EDR_CONTROLLER_ID,  p);
    return (TRUE);
}

BOOLEAN btsnd_hcic_ble_read_remote_feat (UINT16 handle)
{
    BT_HDR *p = (BT_HDR *)osi_malloc(HCI_CMD_BUF_SIZE);
    UINT8 *pp = (UINT8 *)(p + 1);

    p->len    = HCIC_PREAMBLE_SIZE + HCIC_PARAM_SIZE_BLE_READ_REMOTE_FEAT;
    p->offset = 0;

    UINT16_TO_STREAM (pp, HCI_BLE_READ_REMOTE_FEAT);
    UINT8_TO_STREAM  (pp, HCIC_PARAM_SIZE_BLE_READ_REMOTE_FEAT);

    UINT16_TO_STREAM (pp, handle);

    btu_hcif_send_cmd (LOCAL_BR_EDR_CONTROLLER_ID,  p);
    return (TRUE);
}

/* security management commands */
BOOLEAN btsnd_hcic_ble_encrypt (UINT8 *key, UINT8 key_len,
                                UINT8 *plain_text, UINT8 pt_len,
                                void *p_cmd_cplt_cback)
{
    BT_HDR *p = (BT_HDR *)osi_malloc(HCI_CMD_BUF_SIZE);
    UINT8 *pp = (UINT8 *)(p + 1);

    p->len    = HCIC_PREAMBLE_SIZE + HCIC_PARAM_SIZE_BLE_ENCRYPT;
    p->offset = sizeof(void *);

    *((void **)pp) = p_cmd_cplt_cback;  /* Store command complete callback in buffer */
    pp += sizeof(void *);               /* Skip over callback pointer */


    UINT16_TO_STREAM (pp, HCI_BLE_ENCRYPT);
    UINT8_TO_STREAM  (pp, HCIC_PARAM_SIZE_BLE_ENCRYPT);

    memset(pp, 0, HCIC_PARAM_SIZE_BLE_ENCRYPT);

    if (key_len > HCIC_BLE_ENCRYT_KEY_SIZE) key_len = HCIC_BLE_ENCRYT_KEY_SIZE;
    if (pt_len > HCIC_BLE_ENCRYT_KEY_SIZE) pt_len = HCIC_BLE_ENCRYT_KEY_SIZE;

    ARRAY_TO_STREAM (pp, key, key_len);
    pp += (HCIC_BLE_ENCRYT_KEY_SIZE - key_len);
    ARRAY_TO_STREAM (pp, plain_text, pt_len);

    btu_hcif_send_cmd (LOCAL_BR_EDR_CONTROLLER_ID,  p);
    return (TRUE);
}

BOOLEAN btsnd_hcic_ble_rand (void *p_cmd_cplt_cback)
{
    BT_HDR *p = (BT_HDR *)osi_malloc(HCI_CMD_BUF_SIZE);
    UINT8 *pp = (UINT8 *)(p + 1);

    p->len    = HCIC_PREAMBLE_SIZE + HCIC_PARAM_SIZE_BLE_RAND;
    p->offset = sizeof(void *);

    *((void **)pp) = p_cmd_cplt_cback;  /* Store command complete callback in buffer */
    pp += sizeof(void *);               /* Skip over callback pointer */

    UINT16_TO_STREAM (pp, HCI_BLE_RAND);
    UINT8_TO_STREAM  (pp, HCIC_PARAM_SIZE_BLE_RAND);

    btu_hcif_send_cmd (LOCAL_BR_EDR_CONTROLLER_ID,  p);
    return (TRUE);
}

BOOLEAN btsnd_hcic_ble_start_enc (UINT16 handle, UINT8 rand[HCIC_BLE_RAND_DI_SIZE],
                                UINT16 ediv, UINT8 ltk[HCIC_BLE_ENCRYT_KEY_SIZE])
{
    BT_HDR *p = (BT_HDR *)osi_malloc(HCI_CMD_BUF_SIZE);
    UINT8 *pp = (UINT8 *)(p + 1);

    p->len    = HCIC_PREAMBLE_SIZE + HCIC_PARAM_SIZE_BLE_START_ENC;
    p->offset = 0;

    UINT16_TO_STREAM (pp, HCI_BLE_START_ENC);
    UINT8_TO_STREAM  (pp, HCIC_PARAM_SIZE_BLE_START_ENC);

    UINT16_TO_STREAM (pp, handle);
    ARRAY_TO_STREAM (pp, rand, HCIC_BLE_RAND_DI_SIZE);
    UINT16_TO_STREAM (pp, ediv);
    ARRAY_TO_STREAM (pp, ltk, HCIC_BLE_ENCRYT_KEY_SIZE);

    btu_hcif_send_cmd (LOCAL_BR_EDR_CONTROLLER_ID,  p);
    return (TRUE);
}

BOOLEAN btsnd_hcic_ble_ltk_req_reply (UINT16 handle, UINT8 ltk[HCIC_BLE_ENCRYT_KEY_SIZE])
{
    BT_HDR *p = (BT_HDR *)osi_malloc(HCI_CMD_BUF_SIZE);
    UINT8 *pp = (UINT8 *)(p + 1);

    p->len    = HCIC_PREAMBLE_SIZE + HCIC_PARAM_SIZE_LTK_REQ_REPLY;
    p->offset = 0;

    UINT16_TO_STREAM (pp, HCI_BLE_LTK_REQ_REPLY);
    UINT8_TO_STREAM  (pp, HCIC_PARAM_SIZE_LTK_REQ_REPLY);

    UINT16_TO_STREAM (pp, handle);
    ARRAY_TO_STREAM (pp, ltk, HCIC_BLE_ENCRYT_KEY_SIZE);

    btu_hcif_send_cmd (LOCAL_BR_EDR_CONTROLLER_ID,  p);
    return (TRUE);
}

BOOLEAN btsnd_hcic_ble_ltk_req_neg_reply (UINT16 handle)
{
    BT_HDR *p = (BT_HDR *)osi_malloc(HCI_CMD_BUF_SIZE);
    UINT8 *pp = (UINT8 *)(p + 1);

    p->len    = HCIC_PREAMBLE_SIZE + HCIC_PARAM_SIZE_LTK_REQ_NEG_REPLY;
    p->offset = 0;

    UINT16_TO_STREAM (pp, HCI_BLE_LTK_REQ_NEG_REPLY);
    UINT8_TO_STREAM  (pp, HCIC_PARAM_SIZE_LTK_REQ_NEG_REPLY);

    UINT16_TO_STREAM (pp, handle);

    btu_hcif_send_cmd (LOCAL_BR_EDR_CONTROLLER_ID,  p);
    return (TRUE);
}

BOOLEAN btsnd_hcic_ble_receiver_test(UINT8 rx_freq)
{
    BT_HDR *p = (BT_HDR *)osi_malloc(HCI_CMD_BUF_SIZE);
    UINT8 *pp = (UINT8 *)(p + 1);

    p->len    = HCIC_PREAMBLE_SIZE + HCIC_PARAM_SIZE_WRITE_PARAM1;
    p->offset = 0;

    UINT16_TO_STREAM (pp, HCI_BLE_RECEIVER_TEST);
    UINT8_TO_STREAM  (pp, HCIC_PARAM_SIZE_WRITE_PARAM1);

    UINT8_TO_STREAM (pp, rx_freq);

    btu_hcif_send_cmd (LOCAL_BR_EDR_CONTROLLER_ID,  p);
    return (TRUE);
}

BOOLEAN btsnd_hcic_ble_transmitter_test(UINT8 tx_freq, UINT8 test_data_len, UINT8 payload)
{
    BT_HDR *p = (BT_HDR *)osi_malloc(HCI_CMD_BUF_SIZE);
    UINT8 *pp = (UINT8 *)(p + 1);

    p->len    = HCIC_PREAMBLE_SIZE + HCIC_PARAM_SIZE_WRITE_PARAM3;
    p->offset = 0;

    UINT16_TO_STREAM (pp, HCI_BLE_TRANSMITTER_TEST);
    UINT8_TO_STREAM  (pp, HCIC_PARAM_SIZE_WRITE_PARAM3);

    UINT8_TO_STREAM (pp, tx_freq);
    UINT8_TO_STREAM (pp, test_data_len);
    UINT8_TO_STREAM (pp, payload);

    btu_hcif_send_cmd (LOCAL_BR_EDR_CONTROLLER_ID,  p);
    return (TRUE);
}

BOOLEAN btsnd_hcic_ble_test_end(void)
{
    BT_HDR *p = (BT_HDR *)osi_malloc(HCI_CMD_BUF_SIZE);
    UINT8 *pp = (UINT8 *)(p + 1);

    p->len    = HCIC_PREAMBLE_SIZE + HCIC_PARAM_SIZE_READ_CMD;
    p->offset = 0;

    UINT16_TO_STREAM (pp, HCI_BLE_TEST_END);
    UINT8_TO_STREAM  (pp, HCIC_PARAM_SIZE_READ_CMD);

    btu_hcif_send_cmd (LOCAL_BR_EDR_CONTROLLER_ID,  p);
    return (TRUE);
}

BOOLEAN btsnd_hcic_ble_read_host_supported (void)
{
    BT_HDR *p = (BT_HDR *)osi_malloc(HCI_CMD_BUF_SIZE);
    UINT8 *pp = (UINT8 *)(p + 1);

    p->len    = HCIC_PREAMBLE_SIZE + HCIC_PARAM_SIZE_READ_CMD;
    p->offset = 0;

    UINT16_TO_STREAM (pp, HCI_READ_LE_HOST_SUPPORT);
    UINT8_TO_STREAM  (pp, HCIC_PARAM_SIZE_READ_CMD);

    btu_hcif_send_cmd (LOCAL_BR_EDR_CONTROLLER_ID,  p);
    return (TRUE);
}

#if (defined BLE_LLT_INCLUDED) && (BLE_LLT_INCLUDED == TRUE)

BOOLEAN btsnd_hcic_ble_rc_param_req_reply(  UINT16 handle,
                                            UINT16 conn_int_min, UINT16 conn_int_max,
                                            UINT16 conn_latency, UINT16 conn_timeout,
                                            UINT16 min_ce_len, UINT16 max_ce_len  )
{
    BT_HDR *p = (BT_HDR *)osi_malloc(HCI_CMD_BUF_SIZE);
    UINT8 *pp = (UINT8 *)(p + 1);

    p->len    = HCIC_PREAMBLE_SIZE + HCIC_PARAM_SIZE_BLE_RC_PARAM_REQ_REPLY;
    p->offset = 0;

    UINT16_TO_STREAM (pp, HCI_BLE_RC_PARAM_REQ_REPLY);
    UINT8_TO_STREAM  (pp, HCIC_PARAM_SIZE_BLE_RC_PARAM_REQ_REPLY);

    UINT16_TO_STREAM (pp, handle);
    UINT16_TO_STREAM (pp, conn_int_min);
    UINT16_TO_STREAM (pp, conn_int_max);
    UINT16_TO_STREAM (pp, conn_latency);
    UINT16_TO_STREAM (pp, conn_timeout);
    UINT16_TO_STREAM (pp, min_ce_len);
    UINT16_TO_STREAM (pp, max_ce_len);

    btu_hcif_send_cmd (LOCAL_BR_EDR_CONTROLLER_ID,  p);
    return (TRUE);
}

BOOLEAN btsnd_hcic_ble_rc_param_req_neg_reply(UINT16 handle, UINT8 reason)
{
    BT_HDR *p = (BT_HDR *)osi_malloc(HCI_CMD_BUF_SIZE);
    UINT8 *pp = (UINT8 *)(p + 1);

    p->len    = HCIC_PREAMBLE_SIZE + HCIC_PARAM_SIZE_BLE_RC_PARAM_REQ_NEG_REPLY;
    p->offset = 0;

    UINT16_TO_STREAM (pp, HCI_BLE_RC_PARAM_REQ_NEG_REPLY);
    UINT8_TO_STREAM  (pp, HCIC_PARAM_SIZE_BLE_RC_PARAM_REQ_NEG_REPLY);

    UINT16_TO_STREAM (pp, handle);
    UINT8_TO_STREAM (pp, reason);

    btu_hcif_send_cmd (LOCAL_BR_EDR_CONTROLLER_ID,  p);
    return (TRUE);
}
#endif

BOOLEAN btsnd_hcic_ble_add_device_resolving_list (UINT8 addr_type_peer, BD_ADDR bda_peer,
    UINT8 irk_peer[HCIC_BLE_IRK_SIZE],
    UINT8 irk_local[HCIC_BLE_IRK_SIZE])
{
    BT_HDR *p = (BT_HDR *)osi_malloc(HCI_CMD_BUF_SIZE);
    UINT8 *pp = (UINT8 *)(p + 1);

    p->len = HCIC_PREAMBLE_SIZE + HCIC_PARAM_SIZE_BLE_ADD_DEV_RESOLVING_LIST;
    p->offset = 0;

    UINT16_TO_STREAM (pp, HCI_BLE_ADD_DEV_RESOLVING_LIST);
    UINT8_TO_STREAM (pp, HCIC_PARAM_SIZE_BLE_ADD_DEV_RESOLVING_LIST);
    UINT8_TO_STREAM (pp, addr_type_peer);
    BDADDR_TO_STREAM (pp, bda_peer);
    ARRAY_TO_STREAM (pp, irk_peer, HCIC_BLE_ENCRYT_KEY_SIZE);
    ARRAY_TO_STREAM (pp, irk_local, HCIC_BLE_ENCRYT_KEY_SIZE);

    btu_hcif_send_cmd (LOCAL_BR_EDR_CONTROLLER_ID, p);

    return (TRUE);
}

BOOLEAN btsnd_hcic_ble_rm_device_resolving_list (UINT8 addr_type_peer, BD_ADDR bda_peer)
{
    BT_HDR *p = (BT_HDR *)osi_malloc(HCI_CMD_BUF_SIZE);
    UINT8 *pp = (UINT8 *)(p + 1);

    p->len = HCIC_PREAMBLE_SIZE + HCIC_PARAM_SIZE_BLE_RM_DEV_RESOLVING_LIST;
    p->offset = 0;

    UINT16_TO_STREAM (pp, HCI_BLE_RM_DEV_RESOLVING_LIST);
    UINT8_TO_STREAM (pp, HCIC_PARAM_SIZE_BLE_RM_DEV_RESOLVING_LIST);
    UINT8_TO_STREAM (pp, addr_type_peer);
    BDADDR_TO_STREAM (pp, bda_peer);

    btu_hcif_send_cmd (LOCAL_BR_EDR_CONTROLLER_ID, p);

    return (TRUE);
}

BOOLEAN btsnd_hcic_ble_set_privacy_mode (UINT8 addr_type_peer, BD_ADDR bda_peer, UINT8 privacy_type)
{
    BT_HDR *p = (BT_HDR *)osi_malloc(HCI_CMD_BUF_SIZE);
    UINT8 *pp = (UINT8 *)(p + 1);

    p->len = HCIC_PREAMBLE_SIZE + HCIC_PARAM_SIZE_BLE_SET_PRIVACY_MODE;
    p->offset = 0;

    UINT16_TO_STREAM (pp, HCI_BLE_SET_PRIVACY_MODE);
    UINT8_TO_STREAM (pp, HCIC_PARAM_SIZE_BLE_SET_PRIVACY_MODE);
    UINT8_TO_STREAM (pp, addr_type_peer);
    BDADDR_TO_STREAM (pp, bda_peer);
    UINT8_TO_STREAM (pp, privacy_type);

    btu_hcif_send_cmd (LOCAL_BR_EDR_CONTROLLER_ID, p);

    return (TRUE);
}


BOOLEAN btsnd_hcic_ble_clear_resolving_list (void)
{
    BT_HDR *p = (BT_HDR *)osi_malloc(HCI_CMD_BUF_SIZE);
    UINT8 *pp = (UINT8 *)(p + 1);

    p->len = HCIC_PREAMBLE_SIZE + HCIC_PARAM_SIZE_BLE_CLEAR_RESOLVING_LIST;
    p->offset = 0;

    UINT16_TO_STREAM (pp, HCI_BLE_CLEAR_RESOLVING_LIST);
    UINT8_TO_STREAM (pp, HCIC_PARAM_SIZE_BLE_CLEAR_RESOLVING_LIST);

    btu_hcif_send_cmd (LOCAL_BR_EDR_CONTROLLER_ID, p);

    return (TRUE);
}

BOOLEAN btsnd_hcic_ble_read_resolvable_addr_peer (UINT8 addr_type_peer, BD_ADDR bda_peer)
{
    BT_HDR *p = (BT_HDR *)osi_malloc(HCI_CMD_BUF_SIZE);
    UINT8 *pp = (UINT8 *)(p + 1);

    p->len = HCIC_PREAMBLE_SIZE + HCIC_PARAM_SIZE_BLE_READ_RESOLVABLE_ADDR_PEER;
    p->offset = 0;

    UINT16_TO_STREAM (pp, HCI_BLE_READ_RESOLVABLE_ADDR_PEER);
    UINT8_TO_STREAM (pp, HCIC_PARAM_SIZE_BLE_READ_RESOLVABLE_ADDR_PEER);
    UINT8_TO_STREAM (pp, addr_type_peer);
    BDADDR_TO_STREAM (pp, bda_peer);

    btu_hcif_send_cmd (LOCAL_BR_EDR_CONTROLLER_ID, p);

    return (TRUE);
}

BOOLEAN btsnd_hcic_ble_read_resolvable_addr_local (UINT8 addr_type_peer, BD_ADDR bda_peer)
{
    BT_HDR *p = (BT_HDR *)osi_malloc(HCI_CMD_BUF_SIZE);
    UINT8 *pp = (UINT8 *)(p + 1);

    p->len = HCIC_PREAMBLE_SIZE + HCIC_PARAM_SIZE_BLE_READ_RESOLVABLE_ADDR_LOCAL;
    p->offset = 0;

    UINT16_TO_STREAM (pp, HCI_BLE_READ_RESOLVABLE_ADDR_LOCAL);
    UINT8_TO_STREAM (pp, HCIC_PARAM_SIZE_BLE_READ_RESOLVABLE_ADDR_LOCAL);
    UINT8_TO_STREAM (pp, addr_type_peer);
    BDADDR_TO_STREAM (pp, bda_peer);

    btu_hcif_send_cmd (LOCAL_BR_EDR_CONTROLLER_ID, p);

    return (TRUE);
}

BOOLEAN btsnd_hcic_ble_set_addr_resolution_enable (UINT8 addr_resolution_enable)
{
    BT_HDR *p = (BT_HDR *)osi_malloc(HCI_CMD_BUF_SIZE);
    UINT8 *pp = (UINT8 *)(p + 1);

    p->len = HCIC_PREAMBLE_SIZE + HCIC_PARAM_SIZE_BLE_SET_ADDR_RESOLUTION_ENABLE;
    p->offset = 0;

    UINT16_TO_STREAM (pp, HCI_BLE_SET_ADDR_RESOLUTION_ENABLE);
    UINT8_TO_STREAM (pp, HCIC_PARAM_SIZE_BLE_SET_ADDR_RESOLUTION_ENABLE);
    UINT8_TO_STREAM (pp, addr_resolution_enable);

    btu_hcif_send_cmd (LOCAL_BR_EDR_CONTROLLER_ID, p);

    return (TRUE);
}

BOOLEAN btsnd_hcic_ble_set_rand_priv_addr_timeout (UINT16 rpa_timout)
{
    BT_HDR *p = (BT_HDR *)osi_malloc(HCI_CMD_BUF_SIZE);
    UINT8 *pp = (UINT8 *)(p + 1);

    p->len = HCIC_PREAMBLE_SIZE + HCIC_PARAM_SIZE_BLE_SET_RAND_PRIV_ADDR_TIMOUT;
    p->offset = 0;

    UINT16_TO_STREAM (pp, HCI_BLE_SET_RAND_PRIV_ADDR_TIMOUT);
    UINT8_TO_STREAM (pp, HCIC_PARAM_SIZE_BLE_SET_RAND_PRIV_ADDR_TIMOUT);
    UINT16_TO_STREAM (pp, rpa_timout);

    btu_hcif_send_cmd (LOCAL_BR_EDR_CONTROLLER_ID, p);

    return (TRUE);
}

BOOLEAN btsnd_hcic_ble_set_data_length(UINT16 conn_handle, UINT16 tx_octets, UINT16 tx_time)
{
    BT_HDR *p = (BT_HDR *)osi_malloc(HCI_CMD_BUF_SIZE);
    UINT8 *pp = (UINT8 *)(p + 1);

    p->len = HCIC_PREAMBLE_SIZE + HCIC_PARAM_SIZE_BLE_SET_DATA_LENGTH;
    p->offset = 0;

    UINT16_TO_STREAM(pp, HCI_BLE_SET_DATA_LENGTH);
    UINT8_TO_STREAM(pp, HCIC_PARAM_SIZE_BLE_SET_DATA_LENGTH);

    UINT16_TO_STREAM(pp, conn_handle);
    UINT16_TO_STREAM(pp, tx_octets);
    UINT16_TO_STREAM(pp, tx_time);

    btu_hcif_send_cmd (LOCAL_BR_EDR_CONTROLLER_ID, p);
    return TRUE;
}

BOOLEAN btsnd_hcic_ble_set_default_data_rate(UINT8 all_phy, UINT8 tx_phy, UINT8 rx_phy)
{
    BT_HDR *p = (BT_HDR *)osi_malloc(HCI_CMD_BUF_SIZE);
    UINT8 *pp = (UINT8 *)(p + 1);;

    p->len = HCIC_PREAMBLE_SIZE + HCIC_PARAM_SIZE_BLE_WRITE_DEFAULT_PHY;
    p->offset = 0;

    UINT16_TO_STREAM(pp, HCI_BLE_SET_DEFAULT_PHY_RATE);
    UINT8_TO_STREAM(pp, HCIC_PARAM_SIZE_BLE_WRITE_DEFAULT_PHY);

    UINT8_TO_STREAM(pp, all_phy);
    UINT8_TO_STREAM(pp, tx_phy);
    UINT8_TO_STREAM(pp, rx_phy);

    btu_hcif_send_cmd (LOCAL_BR_EDR_CONTROLLER_ID, p);
    return TRUE;
}

BOOLEAN btsnd_hcic_ble_set_data_rate(UINT16 handle, UINT8 all_phy, UINT8 tx_phy,
                                     UINT8 rx_phy, UINT16 phy_options)
{
    BT_HDR *p = (BT_HDR *)osi_malloc(HCI_CMD_BUF_SIZE);
    UINT8 *pp = (UINT8 *)(p + 1);;

    p->len = HCIC_PREAMBLE_SIZE + HCIC_PARAM_SIZE_BLE_WRITE_PHY;
    p->offset = 0;

    UINT16_TO_STREAM(pp, HCI_BLE_SET_PHY_RATE);
    UINT8_TO_STREAM(pp, HCIC_PARAM_SIZE_BLE_WRITE_PHY);

    UINT16_TO_STREAM(pp, handle);
    UINT8_TO_STREAM(pp, all_phy);
    UINT8_TO_STREAM(pp, tx_phy);
    UINT8_TO_STREAM(pp, rx_phy);
    UINT16_TO_STREAM(pp, phy_options);

    btu_hcif_send_cmd (LOCAL_BR_EDR_CONTROLLER_ID, p);
    return TRUE;
}

BOOLEAN btsnd_hcic_ble_set_extended_adv_params (UINT8 set_id, UINT16 event_properties,
                                              UINT32 adv_int_min, UINT32 adv_int_max,
                                              UINT8 channel_map, UINT8 addr_type_own,
                                              UINT8 addr_type_dir, BD_ADDR direct_bda,
                                              UINT8 adv_filter_policy, UINT8 adv_tx_power,
                                              UINT8 primary_adv_phy, UINT8 secondary_max_skip,
                                              UINT8 secondary_adv_phy, UINT8 advertising_sid,
                                              UINT8 scan_req_not_enb)
{
    BT_HDR *p = (BT_HDR *)osi_malloc(HCI_CMD_BUF_SIZE);
    UINT8 *pp = (UINT8 *)(p + 1);;

    p->len    = HCIC_PREAMBLE_SIZE + HCIC_PARAM_SIZE_BLE_WRITE_EXTENDED_ADV_PARAMS ;
    p->offset = 0;

    UINT16_TO_STREAM (pp, HCI_BLE_WRITE_EXTENDED_ADV_PARAMS);
    UINT8_TO_STREAM  (pp, HCIC_PARAM_SIZE_BLE_WRITE_EXTENDED_ADV_PARAMS);
    UINT8_TO_STREAM (pp, set_id);
    UINT16_TO_STREAM (pp, event_properties);
    UINT24_TO_STREAM (pp, adv_int_min);
    UINT24_TO_STREAM (pp, adv_int_max);
    UINT8_TO_STREAM (pp, channel_map);
    UINT8_TO_STREAM (pp, addr_type_own);
    UINT8_TO_STREAM (pp, addr_type_dir);
    BDADDR_TO_STREAM (pp, direct_bda);
    UINT8_TO_STREAM (pp, adv_filter_policy);
    UINT8_TO_STREAM (pp, adv_tx_power);
    UINT8_TO_STREAM (pp, primary_adv_phy);
    UINT8_TO_STREAM (pp, secondary_max_skip);
    UINT8_TO_STREAM (pp, secondary_adv_phy);
    UINT8_TO_STREAM (pp, advertising_sid);
    UINT8_TO_STREAM (pp, scan_req_not_enb);

    btu_hcif_send_cmd (LOCAL_BR_EDR_CONTROLLER_ID,  p);
    return (TRUE);
}

BOOLEAN btsnd_hcic_ble_set_extended_adv_data (UINT8 set_id, UINT8 operation, UINT8 fragment_pref,
                                              UINT8 data_len, UINT8 *p_data)
{
    BT_HDR *p = (BT_HDR *)osi_malloc(HCI_CMD_BUF_SIZE);
    UINT8 *pp = (UINT8 *)(p + 1);;

    p->len    = HCIC_PREAMBLE_SIZE + data_len + 4;
    p->offset = 0;

    if (data_len > HCIC_PARAM_SIZE_BLE_WRITE_EXTENDED_ADV_DATA)
        data_len = HCIC_PARAM_SIZE_BLE_WRITE_EXTENDED_ADV_DATA;


    UINT16_TO_STREAM (pp, HCI_BLE_WRITE_EXTENDED_ADV_DATA);
    UINT8_TO_STREAM  (pp, data_len + 4);
    UINT8_TO_STREAM  (pp, set_id);
    UINT8_TO_STREAM  (pp, operation);
    UINT8_TO_STREAM  (pp, fragment_pref);

    memset(pp, 0, data_len + 1);

    if (p_data != NULL && data_len > 0)
    {
        UINT8_TO_STREAM (pp, data_len);
        ARRAY_TO_STREAM (pp, p_data, data_len);
    }

    btu_hcif_send_cmd (LOCAL_BR_EDR_CONTROLLER_ID,  p);
    return (TRUE);
}

BOOLEAN btsnd_hcic_ble_set_extended_scan_rsp_data (UINT8 set_id, UINT8 operation, UINT8 fragment_pref,
                                                   UINT8 data_len, UINT8 *p_scan_rsp)
{
    BT_HDR *p = (BT_HDR *)osi_malloc(HCI_CMD_BUF_SIZE);
    UINT8 *pp = (UINT8 *)(p + 1);;

    p->len    = HCIC_PREAMBLE_SIZE + data_len + 4;
    p->offset = 0;

    if (data_len > HCIC_PARAM_SIZE_BLE_WRITE_EXTENDED_SCAN_RSP )
            data_len = HCIC_PARAM_SIZE_BLE_WRITE_EXTENDED_SCAN_RSP;

    UINT16_TO_STREAM (pp, HCI_BLE_WRITE_EXTENDED_SCAN_RSP_DATA);
    UINT8_TO_STREAM  (pp, data_len + 4);
    UINT8_TO_STREAM  (pp, set_id);
    UINT8_TO_STREAM  (pp, operation);
    UINT8_TO_STREAM  (pp, fragment_pref);

    memset(pp, 0, data_len + 1);

    if (p_scan_rsp != NULL && data_len > 0)
    {
        UINT8_TO_STREAM (pp, data_len);
        ARRAY_TO_STREAM (pp, p_scan_rsp, data_len);
    }

    btu_hcif_send_cmd (LOCAL_BR_EDR_CONTROLLER_ID,  p);
    return (TRUE);
}

BOOLEAN btsnd_hcic_ble_set_extended_adv_enable (UINT8 adv_enable, UINT8 num_sets, UINT8* handles,
                                            UINT16* durations, UINT8* max_adv_events)
{
    BT_HDR *p = (BT_HDR *)osi_malloc(HCI_CMD_BUF_SIZE);
    UINT8 *pp = (UINT8 *)(p + 1);;

    p->len = HCIC_PREAMBLE_SIZE + HCIC_PARAM_SIZE_WRITE_EXTENDED_ADV_ENABLE + num_sets*4;
    p->offset = 0;

    UINT16_TO_STREAM (pp, HCI_BLE_WRITE_EXTENDED_ADV_ENABLE);
    UINT8_TO_STREAM  (pp, HCIC_PARAM_SIZE_WRITE_EXTENDED_ADV_ENABLE + 4*num_sets);

    UINT8_TO_STREAM (pp, adv_enable);
    UINT8_TO_STREAM (pp, num_sets);

    if (num_sets > 0) {
        for (UINT8 count = 0; count < num_sets; count++) {
            UINT8_TO_STREAM(pp, handles[count]);
            UINT16_TO_STREAM (pp, durations[count]);
            UINT8_TO_STREAM(pp, max_adv_events[count]);
        }
    }

    btu_hcif_send_cmd (LOCAL_BR_EDR_CONTROLLER_ID,  p);
    return (TRUE);
}

BOOLEAN btsnd_hcic_ble_read_num_adv_sets (void)
{
    BT_HDR *p = (BT_HDR *)osi_malloc(HCI_CMD_BUF_SIZE);
    UINT8 *pp = (UINT8 *)(p + 1);;

    p->len    = HCIC_PREAMBLE_SIZE + HCIC_PARAM_SIZE_READ_CMD;
    p->offset = 0;

    UINT16_TO_STREAM (pp, HCI_BLE_READ_NUM_ADV_SETS);
    UINT8_TO_STREAM  (pp, HCIC_PARAM_SIZE_READ_CMD);

    btu_hcif_send_cmd (LOCAL_BR_EDR_CONTROLLER_ID,  p);
    return (TRUE);
}

BOOLEAN btsnd_hcic_ble_write_extended_rpa (UINT8 set_id, BD_ADDR rpa)
{
    BT_HDR *p = (BT_HDR *)osi_malloc(HCI_CMD_BUF_SIZE);
    UINT8 *pp = (UINT8 *)(p + 1);;

    p->len    = HCIC_PREAMBLE_SIZE + HCIC_PARAM_SIZE_WRITE_EXTENDED_ADV_RPA;
    p->offset = 0;

    UINT16_TO_STREAM (pp, HCI_BLE_WRITE_EXTENDED_ADV_RPA);
    UINT8_TO_STREAM  (pp, HCIC_PARAM_SIZE_WRITE_EXTENDED_ADV_RPA);
    UINT8_TO_STREAM  (pp, set_id);
    BDADDR_TO_STREAM (pp, rpa);

    btu_hcif_send_cmd (LOCAL_BR_EDR_CONTROLLER_ID,  p);
    return (TRUE);
}

BOOLEAN btsnd_hcic_ble_read_extended_max_adv_len (void)
{
    BT_HDR *p = (BT_HDR *)osi_malloc(HCI_CMD_BUF_SIZE);
    UINT8 *pp = (UINT8 *)(p + 1);;

    p->len    = HCIC_PREAMBLE_SIZE + HCIC_PARAM_SIZE_READ_CMD;
    p->offset = 0;

    UINT16_TO_STREAM (pp, HCI_BLE_READ_MAX_ADV_LENGTH);
    UINT8_TO_STREAM  (pp, HCIC_PARAM_SIZE_READ_CMD);

    btu_hcif_send_cmd (LOCAL_BR_EDR_CONTROLLER_ID,  p);
    return (TRUE);
}

#endif
