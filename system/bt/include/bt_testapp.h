/*
 * Copyright (c) 2013, The Linux Foundation. All rights reserved.
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

#ifdef TEST_APP_INTERFACE
#ifndef ANDROID_INCLUDE_BT_TESTAPP_H
#define ANDROID_INCLUDE_BT_TESTAPP_H
#include <stdio.h>
#include <dlfcn.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <ctype.h>
#include <fcntl.h>
#include <sys/prctl.h>
#include <linux/capability.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>
#include <private/android_filesystem_config.h>
#include <android/log.h>
#include <hardware/bluetooth.h>
#include "l2c_api.h"
#include "sdp_api.h"
#include "gatt_api.h"
#include "gap_api.h"
#include "mca_api.h"
#include <hardware/hardware.h>
#include "btm_api.h"

__BEGIN_DECLS

typedef void (tREMOTE_DEVICE_NAME_CB) (void *p1);

enum {
    SUCCESS,
    FAIL
};

typedef enum {
    DUMMY,
    ALL,
    SPP,
    FTP,
    OPP,
    MAP,
    PBAP,
    DUN,
    NOT_SUPPORTED,
}profileName;
typedef enum {
    TEST_APP_L2CAP,
    TEST_APP_RFCOMM,
    TEST_APP_MCAP,
    TEST_APP_GATT,
    TEST_APP_GAP,
    TEST_APP_SMP
} test_app_profile;
typedef struct {

    /** set to sizeof(Btl2capInterface) */
    size_t          size;
    /** Register the L2cap callbacks  */
    bt_status_t (*Init)(tL2CAP_APPL_INFO* callbacks);
    bt_status_t (*RegisterPsm)(UINT16 psm, BOOLEAN conn_type, UINT16 sec_level);
    bt_status_t (*Deregister)(UINT16 psm);
    UINT16      (*AllocatePsm)(void);
    UINT16      (*Connect)(UINT16 psm, bt_bdaddr_t *bd_addr);
    BOOLEAN     (*ConnectRsp)(BD_ADDR p_bd_addr, UINT8 id, UINT16 lcid, UINT16 result, UINT16 status);
    UINT16      (*ErtmConnectReq)(UINT16 psm, BD_ADDR p_bd_addr, tL2CAP_ERTM_INFO *p_ertm_info);
    BOOLEAN     (*ErtmConnectRsp)(BD_ADDR p_bd_addr, UINT8 id, UINT16 lcid,
                                             UINT16 result, UINT16 status,
                                             tL2CAP_ERTM_INFO *p_ertm_info);
    BOOLEAN     (*ConfigReq)(UINT16 cid, tL2CAP_CFG_INFO *p_cfg);
    BOOLEAN     (*ConfigRsp)(UINT16 cid, tL2CAP_CFG_INFO *p_cfg);
    BOOLEAN     (*DisconnectReq)(UINT16 cid);
    BOOLEAN     (*DisconnectRsp)(UINT16 cid);
    UINT8       (*DataWrite)(UINT16 cid, char *p_data, UINT32 len);
    BOOLEAN     (*Ping)(BD_ADDR p_bd_addr, tL2CA_ECHO_RSP_CB *p_cb);
    BOOLEAN     (*Echo)(BD_ADDR p_bd_addr, BT_HDR *p_data, tL2CA_ECHO_DATA_CB *p_callback);
    BOOLEAN     (*SetIdleTimeout)(UINT16 cid, UINT16 timeout, BOOLEAN is_global);
    BOOLEAN     (*SetIdleTimeoutByBdAddr)(BD_ADDR bd_addr, UINT16 timeout);
    UINT8       (*SetDesireRole)(UINT8 new_role);
	void        (*SetSecConnOnlyMode)(BOOLEAN secvalue);
    UINT16      (*LocalLoopbackReq)(UINT16 psm, UINT16 handle, BD_ADDR p_bd_addr);
    UINT16      (*FlushChannel)(UINT16 lcid, UINT16 num_to_flush);
    BOOLEAN     (*SetAclPriority)(BD_ADDR bd_addr, UINT8 priority);
    BOOLEAN     (*FlowControl)(UINT16 cid, BOOLEAN data_enabled);
    BOOLEAN     (*SendTestSFrame)(UINT16 cid, BOOLEAN rr_or_rej, UINT8 back_track);
    BOOLEAN     (*SetTxPriority)(UINT16 cid, tL2CAP_CHNL_PRIORITY priority);
    BOOLEAN     (*RegForNoCPEvt)(tL2CA_NOCP_CB *p_cb, BD_ADDR p_bda);
    BOOLEAN     (*SetChnlDataRate)(UINT16 cid, tL2CAP_CHNL_DATA_RATE tx, tL2CAP_CHNL_DATA_RATE rx);
    BOOLEAN     (*SetFlushTimeout)(BD_ADDR bd_addr, UINT16 flush_tout);
    UINT8       (*DataWriteEx)(UINT16 cid, BT_HDR *p_data, UINT16 flags);
    BOOLEAN     (*SetChnlFlushability)(UINT16 cid, BOOLEAN is_flushable);
    BOOLEAN     (*GetPeerFeatures)(BD_ADDR bd_addr, UINT32 *p_ext_feat, UINT8 *p_chnl_mask);
    BOOLEAN     (*GetBDAddrbyHandle)(UINT16 handle, BD_ADDR bd_addr);
    UINT8       (*GetChnlFcrMode)(UINT16 lcid);
    UINT16      (*SendFixedChnlData)(UINT16 fixed_cid, BD_ADDR rem_bda, BT_HDR *p_buf);
    void  (*Cleanup)(void);
    bt_status_t (*RegisterLePsm) (UINT16 le_psm, BOOLEAN ConnType, UINT16 SecLevel,
                                    UINT8 enc_key_size);
    bt_status_t (*LeDeregister)(UINT16 psm);
    UINT16 (*LeConnect) (UINT16 le_psm , BD_ADDR address, tL2CAP_LE_CFG_INFO *p_cfg);
    BOOLEAN (*LeConnectRsp) (BD_ADDR p_bd_addr, UINT8 id, UINT16 lcid, UINT16 result,
                             UINT16 status, tL2CAP_LE_CFG_INFO *p_cfg);
    BOOLEAN (*LeFlowControl) (UINT16 lcid, UINT16 credits);
    void (*LeFreeBuf)(BT_HDR *p_buf);
} btl2cap_interface_t;

