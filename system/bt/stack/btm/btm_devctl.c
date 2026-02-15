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
 *  This file contains functions that handle BTM interface functions for the
 *  Bluetooth device including Rest, HCI buffer size and others
 *
 ******************************************************************************/

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stddef.h>

#include "bt_types.h"
#include "bt_utils.h"
#include "btm_int.h"
#include "btu.h"
#include "device/include/controller.h"
#include "hci_layer.h"
#include "hcimsgs.h"
#include "l2c_int.h"
#include "btcore/include/module.h"
#include "osi/include/thread.h"

#if BLE_INCLUDED == TRUE
#include "gatt_int.h"
#endif /* BLE_INCLUDED */

extern fixed_queue_t *btu_general_alarm_queue;
extern thread_t *bt_workqueue_thread;

/********************************************************************************/
/*                 L O C A L    D A T A    D E F I N I T I O N S                */
/********************************************************************************/

#ifndef BTM_DEV_RESET_TIMEOUT
#define BTM_DEV_RESET_TIMEOUT   4
#endif

// TODO: Reevaluate this value in the context of timers with ms granularity
#define BTM_DEV_NAME_REPLY_TIMEOUT_MS (2 * 1000) /* 2 seconds for name reply */

#define BTM_INFO_TIMEOUT        5   /* 5 seconds for info response */

/********************************************************************************/
/*              L O C A L    F U N C T I O N     P R O T O T Y P E S            */
/********************************************************************************/

static void btm_decode_ext_features_page (UINT8 page_number, const BD_FEATURES p_features);

/*******************************************************************************
**
** Function         btm_dev_init
**
** Description      This function is on the BTM startup
**
** Returns          void
**
*******************************************************************************/
void btm_dev_init (void)
{
#if 0  /* cleared in btm_init; put back in if called from anywhere else! */
    memset (&btm_cb.devcb, 0, sizeof (tBTM_DEVCB));
#endif

    /* Initialize nonzero defaults */
#if (BTM_MAX_LOC_BD_NAME_LEN > 0)
    memset(btm_cb.cfg.bd_name, 0, sizeof(tBTM_LOC_BD_NAME));
#endif

    btm_cb.devcb.read_local_name_timer =
        alarm_new("btm.read_local_name_timer");
    btm_cb.devcb.read_rssi_timer = alarm_new("btm.read_rssi_timer");
    btm_cb.devcb.read_link_quality_timer =
        alarm_new("btm.read_link_quality_timer");
    btm_cb.devcb.read_inq_tx_power_timer =
        alarm_new("btm.read_inq_tx_power_timer");
    btm_cb.devcb.qos_setup_timer = alarm_new("btm.qos_setup_timer");
    btm_cb.devcb.read_tx_power_timer = alarm_new("btm.read_tx_power_timer");

    btm_cb.btm_acl_pkt_types_supported = BTM_ACL_PKT_TYPES_MASK_DH1 + BTM_ACL_PKT_TYPES_MASK_DM1 +
                                         BTM_ACL_PKT_TYPES_MASK_DH3 + BTM_ACL_PKT_TYPES_MASK_DM3 +
                                         BTM_ACL_PKT_TYPES_MASK_DH5 + BTM_ACL_PKT_TYPES_MASK_DM5;

    btm_cb.btm_sco_pkt_types_supported = BTM_SCO_PKT_TYPES_MASK_HV1 +
                                         BTM_SCO_PKT_TYPES_MASK_HV2 +
                                         BTM_SCO_PKT_TYPES_MASK_HV3 +
                                         BTM_SCO_PKT_TYPES_MASK_EV3 +
                                         BTM_SCO_PKT_TYPES_MASK_EV4 +
                                         BTM_SCO_PKT_TYPES_MASK_EV5;
}


/*******************************************************************************
**
** Function         btm_db_reset
**
** Description      This function is called by BTM_DeviceReset and clears out any
**                  pending callbacks for inquiries, discoveries, other pending
**                  functions that may be in progress.
**
** Returns          void
**
*******************************************************************************/
static void btm_db_reset (void)
{
    tBTM_CMPL_CB    *p_cb;
    tBTM_STATUS      status = BTM_DEV_RESET;

    btm_inq_db_reset();

    if (btm_cb.devcb.p_rln_cmpl_cb)
    {
        p_cb = btm_cb.devcb.p_rln_cmpl_cb;
        btm_cb.devcb.p_rln_cmpl_cb = NULL;

        if (p_cb)
            (*p_cb)((void *) NULL);
    }

    if (btm_cb.devcb.p_rssi_cmpl_cb)
    {
        p_cb = btm_cb.devcb.p_rssi_cmpl_cb;
        btm_cb.devcb.p_rssi_cmpl_cb = NULL;

        if (p_cb)
            (*p_cb)((tBTM_RSSI_RESULTS *) &status);
    }
}

bool set_sec_state_idle(void *data, void *context)
{
    tBTM_SEC_DEV_REC *p_dev_rec = data;
    p_dev_rec->sec_state = BTM_SEC_STATE_IDLE;
    return true;
}

