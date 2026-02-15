/*
 *Copyright (c) 2014, The Linux Foundation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the followin conditions are met:
 *        * Redistributions of source code must retain the above copyright
 *          notice, this list of conditions and the followin disclaimer.
 *        * Redistributions in binary form must reproduce the above copyriht
 *            notice, this list of conditions and the followin disclaimer in the
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
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <hardware/bluetooth.h>
#include "port_api.h"
#include "btm_api.h"
#include "btu.h"
#include "btm_api.h"
#if (defined TEST_APP_INTERFACE && TEST_APP_INTERFACE == TRUE)
#include "bt_testapp.h"
#endif
#define LOG_NDDEBUG 0
#define LOG_TAG "bluedroid"
#include "btif_api.h"
#include "bt_utils.h"

/************************************************************************************
**  Constants & Macros
************************************************************************************/
#define RFC_BUFFER_SIZE  20000

/************************************************************************************
**  Externs
************************************************************************************/
static void bt_rfc_mmt_cback (UINT32 code, UINT16 handle); //rfc
static void bt_rfc_mmt_cback_msc_data (UINT32 code, UINT16 handle);
static void bt_rfc_mmt_server_cback (UINT32 code, UINT16 handle);//rfc
static int bt_rfc_data_cback (UINT16 port_handle, void *data, UINT16 len);//rfc
static void bt_rfc_port_cback (UINT32 code, UINT16 handle);//rfc
void rdut_rfcomm (UINT8 server);
#if (defined TEST_APP_INTERFACE && TEST_APP_INTERFACE == TRUE)
void rdut_rfcomm_test_interface (tRFC *input);
#endif
static UINT16 rfc_handle = 0;
char buffer[RFC_BUFFER_SIZE];

/************************************************************************************
**  Functions
************************************************************************************/
#if (defined TEST_APP_INTERFACE && TEST_APP_INTERFACE == TRUE)
static const btrfcomm_interface_t btrfcInterface = {
    sizeof(btrfcomm_interface_t),
    NULL,
    rdut_rfcomm,
    rdut_rfcomm_test_interface,
    NULL,
    NULL,
};

const btrfcomm_interface_t *btif_rfcomm_get_interface(void)
{
    BTIF_TRACE_EVENT("%s", __FUNCTION__);
    ALOGI("\n%s\n", __FUNCTION__);
    return &btrfcInterface;
}
#endif

static void bt_rfc_mmt_cback (UINT32 code, UINT16 handle)
{
    UINT16   length = 0;
    int      count  = 100;//Number of frames to be send

    ALOGI("dut_rfc_mmt_cback %d, %x", code, handle);
    memset(buffer , 0x01 ,10000); //RFC data
    if (code == PORT_SUCCESS)
    {
        PORT_WriteData (handle, buffer, 10000, &length);
        --count;
        ALOGI("rfc mmt length: %d", length);
    }
}

static void bt_rfc_mmt_cback_msc_data (UINT32 code, UINT16 handle)
{
    ALOGI("dut_rfc_mmt_cback_msc_data %d, %x", code, handle);
}

static void bt_rfc_send_data ( UINT16 handle)
{
    UINT16   length = 0;

    ALOGI("bt_rfc_send_data %d", handle);
    memset(buffer , 0x01 ,10000); //RFC data
    PORT_WriteData (handle, buffer, 10000, &length);
}

static void bt_rfc_mmt_server_cback (UINT32 code, UINT16 handle)
{
    ALOGI("dut_rfc_mmt_Server_cback %d, %x", code, handle);
}


static int bt_rfc_data_cback (UINT16 port_handle, void *data, UINT16 len)
{
    ALOGI("dut_rfc_data_cback"); // Called from PORT_DataInd
    return 0;
}

static void bt_rfc_port_cback (UINT32 code, UINT16 handle)
{
    ALOGI("bt_rfc_port_cback");
}

/*static void bdcpy (BD_ADDR a, const BD_ADDR b)
{
    int   i;
    for (i = BD_ADDR_LEN; i != 0; i--)
    {
        *a++ = *b++;
    }
} */



