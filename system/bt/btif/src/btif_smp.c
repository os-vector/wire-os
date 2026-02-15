/*
 * Copyright (c) 2014, The Linux Foundation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 *       copyright notice, this list of conditions and the following
 *       disclaimer in the documentation and/or other materials provided
 *       with the distribution.
 *     * Neither the name of The Linux Foundation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <hardware/bluetooth.h>


#define LOG_NDDEBUG 0
#define LOG_TAG "bluedroid"

#include "btif_api.h"
#include "bt_utils.h"
#include "smp_api.h"

#if (defined TEST_APP_INTERFACE && TEST_APP_INTERFACE == TRUE)
#include <bt_testapp.h>
static void SmpInit(void)
{
    SMP_Init();
}
static BOOLEAN SmpRegister(tSMP_CALLBACK *p_cback)
{
    BOOLEAN Ret = 0;
    Ret = SMP_Register(p_cback);
    printf("%s:: Ret=%d\n", __FUNCTION__, Ret);
    return Ret;

}

static tSMP_STATUS SmpPair(BD_ADDR bd_addr)
{
    tSMP_STATUS Ret = 0;
    Ret = SMP_Pair(bd_addr);
    printf("%s:: Ret=%d\n", __FUNCTION__, Ret);
    return Ret;
}

static BOOLEAN PairCancel(BD_ADDR bd_addr)
{
    BOOLEAN Ret = 0;
    Ret = SMP_PairCancel(bd_addr);
    printf("%s:: Ret=%d\n", __FUNCTION__, Ret);
    return Ret;
}

static void SecurityGrant(BD_ADDR bd_addr, UINT8 res)
{
        SMP_SecurityGrant(bd_addr, res);
    printf("%s:: executed \n", __FUNCTION__);
}

static void PasskeyReply(BD_ADDR bd_addr, UINT8 res, UINT32 passkey)
{
        SMP_PasskeyReply(bd_addr, res, passkey);
    printf("%s:: executed \n", __FUNCTION__);
}

static BOOLEAN Encrypt(UINT8 *key, UINT8 key_len,
                                        UINT8 *plain_text, UINT8 pt_len,
                                        tSMP_ENC *p_out)
{
    BOOLEAN Ret = 0;
    Ret = SMP_Encrypt(key, key_len, plain_text, pt_len, p_out);
    printf("%s:: Ret=%d\n", __FUNCTION__, Ret);
    return Ret;
}

static const btsmp_interface_t btsmpInterface = {
    sizeof(btsmp_interface_t),
    SmpInit,
    SmpRegister,
    SmpPair,
    PairCancel,
    SecurityGrant,
    PasskeyReply,
    Encrypt
};

const btsmp_interface_t *btif_smp_get_interface(void)
{
    BTIF_TRACE_EVENT("%s", __FUNCTION__);
    printf("\n%s\n", __FUNCTION__);
    return &btsmpInterface;
}

#endif   //TEST_APP_INTERFACE