static void reset_complete(void *result) {
  assert(result == FUTURE_SUCCESS);
  const controller_t *controller = controller_get_interface();

  /* Tell L2CAP that all connections are gone */
  l2cu_device_reset ();

  /* Clear current security state */
  list_foreach(btm_cb.sec_dev_rec, set_sec_state_idle, NULL);

  /* After the reset controller should restore all parameters to defaults. */
  btm_cb.btm_inq_vars.inq_counter       = 1;
  btm_cb.btm_inq_vars.inq_scan_window   = HCI_DEF_INQUIRYSCAN_WINDOW;
  btm_cb.btm_inq_vars.inq_scan_period   = HCI_DEF_INQUIRYSCAN_INTERVAL;
  btm_cb.btm_inq_vars.inq_scan_type     = HCI_DEF_SCAN_TYPE;

  btm_cb.btm_inq_vars.page_scan_window  = HCI_DEF_PAGESCAN_WINDOW;
  btm_cb.btm_inq_vars.page_scan_period  = HCI_DEF_PAGESCAN_INTERVAL;
  btm_cb.btm_inq_vars.page_scan_type    = HCI_DEF_SCAN_TYPE;

#if (BLE_INCLUDED == TRUE)
  btm_cb.ble_ctr_cb.conn_state = BLE_CONN_IDLE;
  btm_cb.ble_ctr_cb.bg_conn_type = BTM_BLE_CONN_NONE;
  btm_cb.ble_ctr_cb.p_select_cback = NULL;
  gatt_reset_bgdev_list();

#if (defined BLE_EXTENDED_ADV_SUPPORT && (BLE_EXTENDED_ADV_SUPPORT == TRUE))
  if (controller->supports_ble() && controller->supports_ble_extended_advertisements() &&
      controller->get_ble_adv_ext_size() > 0) {
    // provide an extra instance to accomodate for the 0th instance
    btm_ble_multi_adv_init(controller->get_ble_adv_ext_size() + 1);
  }
#endif

#endif

  btm_pm_reset();

  l2c_link_processs_num_bufs(controller->get_acl_buffer_count_classic());
#if (BLE_INCLUDED == TRUE)

#if (defined BLE_PRIVACY_SPT && BLE_PRIVACY_SPT == TRUE)
  /* Set up the BLE privacy settings */
  if (controller->supports_ble() && controller->supports_ble_privacy() &&
      controller->get_ble_resolving_list_max_size() > 0) {
      btm_ble_resolving_list_init(controller->get_ble_resolving_list_max_size());
      /* set the default random private address timeout */
      btsnd_hcic_ble_set_rand_priv_addr_timeout(BTM_BLE_PRIVATE_ADDR_INT_MS / 1000);
  }
#endif

  if (controller->supports_ble()) {
    btm_ble_white_list_init(controller->get_ble_white_list_size());
    l2c_link_processs_ble_num_bufs(controller->get_acl_buffer_count_ble());
  }

  if(controller->supports_ble_two_mbps_rate()) {
      /* set 2mbps Tx and Rx if supported*/
      btsnd_hcic_ble_set_default_data_rate(0, BTM_DATA_RATE_TWO|BTM_DATA_RATE_ONE,
                                           BTM_DATA_RATE_TWO|BTM_DATA_RATE_ONE);
  }
#endif

  BTM_SetPinType (btm_cb.cfg.pin_type, btm_cb.cfg.pin_code, btm_cb.cfg.pin_code_len);

  for (int i = 0; i <= controller->get_last_features_classic_index(); i++) {
    btm_decode_ext_features_page(i, controller->get_features_classic(i)->as_array);
  }

  btm_report_device_status(BTM_DEV_STATUS_UP);
}

// TODO(zachoverflow): remove this function
void BTM_DeviceReset (UNUSED_ATTR tBTM_CMPL_CB *p_cb) {
  /* Flush all ACL connections */
  btm_acl_device_down();

  /* Clear the callback, so application would not hang on reset */
  btm_db_reset();

  module_start_up_callbacked_wrapper(
    get_module(CONTROLLER_MODULE),
    bt_workqueue_thread,
    reset_complete
  );
}

void BTM_HCI_Reset(void)
{
  /* Flush all ACL connections */
  btm_acl_device_down();

  /* Clear the callback, so application would not hang on reset */
  btm_db_reset();

  btsnd_hcic_reset(LOCAL_BR_EDR_CONTROLLER_ID);
}

/*******************************************************************************
**
** Function         BTM_IsDeviceUp
**
** Description      This function is called to check if the device is up.
**
** Returns          TRUE if device is up, else FALSE
**
*******************************************************************************/
BOOLEAN BTM_IsDeviceUp (void)
{
    return controller_get_interface()->get_is_ready();
}

