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

/******************************************************************************
 *
 *  Filename:      bte_main.c
 *
 *  Description:   Contains BTE core stack initialization and shutdown code
 *
 ******************************************************************************/

#define LOG_TAG "bt_main"

#include <assert.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <stdlib.h>
#include <time.h>

#include <hardware/bluetooth.h>

#include "bt_hci_bdroid.h"
#include "bt_utils.h"
#include "bta_api.h"
#include "btcore/include/module.h"
#include "bte.h"
#include "btif_common.h"
#include "btsnoop.h"
#include "btu.h"
#include "bt_common.h"
#include "device/include/interop.h"
#include "hci_layer.h"
#include "osi/include/alarm.h"
#include "osi/include/fixed_queue.h"
#include "osi/include/future.h"
#include "osi/include/hash_functions.h"
#include "osi/include/hash_map.h"
#include "osi/include/log.h"
#include "osi/include/osi.h"
#include "osi/include/thread.h"
#include "stack_config.h"

/*******************************************************************************
**  Constants & Macros
*******************************************************************************/

/* Run-time configuration file for BLE*/
#ifndef BTE_BLE_STACK_CONF_FILE
// TODO(armansito): Find a better way than searching by a hardcoded path.
#if defined(OS_GENERIC)
#define BTE_BLE_STACK_CONF_FILE "ble_stack.conf"
#else  // !defined(OS_GENERIC)
#define BTE_BLE_STACK_CONF_FILE "/data/misc/bluetooth/ble_stack.conf"
#endif  // defined(OS_GENERIC)
#endif  // BT_BLE_STACK_CONF_FILE

/******************************************************************************
**  Variables
******************************************************************************/

/*******************************************************************************
**  Static variables
*******************************************************************************/
static const hci_t *hci;

/*******************************************************************************
**  Static functions
*******************************************************************************/

/*******************************************************************************
**  Externs
*******************************************************************************/
fixed_queue_t *btu_hci_msg_queue;

/******************************************************************************
**
** Function         bte_main_boot_entry
**
** Description      BTE MAIN API - Entry point for BTE chip/stack initialization
**
** Returns          None
**
******************************************************************************/
void bte_main_boot_entry(void)
{
    module_init(get_module(INTEROP_MODULE));

    hci = hci_layer_get_interface();
    if (!hci)
      LOG_ERROR(LOG_TAG, "%s could not get hci layer interface.", __func__);

    btu_hci_msg_queue = fixed_queue_new(SIZE_MAX);
    if (btu_hci_msg_queue == NULL) {
      LOG_ERROR(LOG_TAG, "%s unable to allocate hci message queue.", __func__);
      return;
    }

    data_dispatcher_register_default(hci->event_dispatcher, btu_hci_msg_queue);
    hci->set_data_queue(btu_hci_msg_queue);

    module_init(get_module(STACK_CONFIG_MODULE));
}

/******************************************************************************
**
** Function         bte_main_cleanup
**
** Description      BTE MAIN API - Cleanup code for BTE chip/stack
**
** Returns          None
**
******************************************************************************/
void bte_main_cleanup()
{
    data_dispatcher_register_default(hci_layer_get_interface()->event_dispatcher, NULL);
    hci->set_data_queue(NULL);
    fixed_queue_free(btu_hci_msg_queue, NULL);

    btu_hci_msg_queue = NULL;

    module_clean_up(get_module(STACK_CONFIG_MODULE));

    module_clean_up(get_module(INTEROP_MODULE));
}

/******************************************************************************
**
** Function         bte_main_enable
**
** Description      BTE MAIN API - Creates all the BTE tasks. Should be called
**                  part of the Bluetooth stack enable sequence
**
** Returns          None
**
******************************************************************************/
void bte_main_enable()
{
    APPL_TRACE_DEBUG("%s", __FUNCTION__);

    module_start_up(get_module(BTSNOOP_MODULE));
    module_start_up(get_module(HCI_MODULE));

    BTU_StartUp();
}

/******************************************************************************
**
** Function         bte_main_disable
**
** Description      BTE MAIN API - Destroys all the BTE tasks. Should be called
**                  part of the Bluetooth stack disable sequence
**
** Returns          None
**
******************************************************************************/
void bte_main_disable(void)
{
    APPL_TRACE_DEBUG("%s", __FUNCTION__);

    module_shut_down(get_module(HCI_MODULE));
    module_shut_down(get_module(BTSNOOP_MODULE));

    BTU_ShutDown();
}

/******************************************************************************
**
** Function         bte_main_postload_cfg
**
** Description      BTE MAIN API - Stack postload configuration
**
** Returns          None
**
******************************************************************************/
void bte_main_postload_cfg(void)
{
    hci->do_postload();
}

#if (defined(HCILP_INCLUDED) && HCILP_INCLUDED == TRUE)
/******************************************************************************
**
** Function         bte_main_enable_lpm
**
** Description      BTE MAIN API - Enable/Disable low power mode operation
**
** Returns          None
**
******************************************************************************/
void bte_main_enable_lpm(BOOLEAN enable)
{
    hci->send_low_power_command(enable ? LPM_ENABLE : LPM_DISABLE);
}

/******************************************************************************
**
** Function         bte_main_lpm_allow_bt_device_sleep
**
** Description      BTE MAIN API - Allow the BT controller to go to sleep
**
** Returns          None
**
******************************************************************************/
void bte_main_lpm_allow_bt_device_sleep()
{
    hci->send_low_power_command(LPM_WAKE_DEASSERT);
}

/******************************************************************************
**
** Function         bte_main_lpm_wake_bt_device
**
** Description      BTE MAIN API - Wake BT controller up if it is in sleep mode
**
** Returns          None
**
******************************************************************************/
void bte_main_lpm_wake_bt_device()
{
    hci->send_low_power_command(LPM_WAKE_ASSERT);
}
#endif  // HCILP_INCLUDED

/******************************************************************************
**
** Function         bte_main_hci_send
**
** Description      BTE MAIN API - This function is called by the upper stack to
**                  send an HCI message. The function displays a protocol trace
**                  message (if enabled), and then calls the 'transmit' function
**                  associated with the currently selected HCI transport
**
** Returns          None
**
******************************************************************************/
void bte_main_hci_send (BT_HDR *p_msg, UINT16 event)
{
    UINT16 sub_event = event & BT_SUB_EVT_MASK;  /* local controller ID */

    p_msg->event = event;


    if((sub_event == LOCAL_BR_EDR_CONTROLLER_ID) || \
       (sub_event == LOCAL_BLE_CONTROLLER_ID))
    {
        hci->transmit_downward(event, p_msg);
    }
    else
    {
        APPL_TRACE_ERROR("Invalid Controller ID. Discarding message.");
        osi_free(p_msg);
    }
}
/******************************************************************************
**
** Function         bte_ssr_cleanup
**
** Description      sends PWR_OFF to vendor library so that harware would be
**                  turned off as part of hardware subsystem crash
**
** Returns          None
**
******************************************************************************/
void bte_ssr_cleanup(int reason)
{
    APPL_TRACE_DEBUG("%s", __FUNCTION__);
    if (hci != NULL) {
        hci->ssr_cleanup(reason);
    } else {
        APPL_TRACE_ERROR("%s hci is NULL", __FUNCTION__);
    }
}
