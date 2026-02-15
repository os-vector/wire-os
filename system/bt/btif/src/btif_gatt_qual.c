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
#include "gatt_api.h"
#include "bta_api.h"

#ifdef TEST_APP_INTERFACE
#include <bt_testapp.h>

UINT16    g_conn_id = 0;

tGATT_IF Gatt_Register (tBT_UUID *p_app_uuid128, tGATT_CBACK *p_cb_info)
{
    tGATT_IF    Gatt_if = 0;
    Gatt_if = GATT_Register (p_app_uuid128, p_cb_info);
    printf("%s:: Gatt_if=%d\n", __FUNCTION__, Gatt_if);
    if (!BTM_SetSecurityLevel (TRUE, "gatt_tool", /*BTM_SEC_SERVICE_SDP_SERVER*/ BTM_SEC_PROTO_L2CAP,
                    0, 0x1f, 0, 0))
        {
        BTIF_TRACE_DEBUG("Error:: BTM_SetSecurityLevel failed");
        return FALSE;
    }
    return Gatt_if;

}
void Gatt_Deregister (tGATT_IF gatt_if)
{
    GATT_Deregister (gatt_if);
    printf("%s:: \n", __FUNCTION__);
}

void Gatt_StartIf(tGATT_IF gatt_if)
{
    GATT_StartIf (gatt_if);
    printf("%s::\n", __FUNCTION__);
}

BOOLEAN Gatt_Connect (tGATT_IF gatt_if, BD_ADDR bd_addr, BOOLEAN is_direct,tBT_TRANSPORT transport)
{
    BOOLEAN     Ret = 0;
    Ret = GATT_Connect(gatt_if, bd_addr, is_direct,BT_TRANSPORT_LE, false);
    printf("%s::Ret=%d,gatt_if=%d, is_direct=%d \n", __FUNCTION__, Ret, gatt_if, is_direct);
    return Ret;
}
tGATT_STATUS Gatt_Disconnect (UINT16 conn_id)
{
    tGATT_STATUS Ret = 0;
    Ret = GATT_Disconnect(conn_id);
    printf("%s::Ret=%d,conn_id=%d\n", __FUNCTION__, Ret, conn_id);
    return Ret;
}

BOOLEAN Gatt_Listen (tGATT_IF gatt_if, BOOLEAN start, BD_ADDR_PTR bd_addr)
{
    BOOLEAN Ret = 0;
    Ret = GATT_Listen(gatt_if, start, bd_addr);
    printf("%s::Ret=%d, gatt_if=%d, start=%d \n", __FUNCTION__, Ret, gatt_if, start);
    return Ret;
}
    //GATT Client APIs
    tGATT_STATUS Gatt_ConfigureMTU (UINT16 conn_id, UINT16  mtu)
    {
        tGATT_STATUS Ret =0;
        Ret = GATTC_ConfigureMTU(conn_id, mtu);
        printf("%s::Ret=%d, conn_id=%d, mtu=%d \n", __FUNCTION__, Ret, conn_id, mtu);
        return Ret;
    }

    tGATT_STATUS Gatt_Discover (UINT16 conn_id, tGATT_DISC_TYPE disc_type, tGATT_DISC_PARAM *p_param )
    {
        tGATT_STATUS Ret = 0;
        Ret = GATTC_Discover(conn_id, disc_type, p_param);
        printf("%s::Ret=%d, conn_id=%d, disc_type=%d \n", __FUNCTION__, Ret, conn_id, disc_type);
        return Ret;
    }

    tGATT_STATUS Gatt_Read (UINT16 conn_id, tGATT_READ_TYPE type, tGATT_READ_PARAM *p_read)
    {
        tGATT_STATUS Ret = 0;
        Ret = GATTC_Read(conn_id, type, p_read);
        printf("%s::Ret=%d, conn_id=%d, type=%d \n", __FUNCTION__, Ret, conn_id, type);
        return Ret;
    }

    tGATT_STATUS Gatt_Write (UINT16 conn_id, tGATT_WRITE_TYPE type, tGATT_VALUE *p_write)
    {
        tGATT_STATUS Ret = 0;
        Ret = GATTC_Write(conn_id, type, p_write);
        printf("%s::Ret=%d, conn_id=%d, type=%d \n", __FUNCTION__, Ret, conn_id, type);
        return Ret;
    }
    tGATT_STATUS Gatt_ExecuteWrite (UINT16 conn_id, BOOLEAN is_execute)
    {
        tGATT_STATUS Ret = 0;
        Ret = GATTC_ExecuteWrite(conn_id, is_execute);
        printf("%s::Ret=%d, conn_id=%d, is_execute=%d \n", __FUNCTION__, Ret, conn_id, is_execute);
        return Ret;
    }
    tGATT_STATUS Gatt_SendHandleValueConfirm (UINT16 conn_id, UINT16 handle)
    {
        tGATT_STATUS Ret = 0;
        Ret = GATTC_SendHandleValueConfirm(conn_id, handle);
        printf("%s::Ret=%d, conn_id=%d, handle=%d \n", __FUNCTION__, Ret, conn_id, handle);
        return Ret;
    }

    void Gatt_SetIdleTimeout (BD_ADDR bd_addr, UINT16 idle_tout)
    {
        GATT_SetIdleTimeout (bd_addr, idle_tout,BT_TRANSPORT_LE);
        printf("%s::\n", __FUNCTION__);
    }

    void Gatt_SetLeAdvMode (tBTA_DM_DISC disc_mode, tBTA_DM_CONN conn_mode)
    {
        printf("%s::call set Visibility\n", __FUNCTION__);
        BTA_DmSetVisibility(disc_mode, conn_mode, BTA_DM_IGNORE, BTA_DM_IGNORE);
    }

static const btgatt_test_interface_t    btgatt_testInterface =
{
    sizeof(btgatt_test_interface_t),
    Gatt_Register,
    Gatt_Deregister,
    Gatt_StartIf,
    Gatt_Connect,
    Gatt_Disconnect,
    Gatt_Listen,
    Gatt_ConfigureMTU,
    Gatt_Discover,
    Gatt_Read,
    Gatt_Write,
    Gatt_ExecuteWrite,
    Gatt_SendHandleValueConfirm,
    Gatt_SetIdleTimeout,
    Gatt_SetLeAdvMode

};


const btgatt_test_interface_t *btif_gatt_test_get_interface(void)
{
    BTIF_TRACE_EVENT("%s", __FUNCTION__);
    printf("\n%s\n", __FUNCTION__);
    return &btgatt_testInterface;
}

#endif     //TEST_APP_INTERFACE