typedef struct
{
    size_t    size;
    void (*Init)(void);
    tMCA_HANDLE (*Register)(tMCA_REG *p_reg, tMCA_CTRL_CBACK *p_cback);
    void        (*Deregister)(tMCA_HANDLE handle);
    tMCA_RESULT (*CreateDep)(tMCA_HANDLE handle, tMCA_DEP *p_dep, tMCA_CS *p_cs);
    tMCA_RESULT (*DeleteDep)(tMCA_HANDLE handle, tMCA_DEP dep);
    tMCA_RESULT (*ConnectReq)(tMCA_HANDLE handle, BD_ADDR bd_addr,
                                          UINT16 ctrl_psm,
                                          UINT16 sec_mask);
    tMCA_RESULT (*DisconnectReq)(tMCA_CL mcl);
    tMCA_RESULT (*CreateMdl)(tMCA_CL mcl, tMCA_DEP dep, UINT16 data_psm,
                                         UINT16 mdl_id, UINT8 peer_dep_id,
                                         UINT8 cfg, const tMCA_CHNL_CFG *p_chnl_cfg);
    tMCA_RESULT (*CreateMdlRsp)(tMCA_CL mcl, tMCA_DEP dep,
                                            UINT16 mdl_id, UINT8 cfg, UINT8 rsp_code,
                                            const tMCA_CHNL_CFG *p_chnl_cfg);
    tMCA_RESULT (*CloseReq)(tMCA_DL mdl);
    tMCA_RESULT (*ReconnectMdl)(tMCA_CL mcl, tMCA_DEP dep, UINT16 data_psm,
                                            UINT16 mdl_id, const tMCA_CHNL_CFG *p_chnl_cfg);
    tMCA_RESULT (*ReconnectMdlRsp)(tMCA_CL mcl, tMCA_DEP dep,
                                               UINT16 mdl_id, UINT8 rsp_code,
                                               const tMCA_CHNL_CFG *p_chnl_cfg);
    tMCA_RESULT (*DataChnlCfg)(tMCA_CL mcl, const tMCA_CHNL_CFG *p_chnl_cfg);
    tMCA_RESULT (*Abort)(tMCA_CL mcl);
    tMCA_RESULT (*Delete)(tMCA_CL mcl, UINT16 mdl_id);
    tMCA_RESULT (*WriteReq)(tMCA_DL mdl, BT_HDR *p_pkt);
    UINT16 (*GetL2CapChannel) (tMCA_DL mdl);
}btmcap_interface_t;