/*******************************************************************************
**
** Function         btm_read_local_name_timeout
**
** Description      Callback when reading the local name times out.
**
** Returns          void
**
*******************************************************************************/
void btm_read_local_name_timeout(UNUSED_ATTR void *data)
{
    tBTM_CMPL_CB  *p_cb = btm_cb.devcb.p_rln_cmpl_cb;
    btm_cb.devcb.p_rln_cmpl_cb = NULL;
    if (p_cb)
        (*p_cb)((void *) NULL);
}

/*******************************************************************************
**
** Function         btm_decode_ext_features_page
**
** Description      This function is decodes a features page.
**
** Returns          void
**
*******************************************************************************/
static void btm_decode_ext_features_page (UINT8 page_number, const UINT8 *p_features)
{
    BTM_TRACE_DEBUG ("btm_decode_ext_features_page page: %d", page_number);
    switch (page_number)
    {
    /* Extended (Legacy) Page 0 */
    case HCI_EXT_FEATURES_PAGE_0:

        /* Create ACL supported packet types mask */
        btm_cb.btm_acl_pkt_types_supported = (BTM_ACL_PKT_TYPES_MASK_DH1 +
                                              BTM_ACL_PKT_TYPES_MASK_DM1);

        if (HCI_3_SLOT_PACKETS_SUPPORTED(p_features))
            btm_cb.btm_acl_pkt_types_supported |= (BTM_ACL_PKT_TYPES_MASK_DH3 +
                                                   BTM_ACL_PKT_TYPES_MASK_DM3);

        if (HCI_5_SLOT_PACKETS_SUPPORTED(p_features))
            btm_cb.btm_acl_pkt_types_supported |= (BTM_ACL_PKT_TYPES_MASK_DH5 +
                                                   BTM_ACL_PKT_TYPES_MASK_DM5);

        /* Add in EDR related ACL types */
        if (!HCI_EDR_ACL_2MPS_SUPPORTED(p_features))
        {
            btm_cb.btm_acl_pkt_types_supported |= (BTM_ACL_PKT_TYPES_MASK_NO_2_DH1 +
                                                   BTM_ACL_PKT_TYPES_MASK_NO_2_DH3 +
                                                   BTM_ACL_PKT_TYPES_MASK_NO_2_DH5);
        }

        if (!HCI_EDR_ACL_3MPS_SUPPORTED(p_features))
        {
            btm_cb.btm_acl_pkt_types_supported |= (BTM_ACL_PKT_TYPES_MASK_NO_3_DH1 +
                                                   BTM_ACL_PKT_TYPES_MASK_NO_3_DH3 +
                                                   BTM_ACL_PKT_TYPES_MASK_NO_3_DH5);
        }

        /* Check to see if 3 and 5 slot packets are available */
        if (HCI_EDR_ACL_2MPS_SUPPORTED(p_features) ||
            HCI_EDR_ACL_3MPS_SUPPORTED(p_features))
        {
            if (!HCI_3_SLOT_EDR_ACL_SUPPORTED(p_features))
                btm_cb.btm_acl_pkt_types_supported |= (BTM_ACL_PKT_TYPES_MASK_NO_2_DH3 +
                                                       BTM_ACL_PKT_TYPES_MASK_NO_3_DH3);

            if (!HCI_5_SLOT_EDR_ACL_SUPPORTED(p_features))
                btm_cb.btm_acl_pkt_types_supported |= (BTM_ACL_PKT_TYPES_MASK_NO_2_DH5 +
                                                       BTM_ACL_PKT_TYPES_MASK_NO_3_DH5);
        }

        BTM_TRACE_DEBUG("Local supported ACL packet types: 0x%04x",
                         btm_cb.btm_acl_pkt_types_supported);

        /* Create (e)SCO supported packet types mask */
        btm_cb.btm_sco_pkt_types_supported = 0;
#if BTM_SCO_INCLUDED == TRUE
        btm_cb.sco_cb.esco_supported = FALSE;
#endif
        if (HCI_SCO_LINK_SUPPORTED(p_features))
        {
            btm_cb.btm_sco_pkt_types_supported = BTM_SCO_PKT_TYPES_MASK_HV1;

            if (HCI_HV2_PACKETS_SUPPORTED(p_features))
                btm_cb.btm_sco_pkt_types_supported |= BTM_SCO_PKT_TYPES_MASK_HV2;

            if (HCI_HV3_PACKETS_SUPPORTED(p_features))
                btm_cb.btm_sco_pkt_types_supported |= BTM_SCO_PKT_TYPES_MASK_HV3;
        }

        if (HCI_ESCO_EV3_SUPPORTED(p_features))
            btm_cb.btm_sco_pkt_types_supported |= BTM_SCO_PKT_TYPES_MASK_EV3;

        if (HCI_ESCO_EV4_SUPPORTED(p_features))
            btm_cb.btm_sco_pkt_types_supported |= BTM_SCO_PKT_TYPES_MASK_EV4;

        if (HCI_ESCO_EV5_SUPPORTED(p_features))
            btm_cb.btm_sco_pkt_types_supported |= BTM_SCO_PKT_TYPES_MASK_EV5;
#if BTM_SCO_INCLUDED == TRUE
        if (btm_cb.btm_sco_pkt_types_supported & BTM_ESCO_LINK_ONLY_MASK)
        {
            btm_cb.sco_cb.esco_supported = TRUE;

            /* Add in EDR related eSCO types */
            if (HCI_EDR_ESCO_2MPS_SUPPORTED(p_features))
            {
                if (!HCI_3_SLOT_EDR_ESCO_SUPPORTED(p_features))
                    btm_cb.btm_sco_pkt_types_supported |= BTM_SCO_PKT_TYPES_MASK_NO_2_EV5;
            }
            else
            {
                btm_cb.btm_sco_pkt_types_supported |= (BTM_SCO_PKT_TYPES_MASK_NO_2_EV3 +
                                                       BTM_SCO_PKT_TYPES_MASK_NO_2_EV5);
            }

            if (HCI_EDR_ESCO_3MPS_SUPPORTED(p_features))
            {
                if (!HCI_3_SLOT_EDR_ESCO_SUPPORTED(p_features))
                    btm_cb.btm_sco_pkt_types_supported |= BTM_SCO_PKT_TYPES_MASK_NO_3_EV5;
            }
            else
            {
                btm_cb.btm_sco_pkt_types_supported |= (BTM_SCO_PKT_TYPES_MASK_NO_3_EV3 +
                                                       BTM_SCO_PKT_TYPES_MASK_NO_3_EV5);
            }
        }
#endif

        BTM_TRACE_DEBUG("Local supported SCO packet types: 0x%04x",
                         btm_cb.btm_sco_pkt_types_supported);

        /* Create Default Policy Settings */
        if (HCI_SWITCH_SUPPORTED(p_features))
            btm_cb.btm_def_link_policy |= HCI_ENABLE_MASTER_SLAVE_SWITCH;
        else
            btm_cb.btm_def_link_policy &= ~HCI_ENABLE_MASTER_SLAVE_SWITCH;

        if (HCI_HOLD_MODE_SUPPORTED(p_features))
            btm_cb.btm_def_link_policy |= HCI_ENABLE_HOLD_MODE;
        else
            btm_cb.btm_def_link_policy &= ~HCI_ENABLE_HOLD_MODE;

        if (HCI_SNIFF_MODE_SUPPORTED(p_features))
            btm_cb.btm_def_link_policy |= HCI_ENABLE_SNIFF_MODE;
        else
            btm_cb.btm_def_link_policy &= ~HCI_ENABLE_SNIFF_MODE;

        if (HCI_PARK_MODE_SUPPORTED(p_features))
            btm_cb.btm_def_link_policy |= HCI_ENABLE_PARK_MODE;
        else
            btm_cb.btm_def_link_policy &= ~HCI_ENABLE_PARK_MODE;

        btm_sec_dev_reset ();

        if (HCI_LMP_INQ_RSSI_SUPPORTED(p_features))
        {
            if (HCI_EXT_INQ_RSP_SUPPORTED(p_features))
                BTM_SetInquiryMode (BTM_INQ_RESULT_EXTENDED);
            else
                BTM_SetInquiryMode (BTM_INQ_RESULT_WITH_RSSI);
        }

#if L2CAP_NON_FLUSHABLE_PB_INCLUDED == TRUE
        if( HCI_NON_FLUSHABLE_PB_SUPPORTED(p_features))
            l2cu_set_non_flushable_pbf(TRUE);
        else
            l2cu_set_non_flushable_pbf(FALSE);
#endif
        BTM_SetPageScanType (BTM_DEFAULT_SCAN_TYPE);
        BTM_SetInquiryScanType (BTM_DEFAULT_SCAN_TYPE);

        break;

    /* Extended Page 1 */
    case HCI_EXT_FEATURES_PAGE_1:
        /* Nothing to do for page 1 */
        break;

    /* Extended Page 2 */
    case HCI_EXT_FEATURES_PAGE_2:
        /* Nothing to do for page 2 */
        break;

    /* Extended Page 3 */
    case HCI_EXT_FEATURES_PAGE_3:
        /* Nothing to do for page 3 */
        break;

    default:
        BTM_TRACE_ERROR("btm_decode_ext_features_page page=%d unknown", page_number);
        break;
    }
}

