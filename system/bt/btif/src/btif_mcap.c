/*
 *Copyright (c) 2015, The Linux Foundation. All rights reserved.
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


#define LOG_NDDEBUG 0
#define LOG_TAG "bluedroid"

#include "btif_api.h"
#include "bt_utils.h"
#include "bt_testapp.h"
#include "btm_api.h"
#include "btu.h"
#include "btm_api.h"
#include "mca_api.h"

#ifdef TEST_APP_INTERFACE

static void McaInit(void)
{
    MCA_Init();
}

static tMCA_HANDLE McaRegister(tMCA_REG *p_reg, tMCA_CTRL_CBACK *p_cback)
{
    tMCA_HANDLE Ret = 0;
    BTM_SetConnectability (1, 0, 0);
    Ret = MCA_Register(p_reg, p_cback);
    ALOGI("McaRegister");
    return Ret;
}

static void Mca_Deregister(tMCA_HANDLE handle)
{
    MCA_Deregister(handle);
    ALOGI("McaRegister");
}

static tMCA_RESULT Mca_CreateDep(tMCA_HANDLE handle, tMCA_DEP *p_dep, tMCA_CS *p_cs)
{
    tMCA_RESULT	Ret = 0;
    ALOGI("Mca_CreateDep Enter");
    Ret = MCA_CreateDep(handle, p_dep, p_cs);
    ALOGI("Mca_CreateDep Exit");
    return Ret;
}

static tMCA_RESULT Mca_DeleteDep(tMCA_HANDLE handle, tMCA_DEP dep)
{
    tMCA_RESULT  Ret = 0;
    Ret = MCA_DeleteDep(handle, dep);
    ALOGI("MCA_DeleteDep Exit");
    return Ret;
}


static tMCA_RESULT Mca_ConnectReq(tMCA_HANDLE handle, BD_ADDR bd_addr, UINT16 ctrl_psm, UINT16 sec_mask)
{
    tMCA_RESULT Ret = 0;
    Ret = MCA_ConnectReq(handle, bd_addr, ctrl_psm, sec_mask);
    ALOGI("MCA_ConnectReq");
    return Ret;
}

static tMCA_RESULT Mca_DisconnectReq(tMCA_CL mcl)
{
    tMCA_RESULT Ret = 0;
    Ret = MCA_DisconnectReq(mcl);
    ALOGI("Mca_DisconnectReq");
    return Ret;
}


static tMCA_RESULT Mca_CreateMdl(tMCA_CL mcl, tMCA_DEP dep, UINT16 data_psm,
                                         UINT16 mdl_id, UINT8 peer_dep_id,
                                         UINT8 cfg, const tMCA_CHNL_CFG *p_chnl_cfg)
{
    tMCA_RESULT Ret = 0;
    Ret = MCA_CreateMdl(mcl, dep, data_psm, mdl_id, peer_dep_id, cfg, p_chnl_cfg);
    ALOGI("MCA_CreateMdl");
    return Ret;
}


static tMCA_RESULT Mca_CreateMdlRsp(tMCA_CL mcl, tMCA_DEP dep, UINT16 mdl_id, UINT8 cfg, UINT8 rsp_code, const tMCA_CHNL_CFG *p_chnl_cfg)
{
    tMCA_RESULT Ret = 0;
    Ret = MCA_CreateMdlRsp(mcl, dep, mdl_id, cfg, rsp_code, p_chnl_cfg);
    ALOGI("Mca_CreateMdlRsp");
    return Ret;
}


static tMCA_RESULT Mca_CloseReq(tMCA_DL mdl)
{
    tMCA_RESULT Ret = 0;
    Ret = MCA_CloseReq(mdl);
    ALOGI("Mca_CloseReq");
    return Ret;
}

static tMCA_RESULT Mca_ReconnectMdl(tMCA_CL mcl, tMCA_DEP dep, UINT16 data_psm, UINT16 mdl_id, const tMCA_CHNL_CFG *p_chnl_cfg)
{
    tMCA_RESULT Ret = 0;
    Ret = MCA_ReconnectMdl(mcl, dep, data_psm, mdl_id, p_chnl_cfg);
    ALOGI("Mca_ReconnectMdl");
    return Ret;
}

static tMCA_RESULT Mca_ReconnectMdlRsp(tMCA_CL mcl, tMCA_DEP dep, UINT16 mdl_id, UINT8 rsp_code, const tMCA_CHNL_CFG *p_chnl_cfg)
{
    tMCA_RESULT Ret = 0;
    Ret = MCA_ReconnectMdlRsp(mcl, dep, mdl_id, rsp_code, p_chnl_cfg);
    ALOGI("Mca_ReconnectMdl");
    return Ret;
}

static tMCA_RESULT Mca_DataChnlCfg(tMCA_CL mcl, const tMCA_CHNL_CFG *p_chnl_cfg)
{
    tMCA_RESULT Ret = 0;
    Ret = MCA_DataChnlCfg(mcl, p_chnl_cfg);
    ALOGI("Mca_DataChnlCfg");
    return Ret;
}

static tMCA_RESULT Mca_Abort(tMCA_CL mcl)
{
    tMCA_RESULT Ret = 0;
    Ret = MCA_Abort(mcl);
    ALOGI("MCA_Abort");
    return Ret;
}


static tMCA_RESULT Mca_Delete(tMCA_CL mcl, UINT16 mdl_id)
{
    tMCA_RESULT Ret = 0;
    Ret = MCA_Delete(mcl, mdl_id);
    ALOGI("Mca_Delete");
    return Ret;
}

static tMCA_RESULT Mca_WriteReq(tMCA_DL mdl, BT_HDR *p_pkt)
{
    tMCA_RESULT Ret = 0;
    Ret = MCA_WriteReq(mdl, p_pkt);
    ALOGI("Mca_Delete");
    return Ret;
}

static UINT16 Mca_GetL2CapChannel (tMCA_DL mdl)
{
    UINT16 Ret = 0;
    Ret = MCA_GetL2CapChannel(mdl);
    ALOGI("Mca_GetL2CapChannel");
    return Ret;
}

static const btmcap_interface_t btmcaInterface = {
    sizeof(btmcap_interface_t),
    McaInit,
    McaRegister,
    Mca_Deregister,
    Mca_CreateDep,
    Mca_DeleteDep,
    Mca_ConnectReq,
    Mca_DisconnectReq,
    Mca_CreateMdl,
    Mca_CreateMdlRsp,
    Mca_CloseReq,
    Mca_ReconnectMdl,
    Mca_ReconnectMdlRsp,
    Mca_DataChnlCfg,
    Mca_Abort,
    Mca_Delete,
    Mca_WriteReq,
    Mca_GetL2CapChannel
};


const btmcap_interface_t *btif_mcap_get_interface(void)
{
    //BTIF_TRACE_EVENT1("%s", __FUNCTION__);
    //printf("\n%s\n", __FUNCTION__);
    return &btmcaInterface;
}


#endif
