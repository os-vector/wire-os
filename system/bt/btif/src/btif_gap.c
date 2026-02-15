/*
 * Copyright (c) 2014, The Linux Foundation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *        * Redistributions of source code must retain the above copyright
 *          notice, this list of conditions and the following disclaimer.
 *        * Redistributions in binary form must reproduce the above copyright
 *            notice, this list of conditions and the following disclaimer in the
 *            documentation and/or other materials provided with the distribution.
 *        * Neither the name of The Linux Foundation nor
 *            the names of its contributors may be used to endorse or promote
 *            products derived from this software without specific prior written
 *            permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NON-INFRINGEMENT ARE DISCLAIMED.    IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <hardware/bluetooth.h>


#define LOG_NDDEBUG 0
#define LOG_TAG "bluedroid"

#include "btif_api.h"
#include "bt_utils.h"
#include "gap_api.h"
#include "l2c_api.h"

#if (defined TEST_APP_INTERFACE && TEST_APP_INTERFACE == TRUE)
#include <bt_testapp.h>



static void GapAttrInit(void)
{
    GAP_Init();
}
static void Gap_BleAttrDBUpdate(BD_ADDR p_bda, UINT16 int_min, UINT16 int_max, UINT16 latency, UINT16 sp_tout)
{
    printf("%s:: remote_bd_addr=%02x:%02x:%02x:%02x:%02x:%02x, int_min=%d, int_max=%d, latency=%d sp_tout=%d\n", __FUNCTION__,
            p_bda[0], p_bda[1], p_bda[2], p_bda[3], p_bda[4], p_bda[5],
            int_min, int_max, latency, sp_tout);
    L2CA_UpdateBleConnParams(p_bda, 50, 70, 0, 1000);

}

static const btgap_interface_t btgapInterface = {
    sizeof(btgap_interface_t),
    GapAttrInit,
    Gap_BleAttrDBUpdate,
};

const btgap_interface_t *btif_gap_get_interface(void)
{
    BTIF_TRACE_EVENT("%s", __FUNCTION__);
    return &btgapInterface;
}

#endif   //TEST_APP_INTERFACE