/*******************************************************************************
**
** Function         BTM_SetLocalDeviceName
**
** Description      This function is called to set the local device name.
**
** Returns          status of the operation
**
*******************************************************************************/
tBTM_STATUS BTM_SetLocalDeviceName (char *p_name)
{
    UINT8    *p;

    if (!p_name || !p_name[0] || (strlen ((char *)p_name) > BD_NAME_LEN))
        return (BTM_ILLEGAL_VALUE);

    if (!controller_get_interface()->get_is_ready())
        return (BTM_DEV_RESET);

#if BTM_MAX_LOC_BD_NAME_LEN > 0
    /* Save the device name if local storage is enabled */
    p = (UINT8 *)btm_cb.cfg.bd_name;
    if (p != (UINT8 *)p_name)
        strlcpy(btm_cb.cfg.bd_name, p_name, BTM_MAX_LOC_BD_NAME_LEN);
#else
    p = (UINT8 *)p_name;
#endif

    if (btsnd_hcic_change_name(p))
        return (BTM_CMD_STARTED);
    else
        return (BTM_NO_RESOURCES);
}



/*******************************************************************************
**
** Function         BTM_ReadLocalDeviceName
**
** Description      This function is called to read the local device name.
**
** Returns          status of the operation
**                  If success, BTM_SUCCESS is returned and p_name points stored
**                              local device name
**                  If BTM doesn't store local device name, BTM_NO_RESOURCES is
**                              is returned and p_name is set to NULL
**
*******************************************************************************/
tBTM_STATUS BTM_ReadLocalDeviceName (char **p_name)
{
#if BTM_MAX_LOC_BD_NAME_LEN > 0
    *p_name = btm_cb.cfg.bd_name;
    return(BTM_SUCCESS);
#else
    *p_name = NULL;
    return(BTM_NO_RESOURCES);
#endif
}


