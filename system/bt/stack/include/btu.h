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
 *  this file contains the main Bluetooth Upper Layer definitions. The Broadcom
 *  implementations of L2CAP RFCOMM, SDP and the BTIf run as one GKI task. The
 *  btu_task switches between them.
 *
 ******************************************************************************/

#ifndef BTU_H
#define BTU_H

#include "bt_target.h"
#include "bt_common.h"
#include "osi/include/alarm.h"

// HACK(zachoverflow): temporary dark magic
#define BTU_POST_TO_TASK_NO_GOOD_HORRIBLE_HACK 0x1700 // didn't look used in bt_types...here goes nothing
typedef struct {
  void (*callback)(BT_HDR *);
} post_to_task_hack_t;

typedef struct {
  void (*callback)(BT_HDR *);
  BT_HDR *response;
  void *context;
} command_complete_hack_t;

typedef struct {
  void (*callback)(BT_HDR *);
  uint8_t status;
  BT_HDR *command;
  void *context;
} command_status_hack_t;

void bte_ssr_cleanup(int reason);

#ifdef __cplusplus
extern "C" {
#endif

/* Global BTU data */
extern uint8_t btu_trace_level;

extern const BD_ADDR        BT_BD_ANY;

/* Functions provided by btu_task.c
************************************
*/

#if (defined(HCILP_INCLUDED) && HCILP_INCLUDED == TRUE)
extern void btu_check_bt_sleep (void);
#endif

/* Functions provided by btu_hcif.c
************************************
*/
extern void  btu_hcif_process_event (UINT8 controller_id, BT_HDR *p_buf);
extern void  btu_hcif_send_cmd (UINT8 controller_id, BT_HDR *p_msg);

/* Functions provided by btu_core.c
************************************
*/
extern void  btu_init_core(void);
extern void  btu_free_core(void);

void BTU_StartUp(void);
void BTU_ShutDown(void);

#ifdef __cplusplus
}
#endif

#endif