typedef struct
{
    size_t    size;
    //GATT common APIs (Both client and server)
    tGATT_IF (*Register) (tBT_UUID *p_app_uuid128, tGATT_CBACK *p_cb_info);
    void (*Deregister) (tGATT_IF gatt_if);
    void (*StartIf) (tGATT_IF gatt_if);
    BOOLEAN (*Connect) (tGATT_IF gatt_if, BD_ADDR bd_addr, BOOLEAN is_direct,tBT_TRANSPORT transport);
    tGATT_STATUS (*Disconnect) (UINT16 conn_id);
    BOOLEAN (*Listen) (tGATT_IF gatt_if, BOOLEAN start, BD_ADDR_PTR bd_addr);

    //GATT Client APIs
    tGATT_STATUS (*cConfigureMTU) (UINT16 conn_id, UINT16  mtu);
    tGATT_STATUS (*cDiscover) (UINT16 conn_id, tGATT_DISC_TYPE disc_type, tGATT_DISC_PARAM *p_param );
    tGATT_STATUS (*cRead) (UINT16 conn_id, tGATT_READ_TYPE type, tGATT_READ_PARAM *p_read);
    tGATT_STATUS (*cWrite) (UINT16 conn_id, tGATT_WRITE_TYPE type, tGATT_VALUE *p_write);
    tGATT_STATUS (*cExecuteWrite) (UINT16 conn_id, BOOLEAN is_execute);
    tGATT_STATUS (*cSendHandleValueConfirm) (UINT16 conn_id, UINT16 handle);
    void (*cSetIdleTimeout)(BD_ADDR bd_addr, UINT16 idle_tout);
    void (*cSetVisibility) (UINT16 disc_mode, UINT16 conn_mode);

    //GATT Server APIs
    //TODO - Add api on the need basis

}btgatt_test_interface_t;

typedef struct
{
    size_t    size;
    void (*init)(void);
    BOOLEAN (*Register) (tSMP_CALLBACK *p_cback);
    tSMP_STATUS (*Pair) (BD_ADDR bd_addr);
    BOOLEAN (*PairCancel) (BD_ADDR bd_addr);
    void (*SecurityGrant)(BD_ADDR bd_addr, UINT8 res);
    void (*PasskeyReply) (BD_ADDR bd_addr, UINT8 res, UINT32 passkey);
    BOOLEAN (*Encrypt) (UINT8 *key, UINT8 key_len, UINT8 *plain_text, UINT8 pt_len, tSMP_ENC *p_out);
}btsmp_interface_t;
typedef struct
{
    size_t    size;
    void (*Gap_AttrInit)();
    void (*Gap_BleAttrDBUpdate)(BD_ADDR bd_addr, UINT16 int_min, UINT16 int_max, UINT16 latency, UINT16 sp_tout);
}btgap_interface_t;

/** Bluetooth RFC tool commands */
typedef enum {
    RFC_TEST_CLIENT =1,
    RFC_TEST_FRAME_ERROR,
    RFC_TEST_ROLE_SWITCH,
    RFC_TEST_SERVER,
    RFC_TEST_DISCON,
    RFC_TEST_CLIENT_TEST_MSC_DATA, //For PTS test case BV 21 and 22
    RFC_TEST_WRITE_DATA
}rfc_test_cmd_t;


typedef struct {
    bt_bdaddr_t bdadd;
    uint8_t     scn; //Server Channel Number
}bt_rfc_conn_t;

typedef struct {
    bt_bdaddr_t bdadd;
    uint8_t     role; //0x01 for master
}bt_role_sw;

typedef union {
    bt_rfc_conn_t  conn;
    uint8_t        server;
    bt_role_sw     role_switch;
}tRfcomm_test;

typedef struct {
    rfc_test_cmd_t param;
    tRfcomm_test   data;
}tRFC;

typedef struct {
    size_t          size;
    bt_status_t (*init)( tL2CAP_APPL_INFO* callbacks );
    void  (*rdut_rfcomm)( UINT8 server );
    void  (*rdut_rfcomm_test_interface)( tRFC *input);
    bt_status_t (*connect)( bt_bdaddr_t *bd_addr );
    void  (*cleanup)( void );
} btrfcomm_interface_t;

#endif

__END_DECLS

#endif /* ANDROID_INCLUDE_BT_TESTAPP_H */