/*******************************************************************************
**
** Function         BTM_ReadLocalDeviceNameFromController
**
** Description      Get local device name from controller. Do not use cached
**                  name (used to get chip-id prior to btm reset complete).
**
** Returns          BTM_CMD_STARTED if successful, otherwise an error
**
*******************************************************************************/
tBTM_STATUS BTM_ReadLocalDeviceNameFromController (tBTM_CMPL_CB *p_rln_cmpl_cback)
{
    /* Check if rln already in progress */
    if (btm_cb.devcb.p_rln_cmpl_cb)
        return(BTM_NO_RESOURCES);

    /* Save callback */
    btm_cb.devcb.p_rln_cmpl_cb = p_rln_cmpl_cback;

    btsnd_hcic_read_name();
    alarm_set_on_queue(btm_cb.devcb.read_local_name_timer,
                       BTM_DEV_NAME_REPLY_TIMEOUT_MS,
                       btm_read_local_name_timeout, NULL,
                       btu_general_alarm_queue);

    return BTM_CMD_STARTED;
}

/*******************************************************************************
**
** Function         btm_read_local_name_complete
**
** Description      This function is called when local name read complete.
**                  message is received from the HCI.
**
** Returns          void
**
*******************************************************************************/
void btm_read_local_name_complete (UINT8 *p, UINT16 evt_len)
{
    tBTM_CMPL_CB   *p_cb = btm_cb.devcb.p_rln_cmpl_cb;
    UINT8           status;
    UNUSED(evt_len);

    alarm_cancel(btm_cb.devcb.read_local_name_timer);

    /* If there was a callback address for read local name, call it */
    btm_cb.devcb.p_rln_cmpl_cb = NULL;

    if (p_cb)
    {
        STREAM_TO_UINT8  (status, p);

        if (status == HCI_SUCCESS)
            (*p_cb)(p);
        else
            (*p_cb)(NULL);
    }
}

/*******************************************************************************
**
** Function         BTM_SetDeviceClass
**
** Description      This function is called to set the local device class
**
** Returns          status of the operation
**
*******************************************************************************/
tBTM_STATUS BTM_SetDeviceClass (DEV_CLASS dev_class)
{
    if(!memcmp (btm_cb.devcb.dev_class, dev_class, DEV_CLASS_LEN))
        return(BTM_SUCCESS);

    memcpy (btm_cb.devcb.dev_class, dev_class, DEV_CLASS_LEN);

    if (!controller_get_interface()->get_is_ready())
        return (BTM_DEV_RESET);

    if (!btsnd_hcic_write_dev_class (dev_class))
        return (BTM_NO_RESOURCES);

    return (BTM_SUCCESS);
}


/*******************************************************************************
**
** Function         BTM_ReadDeviceClass
**
** Description      This function is called to read the local device class
**
** Returns          pointer to the device class
**
*******************************************************************************/
UINT8 *BTM_ReadDeviceClass (void)
{
    return ((UINT8 *)btm_cb.devcb.dev_class);
}


/*******************************************************************************
**
** Function         BTM_ReadLocalFeatures
**
** Description      This function is called to read the local features
**
** Returns          pointer to the local features string
**
*******************************************************************************/
// TODO(zachoverflow): get rid of this function
UINT8 *BTM_ReadLocalFeatures (void)
{
    // Discarding const modifier for now, until this function dies
    return (UINT8 *)controller_get_interface()->get_features_classic(0)->as_array;
}

/*******************************************************************************
**
** Function         BTM_RegisterForDeviceStatusNotif
**
** Description      This function is called to register for device status
**                  change notifications.
**
**                  If one registration is already there calling function should
**                  save the pointer to the function that is return and
**                  call it when processing of the event is complete
**
** Returns          status of the operation
**
*******************************************************************************/
tBTM_DEV_STATUS_CB *BTM_RegisterForDeviceStatusNotif (tBTM_DEV_STATUS_CB *p_cb)
{
    tBTM_DEV_STATUS_CB *p_prev = btm_cb.devcb.p_dev_status_cb;

    btm_cb.devcb.p_dev_status_cb = p_cb;
    return (p_prev);
}


