/******************************************************************************
 *
 *  Copyright (C) 2000-2012 Broadcom Corporation
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

#define LOG_TAG "bt_task"

#include <assert.h>
#include <pthread.h>
#include <string.h>

#include "bt_target.h"
#include "btm_int.h"
#include "btu.h"
#include "device/include/controller.h"
#include "dyn_mem.h"
#include "l2c_int.h"
#include "osi/include/alarm.h"
#include "osi/include/fixed_queue.h"
#include "osi/include/hash_functions.h"
#include "osi/include/hash_map.h"
#include "osi/include/log.h"
#include "osi/include/thread.h"
#include "sdpint.h"

#if (BLE_INCLUDED == TRUE)
#include "gatt_api.h"
#include "gatt_int.h"
#if SMP_INCLUDED == TRUE
#include "smp_int.h"
#endif
#endif

// Increase BTU task thread priority to avoid pre-emption
// of audio realated tasks.
#define BTU_TASK_THREAD_PRIORITY -19

extern fixed_queue_t *btif_msg_queue;

// Communication queue from bta thread to bt_workqueue.
fixed_queue_t *btu_bta_msg_queue;

// Communication queue from hci thread to bt_workqueue.
extern fixed_queue_t *btu_hci_msg_queue;

// General timer queue.
fixed_queue_t *btu_general_alarm_queue;

thread_t *bt_workqueue_thread;
static const char *BT_WORKQUEUE_NAME = "bt_workqueue";

extern void PLATFORM_DisableHciTransport(UINT8 bDisable);
/*****************************************************************************
**                          V A R I A B L E S                                *
******************************************************************************/
// TODO(cmanton) Move this out of this file
const BD_ADDR   BT_BD_ANY = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

void btu_task_start_up(void *context);
void btu_task_shut_down(void *context);

/*****************************************************************************
**
** Function         btu_init_core
**
** Description      Initialize control block memory for each core component.
**
**
** Returns          void
**
******************************************************************************/
void btu_init_core(void)
{
    LOG_INFO(LOG_TAG, "%s ", __func__);
    /* Initialize the mandatory core stack components */
    btm_init();

    l2c_init();

    sdp_init();

#if BLE_INCLUDED == TRUE
    gatt_init();
#if (defined(SMP_INCLUDED) && SMP_INCLUDED == TRUE)
    SMP_Init();
#endif
    btm_ble_init();
#endif
}

/*****************************************************************************
**
** Function         btu_free_core
**
** Description      Releases control block memory for each core component.
**
**
** Returns          void
**
******************************************************************************/
void btu_free_core(void)
{
    LOG_INFO(LOG_TAG, "%s ", __func__);
    btm_free();
    /* Free the mandatory core stack components */
    l2c_free();

#if BLE_INCLUDED == TRUE
    gatt_free();
    btm_ble_free();
#endif
}

/*****************************************************************************
**
** Function         BTU_StartUp
**
** Description      Initializes the BTU control block.
**
**                  NOTE: Must be called before creating any tasks
**                      (RPC, BTU, HCIT, APPL, etc.)
**
** Returns          void
**
******************************************************************************/
void BTU_StartUp(void)
{
    btu_trace_level = HCI_INITIAL_TRACE_LEVEL;

    btu_bta_msg_queue = fixed_queue_new(SIZE_MAX);
    if (btu_bta_msg_queue == NULL)
        goto error_exit;

    btu_general_alarm_queue = fixed_queue_new(SIZE_MAX);
    if (btu_general_alarm_queue == NULL)
        goto error_exit;

    bt_workqueue_thread = thread_new(BT_WORKQUEUE_NAME);
    if (bt_workqueue_thread == NULL)
        goto error_exit;

    thread_set_priority(bt_workqueue_thread, BTU_TASK_THREAD_PRIORITY);

    // Continue startup on bt workqueue thread.
    thread_post(bt_workqueue_thread, btu_task_start_up, NULL);
    return;

  error_exit:;
    LOG_ERROR(LOG_TAG, "%s Unable to allocate resources for bt_workqueue", __func__);
    BTU_ShutDown();
}

void BTU_ShutDown(void) {
  btu_task_shut_down(NULL);

  fixed_queue_free(btu_bta_msg_queue, NULL);
  btu_bta_msg_queue = NULL;

  fixed_queue_free(btu_general_alarm_queue, NULL);
  btu_general_alarm_queue = NULL;

  thread_free(bt_workqueue_thread);

  bt_workqueue_thread = NULL;
}
