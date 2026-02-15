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

#define LOG_TAG "bt_btu_task"

#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bt_target.h"
#include "bt_trace.h"
#include "bt_types.h"
#include "bt_utils.h"
#include "btcore/include/module.h"
#include "btif_common.h"
#include "btm_api.h"
#include "btm_int.h"
#include "btu.h"
#include "gap_int.h"
#include "bt_common.h"
#include "hcimsgs.h"
#include "l2c_int.h"
#include "osi/include/alarm.h"
#include "osi/include/fixed_queue.h"
#include "osi/include/future.h"
#include "osi/include/hash_map.h"
#include "osi/include/log.h"
#include "osi/include/osi.h"
#include "osi/include/thread.h"
#include "port_api.h"
#include "port_ext.h"
#include "sdpint.h"

#if (defined(BNEP_INCLUDED) && BNEP_INCLUDED == TRUE)
#include "bnep_int.h"
#endif

#if (defined(PAN_INCLUDED) && PAN_INCLUDED == TRUE)
#include "pan_int.h"
#endif

#if (defined(HID_HOST_INCLUDED) && HID_HOST_INCLUDED == TRUE )
#include "hidh_int.h"
#endif

#if (defined(AVDT_INCLUDED) && AVDT_INCLUDED == TRUE)
#include "avdt_int.h"
#else
extern void avdt_rcv_sync_info (BT_HDR *p_buf); /* this is for hci_test */
#endif

#if (defined(MCA_INCLUDED) && MCA_INCLUDED == TRUE)
#include "mca_api.h"
#include "mca_defs.h"
#include "mca_int.h"
#endif

#include "bta_sys.h"

#if (BLE_INCLUDED == TRUE)
#include "gatt_int.h"
#if (SMP_INCLUDED == TRUE)
#include "smp_int.h"
#endif
#include "btm_ble_int.h"
#endif

extern void BTE_InitStack(void);

/* Define BTU storage area
*/
uint8_t btu_trace_level = HCI_INITIAL_TRACE_LEVEL;

// Communication queue between btu_task and bta.
extern fixed_queue_t *btu_bta_msg_queue;

// Communication queue between btu_task and hci.
extern fixed_queue_t *btu_hci_msg_queue;

// General timer queue.
extern fixed_queue_t *btu_general_alarm_queue;

extern fixed_queue_t *event_queue;
extern fixed_queue_t *btif_msg_queue;

extern thread_t *bt_workqueue_thread;

static void btu_hci_msg_process(BT_HDR *p_msg);

void btu_hci_msg_ready(fixed_queue_t *queue, UNUSED_ATTR void *context) {
    BT_HDR *p_msg = (BT_HDR *)fixed_queue_dequeue(queue);
    btu_hci_msg_process(p_msg);
}

void btu_bta_msg_ready(fixed_queue_t *queue, UNUSED_ATTR void *context) {
    BT_HDR *p_msg = (BT_HDR *)fixed_queue_dequeue(queue);
    bta_sys_event(p_msg);
}

static void btu_hci_msg_process(BT_HDR *p_msg) {
    /* Determine the input message type. */
    switch (p_msg->event & BT_EVT_MASK)
    {
        case BTU_POST_TO_TASK_NO_GOOD_HORRIBLE_HACK: // TODO(zachoverflow): remove this
            ((post_to_task_hack_t *)(&p_msg->data[0]))->callback(p_msg);
#if (defined(HCILP_INCLUDED) && HCILP_INCLUDED == TRUE)
            /* If the host receives events which it doesn't responsd to, */
            /* it should start an idle timer to enter sleep mode.        */
            btu_check_bt_sleep ();
#endif
            break;
        case BT_EVT_TO_BTU_HCI_ACL:
            /* All Acl Data goes to L2CAP */
            l2c_rcv_acl_data (p_msg);
            break;

        case BT_EVT_TO_BTU_L2C_SEG_XMIT:
            /* L2CAP segment transmit complete */
            l2c_link_segments_xmitted (p_msg);
            break;

        case BT_EVT_TO_BTU_HCI_SCO:
#if BTM_SCO_INCLUDED == TRUE
            btm_route_sco_data (p_msg);
            break;
#endif

        case BT_EVT_TO_BTU_HCI_EVT:
            btu_hcif_process_event ((UINT8)(p_msg->event & BT_SUB_EVT_MASK), p_msg);
            osi_free(p_msg);

#if (defined(HCILP_INCLUDED) && HCILP_INCLUDED == TRUE)
            /* If host receives events which it doesn't response to, */
            /* host should start idle timer to enter sleep mode.     */
            btu_check_bt_sleep ();
#endif
            break;

        case BT_EVT_TO_BTU_HCI_CMD:
            btu_hcif_send_cmd ((UINT8)(p_msg->event & BT_SUB_EVT_MASK), p_msg);
            break;

        default:
            osi_free(p_msg);
            break;
    }
}

void btu_task_start_up(UNUSED_ATTR void *context) {
  BT_TRACE(TRACE_LAYER_BTU, TRACE_TYPE_API,
      "btu_task pending for preload complete event");

  LOG_INFO(LOG_TAG, "Bluetooth chip preload is complete");

  BT_TRACE(TRACE_LAYER_BTU, TRACE_TYPE_API,
      "btu_task received preload complete event");

  /* Initialize the mandatory core stack control blocks
     (BTU, BTM, L2CAP, and SDP)
   */
  btu_init_core();

  /* Initialize any optional stack components */
  BTE_InitStack();

  bta_sys_init();

  /* Initialise platform trace levels at this point as BTE_InitStack() and bta_sys_init()
   * reset the control blocks and preset the trace level with XXX_INITIAL_TRACE_LEVEL
   */
#if ( BT_USE_TRACES==TRUE )
  module_init(get_module(BTE_LOGMSG_MODULE));
#endif

  // Inform the bt jni thread initialization is ok.
  btif_transfer_context(btif_init_ok, 0, NULL, 0, NULL);

  fixed_queue_register_dequeue(btu_bta_msg_queue,
      thread_get_reactor(bt_workqueue_thread),
      btu_bta_msg_ready,
      NULL);

  fixed_queue_register_dequeue(btu_hci_msg_queue,
      thread_get_reactor(bt_workqueue_thread),
      btu_hci_msg_ready,
      NULL);

  alarm_register_processing_queue(btu_general_alarm_queue, bt_workqueue_thread);
}

void btu_task_shut_down(UNUSED_ATTR void *context) {
  fixed_queue_unregister_dequeue(btu_bta_msg_queue);
  fixed_queue_unregister_dequeue(btu_hci_msg_queue);
  alarm_unregister_processing_queue(btu_general_alarm_queue);

#if ( BT_USE_TRACES==TRUE )
  module_clean_up(get_module(BTE_LOGMSG_MODULE));
#endif

  bta_sys_free();
  btu_free_core();
}

#if (defined(HCILP_INCLUDED) && HCILP_INCLUDED == TRUE)
/*******************************************************************************
**
** Function         btu_check_bt_sleep
**
** Description      This function is called to check if controller can go to sleep.
**
** Returns          void
**
*******************************************************************************/
void btu_check_bt_sleep (void)
{
    // TODO(zachoverflow) take pending commands into account?
    if (l2cb.controller_xmit_window == l2cb.num_lm_acl_bufs)
    {
        bte_main_lpm_allow_bt_device_sleep();
    }
}
#endif