/*******************************************************************************
**
** Function         BTM_Hci_Raw_Command
**
** Description      Send  HCI raw command to the controller.
**
** Returns
**      BTM_SUCCESS         Command sent. Does not expect command complete
**                              event. (command cmpl callback param is NULL)
**      BTM_CMD_STARTED     Command sent. Waiting for command cmpl event.
**
**
*******************************************************************************/
tBTM_STATUS BTM_Hci_Raw_Command(UINT16 opcode, UINT8 param_len,
                              UINT8 *p_param_buf, tBTM_RAW_CMPL_CB *p_cb)
{
    void *p_buf;
#if HCI_RAW_CMD_INCLUDED == TRUE
    tBTM_DEVCB  *p_devcb = &btm_cb.devcb;
#endif

    BTM_TRACE_EVENT ("BTM: BTM_Hci_Raw_Command: Opcode: 0x%04X, ParamLen: %i.",
                      opcode, param_len);

    /* Allocate a buffer to hold HCI command plus the callback function */
    p_buf = osi_malloc((UINT16)(sizeof(BT_HDR) + sizeof (tBTM_CMPL_CB *) +
                            param_len + HCIC_PREAMBLE_SIZE));
    if (p_buf != NULL)
    {
        btsnd_hcic_raw_cmd (p_buf, opcode, param_len, p_param_buf, (void *)p_cb);

        /* Return value */
#if HCI_RAW_CMD_INCLUDED == TRUE
        if (p_cb != NULL) {
            if(p_cb != (p_devcb->p_hci_evt_cb)) {
                p_devcb->p_hci_evt_cb = p_cb;
            }
            return BTM_CMD_STARTED;
        }
#else
        if (p_cb != NULL)
            return BTM_CMD_STARTED;
#endif
        else
            return BTM_SUCCESS;
    }
    else
        return BTM_NO_RESOURCES;

}

/*******************************************************************************
**
** Function         BTM_VendorSpecificCommand
**
** Description      Send a vendor specific HCI command to the controller.
**
** Returns
**      BTM_SUCCESS         Command sent. Does not expect command complete
**                              event. (command cmpl callback param is NULL)
**      BTM_CMD_STARTED     Command sent. Waiting for command cmpl event.
**
** Notes
**      Opcode will be OR'd with HCI_GRP_VENDOR_SPECIFIC.
**
*******************************************************************************/
tBTM_STATUS BTM_VendorSpecificCommand(UINT16 opcode, UINT8 param_len,
                                      UINT8 *p_param_buf, tBTM_VSC_CMPL_CB *p_cb)
{
    /* Allocate a buffer to hold HCI command plus the callback function */
    void *p_buf = osi_malloc(sizeof(BT_HDR) + sizeof(tBTM_CMPL_CB *) +
                             param_len + HCIC_PREAMBLE_SIZE);

    BTM_TRACE_EVENT("BTM: %s: Opcode: 0x%04X, ParamLen: %i.", __func__,
                    opcode, param_len);

    /* Send the HCI command (opcode will be OR'd with HCI_GRP_VENDOR_SPECIFIC) */
    btsnd_hcic_vendor_spec_cmd(p_buf, opcode, param_len, p_param_buf, (void *)p_cb);

    /* Return value */
    if (p_cb != NULL)
        return (BTM_CMD_STARTED);
    else
        return (BTM_SUCCESS);
}

#if HCI_RAW_CMD_INCLUDED == TRUE
/*******************************************************************************
**
** Function         btm_hci_event
**
** Description      This function is called when HCI event is received
**                  from the HCI layer.
**
** Returns          void
**
*******************************************************************************/
void btm_hci_event(UINT8 *p, UINT8 event_code, UINT8 param_len)
{
    tBTM_DEVCB     *p_devcb = &btm_cb.devcb;
    tBTM_RAW_CMPL  raw_cplt_params;

    /* If there was a callback address for raw cmd complete, call it */
    if (p_devcb->p_hci_evt_cb)
    {
        /* Pass paramters to the callback function */
        raw_cplt_params.event_code = event_code;   /* Number of bytes in return info */
        raw_cplt_params.param_len = param_len;    /* Number of bytes in return info */
        raw_cplt_params.p_param_buf = p;
        (p_devcb->p_hci_evt_cb) (&raw_cplt_params);  /* Call the cmd complete callback function */
    }
}
#endif