void rdut_rfcomm (UINT8 server)
{
    UINT16   handle;
    int status = -1;
    BD_ADDR remote_bd = {0x00, 0x15, 0x83, 0x0A, 0x0E, 0x1F};
    BD_ADDR any_add   = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

    ALOGI("dut_rfc_mode:%d",server);
    if (server == 0)
    {
        BTM_SetSecurityLevel (TRUE, "", 0, 0, 0x03, 3/*BTM_SEC_PROTO_RFCOMM*/, 20);
        status = RFCOMM_CreateConnection(0x0020, 20, FALSE, 256, (UINT8 *)remote_bd,
                                            &handle, bt_rfc_mmt_cback);
        rfc_handle = handle;
    }
    else if (server == 1)
    {
        BTM_SetSecurityLevel (FALSE, "", 0, 0, 0x03, 3/*BTM_SEC_PROTO_RFCOMM */, 20);
        BTM_SetConnectability (1, 0, 0); //Pae Mode , window , interval
        status = RFCOMM_CreateConnection (0x0020, 20, TRUE, 256, (UINT8 *)any_add,
                                       &handle, bt_rfc_mmt_server_cback);
        rfc_handle = handle;
    }
    else if (server == 3)
    {
        ALOGI("dut RFCOMM RemoveConnection");
        RFCOMM_RemoveConnection(rfc_handle);
    }
    if (status == PORT_SUCCESS)
    {
        ALOGI("dut_setdata_callback");
        PORT_SetDataCallback (handle, bt_rfc_data_cback);
        PORT_SetEventCallback(handle, bt_rfc_port_cback);
    }
}

#if (defined TEST_APP_INTERFACE && TEST_APP_INTERFACE == TRUE)
void rdut_rfcomm_test_interface (tRFC *input)
{
    BD_ADDR   remote_bd;
    UINT16    handle;
    int status = -1;
    BD_ADDR any_add   = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

    switch (input->param)
    {
        case RFC_TEST_CLIENT:
        {
            bdcpy (remote_bd, input->data.conn.bdadd.address);
            BTM_SetSecurityLevel (TRUE, "", 0, 0, 0x03, 3/*BTM_SEC_PROTO_RFCOMM */,
                                                                   input->data.conn.scn);
            status = RFCOMM_CreateConnection(0x0020, input->data.conn.scn, FALSE, 256,
                                                 (UINT8 *)remote_bd,&handle, bt_rfc_mmt_cback);
            rfc_handle = handle;
        }
        break;
        case RFC_TEST_CLIENT_TEST_MSC_DATA:
        {
            bdcpy (remote_bd, input->data.conn.bdadd.address);
            BTM_SetSecurityLevel (TRUE, "", 0, 0, 0x03, 3/*BTM_SEC_PROTO_RFCOMM */,
                                                                input->data.conn.scn);
            status = RFCOMM_CreateConnection(0x0020, input->data.conn.scn, FALSE, 256,
                                       (UINT8 *)remote_bd, &handle, bt_rfc_mmt_cback_msc_data);
            rfc_handle = handle;
        }
        break;
        case RFC_TEST_FRAME_ERROR:
        {
             /* Framing Error */
             PORT_SendError (rfc_handle, 0x08);
        }
        break;
        case RFC_TEST_ROLE_SWITCH:
        {
            /* Role Switch */
            BTM_SwitchRole(input->data.role_switch.bdadd.address, input->data.role_switch.role,
                                                                                         NULL);
        }
        break;
        case RFC_TEST_SERVER:
        {
            BTM_SetSecurityLevel (FALSE, "", 0, 0, 0x03, 3/*BTM_SEC_PROTO_RFCOMM */, 20);
            BTM_SetConnectability (1, 0, 0); //Pae Mode , window , interval
            status = RFCOMM_CreateConnection (0x0020, 20, TRUE, 256, (UINT8 *)any_add,
                                               &handle, bt_rfc_mmt_server_cback);
            rfc_handle = handle;
        }
        break;
        case RFC_TEST_DISCON:
        {
            ALOGI("dut RFCOMM RemoveConnection");
            RFCOMM_RemoveConnection(rfc_handle);
        }
        break;
        case RFC_TEST_WRITE_DATA:
        {
            ALOGI("dut RFC_TEST_WRITE_DATA");
            bt_rfc_send_data(rfc_handle);
        }
        break;
        default :
            ALOGI("dut RFCOMM Unreconised command");
        break;
    }
    if (status == PORT_SUCCESS)
    {
        ALOGI("dut_setdata_callback");
        PORT_SetDataCallback (handle, bt_rfc_data_cback);
        PORT_SetEventCallback(handle, bt_rfc_port_cback);
    }
}
#endif