/*******************************************************************************
**
** Function         btm_vsc_complete
**
** Description      This function is called when local HCI Vendor Specific
**                  Command complete message is received from the HCI.
**
** Returns          void
**
*******************************************************************************/
void btm_vsc_complete (UINT8 *p, UINT16 opcode, UINT16 evt_len,
                       tBTM_CMPL_CB *p_vsc_cplt_cback)
{
    tBTM_VSC_CMPL   vcs_cplt_params;

    /* If there was a callback address for vcs complete, call it */
    if (p_vsc_cplt_cback)
    {
        /* Pass paramters to the callback function */
        vcs_cplt_params.opcode = opcode;        /* Number of bytes in return info */
        vcs_cplt_params.param_len = evt_len;    /* Number of bytes in return info */
        vcs_cplt_params.p_param_buf = p;
        (*p_vsc_cplt_cback)(&vcs_cplt_params);  /* Call the VSC complete callback function */
    }
}

/*******************************************************************************
**
** Function         BTM_RegisterForVSEvents
**
** Description      This function is called to register/deregister for vendor
**                  specific HCI events.
**
**                  If is_register=TRUE, then the function will be registered;
**                  if is_register=FALSE, then the function will be deregistered.
**
** Returns          BTM_SUCCESS if successful,
**                  BTM_BUSY if maximum number of callbacks have already been
**                           registered.
**
*******************************************************************************/
tBTM_STATUS BTM_RegisterForVSEvents (tBTM_VS_EVT_CB *p_cb, BOOLEAN is_register)
{
    tBTM_STATUS retval = BTM_SUCCESS;
    UINT8 i, free_idx = BTM_MAX_VSE_CALLBACKS;

    /* See if callback is already registered */
    for (i=0; i<BTM_MAX_VSE_CALLBACKS; i++)
    {
        if (btm_cb.devcb.p_vend_spec_cb[i] == NULL)
        {
            /* Found a free slot. Store index */
            free_idx = i;
        }
        else if (btm_cb.devcb.p_vend_spec_cb[i] == p_cb)
        {
            /* Found callback in lookup table. If deregistering, clear the entry. */
            if (is_register == FALSE)
            {
                btm_cb.devcb.p_vend_spec_cb[i] = NULL;
                BTM_TRACE_EVENT("BTM Deregister For VSEvents is successfully");
            }
            return (BTM_SUCCESS);
        }
    }

    /* Didn't find callback. Add callback to free slot if registering */
    if (is_register)
    {
        if (free_idx < BTM_MAX_VSE_CALLBACKS)
        {
            btm_cb.devcb.p_vend_spec_cb[free_idx] = p_cb;
            BTM_TRACE_EVENT("BTM Register For VSEvents is successfully");
        }
        else
        {
            /* No free entries available */
            BTM_TRACE_ERROR ("BTM_RegisterForVSEvents: too many callbacks registered");

            retval = BTM_NO_RESOURCES;
        }
    }

    return (retval);
}

/*******************************************************************************
**
** Function         btm_vendor_specific_evt
**
** Description      Process event HCI_VENDOR_SPECIFIC_EVT
**
**                  Note: Some controllers do not send command complete, so
**                  the callback and busy flag are cleared here also.
**
** Returns          void
**
*******************************************************************************/
void btm_vendor_specific_evt (UINT8 *p, UINT8 evt_len)
{
    UINT8 i;

    BTM_TRACE_DEBUG ("BTM Event: Vendor Specific event from controller");

    for (i=0; i<BTM_MAX_VSE_CALLBACKS; i++)
    {
        if (btm_cb.devcb.p_vend_spec_cb[i])
            (*btm_cb.devcb.p_vend_spec_cb[i])(evt_len, p);
    }
}


/*******************************************************************************
**
** Function         BTM_WritePageTimeout
**
** Description      Send HCI Write Page Timeout.
**
** Returns
**      BTM_SUCCESS         Command sent.
**      BTM_NO_RESOURCES     If out of resources to send the command.
**
**
*******************************************************************************/
tBTM_STATUS BTM_WritePageTimeout(UINT16 timeout)
{
    BTM_TRACE_EVENT ("BTM: BTM_WritePageTimeout: Timeout: %d.", timeout);

    /* Send the HCI command */
    if (btsnd_hcic_write_page_tout (timeout))
        return (BTM_SUCCESS);
    else
        return (BTM_NO_RESOURCES);
}

/*******************************************************************************
**
** Function         BTM_WriteVoiceSettings
**
** Description      Send HCI Write Voice Settings command.
**                  See hcidefs.h for settings bitmask values.
**
** Returns
**      BTM_SUCCESS         Command sent.
**      BTM_NO_RESOURCES     If out of resources to send the command.
**
**
*******************************************************************************/
tBTM_STATUS BTM_WriteVoiceSettings(UINT16 settings)
{
    BTM_TRACE_EVENT ("BTM: BTM_WriteVoiceSettings: Settings: 0x%04x.", settings);

    /* Send the HCI command */
    if (btsnd_hcic_write_voice_settings ((UINT16)(settings & 0x03ff)))
        return (BTM_SUCCESS);

    return (BTM_NO_RESOURCES);
}

/*******************************************************************************
**
** Function         BTM_EnableTestMode
**
** Description      Send HCI the enable device under test command.
**
**                  Note: Controller can only be taken out of this mode by
**                      resetting the controller.
**
** Returns
**      BTM_SUCCESS         Command sent.
**      BTM_NO_RESOURCES    If out of resources to send the command.
**
**
*******************************************************************************/
tBTM_STATUS BTM_EnableTestMode(void)
{
    UINT8   cond;

    BTM_TRACE_EVENT ("BTM: BTM_EnableTestMode");

    /* set auto accept connection as this is needed during test mode */
    /* Allocate a buffer to hold HCI command */
    cond = HCI_DO_AUTO_ACCEPT_CONNECT;
    if (!btsnd_hcic_set_event_filter(HCI_FILTER_CONNECTION_SETUP,
                                     HCI_FILTER_COND_NEW_DEVICE,
                                     &cond, sizeof(cond)))
    {
        return (BTM_NO_RESOURCES);
    }

    /* put device to connectable mode */
    if (BTM_SetConnectability(BTM_CONNECTABLE, BTM_DEFAULT_CONN_WINDOW,
                              BTM_DEFAULT_CONN_INTERVAL) != BTM_SUCCESS) {
        return BTM_NO_RESOURCES;
    }

    /* put device to discoverable mode */
    if (BTM_SetDiscoverability(BTM_GENERAL_DISCOVERABLE,
                               BTM_DEFAULT_DISC_WINDOW,
                               BTM_DEFAULT_DISC_INTERVAL) != BTM_SUCCESS) {
        return BTM_NO_RESOURCES;
    }

    /* mask off all of event from controller */
    hci_layer_get_interface()->transmit_command(
      hci_packet_factory_get_interface()->make_set_event_mask((const bt_event_mask_t *)("\x00\x00\x00\x00\x00\x00\x00\x00")),
      NULL,
      NULL,
      NULL);

    /* Send the HCI command */
    if (btsnd_hcic_enable_test_mode ())
        return (BTM_SUCCESS);
    else
        return (BTM_NO_RESOURCES);
}

/*******************************************************************************
**
** Function         BTM_DeleteStoredLinkKey
**
** Description      This function is called to delete link key for the specified
**                  device addresses from the NVRAM storage attached to the Bluetooth
**                  controller.
**
** Parameters:      bd_addr      - Addresses of the devices
**                  p_cb         - Call back function to be called to return
**                                 the results
**
*******************************************************************************/
tBTM_STATUS BTM_DeleteStoredLinkKey(BD_ADDR bd_addr, tBTM_CMPL_CB *p_cb)
{
    BD_ADDR local_bd_addr;
    BOOLEAN delete_all_flag = FALSE;

    /* Check if the previous command is completed */
    if (btm_cb.devcb.p_stored_link_key_cmpl_cb)
        return (BTM_BUSY);

    if (!bd_addr)
    {
        /* This is to delete all link keys */
        delete_all_flag = TRUE;

        /* We don't care the BD address. Just pass a non zero pointer */
        bd_addr = local_bd_addr;
    }

    BTM_TRACE_EVENT ("BTM: BTM_DeleteStoredLinkKey: delete_all_flag: %s",
                        delete_all_flag ? "TRUE" : "FALSE");

    /* Send the HCI command */
    btm_cb.devcb.p_stored_link_key_cmpl_cb = p_cb;
    if (!btsnd_hcic_delete_stored_key (bd_addr, delete_all_flag))
    {
        return (BTM_NO_RESOURCES);
    }
    else
        return (BTM_SUCCESS);
}

/*******************************************************************************
**
** Function         btm_delete_stored_link_key_complete
**
** Description      This function is called when the command complete message
**                  is received from the HCI for the delete stored link key command.
**
** Returns          void
**
*******************************************************************************/
void btm_delete_stored_link_key_complete (UINT8 *p)
{
    tBTM_CMPL_CB         *p_cb = btm_cb.devcb.p_stored_link_key_cmpl_cb;
    tBTM_DELETE_STORED_LINK_KEY_COMPLETE  result;

    /* If there was a callback registered for read stored link key, call it */
    btm_cb.devcb.p_stored_link_key_cmpl_cb = NULL;

    if (p_cb)
    {
        /* Set the call back event to indicate command complete */
        result.event = BTM_CB_EVT_DELETE_STORED_LINK_KEYS;

        /* Extract the result fields from the HCI event */
        STREAM_TO_UINT8  (result.status, p);
        STREAM_TO_UINT16 (result.num_keys, p);

        /* Call the call back and pass the result */
        (*p_cb)(&result);
    }
}

/*******************************************************************************
**
** Function         btm_report_device_status
**
** Description      This function is called when there is a change in the device
**                  status. This function will report the new device status to
**                  the application
**
** Returns          void
**
*******************************************************************************/
void btm_report_device_status (tBTM_DEV_STATUS status)
{
    tBTM_DEV_STATUS_CB *p_cb = btm_cb.devcb.p_dev_status_cb;

    /* Call the call back to pass the device status to application */
    if (p_cb)
        (*p_cb)(status);
}


