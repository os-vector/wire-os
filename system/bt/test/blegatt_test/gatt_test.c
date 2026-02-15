/******************************************************************************
 ** Copyright (c) 2014-2015, The Linux Foundation. All rights reserved.
 *
 *  Not a Contribution.
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


/************************************************************************************
 *
 *  Filename:      gatt_tool.c
 *
 *  Description:   Bluedroid GATT TOOL application
 *
 ***********************************************************************************/

#include <stdio.h>
#include <stdint.h>
#include <dlfcn.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <ctype.h>
#include <fcntl.h>
#include <sys/prctl.h>
#include <sys/capability.h>
//#include <sys/time.h>
//#include <signal.h>
//#include <time.h>
#include "bt_target.h"
#include "l2c_api.h"
#include "bta_api.h"
#include "l2c_int.h"
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>
#include <private/android_filesystem_config.h>
#include <android/log.h>

#include <hardware/hardware.h>
#include <hardware/bluetooth.h>
#include <hardware/bt_gatt.h>
#include <hardware/bt_gatt_client.h>
#include <hardware/bt_gatt_server.h>
#include <hardware/bt_gatt_types.h>
//#include "../../osi/include/allocator.h"
#include <bt_testapp.h>

#include <signal.h>
#include <time.h>


#ifdef TEST_APP_INTERFACE
/************************************************************************************
**  Constants & Macros
************************************************************************************/

#ifndef TRUE
#define     TRUE       1
#endif
#ifndef FALSE
#define     FALSE      0
#endif

#define PID_FILE "/data/.bdt_pid"

#ifndef MAX
#define MAX(x, y) ((x) > (y) ? (x) : (y))
#endif

#define CASE_RETURN_STR(const) case const: return #const;

/************************************************************************************
**  Local type definitions
************************************************************************************/
static void register_client_cb(int status, int client_if, bt_uuid_t *app_uuid);
static void scan_result_cb(bt_bdaddr_t* remote_bd_addr, int rssi, uint8_t* adv_data);
static void listen_cb(int status, int server_if);

static void register_server_cb(int status, int server_if, bt_uuid_t *app_uuid);


/************************************************************************************
**  Static variables
************************************************************************************/

static unsigned char main_done = 0;
static bt_status_t status;
typedef struct
{
    UINT16      result;                 /* Only used in confirm messages */
    UINT16      credits;                /* used to send the outstanding credits */
    UINT16      le_psm;
    UINT16      le_mps;
    UINT16      le_mtu;
    UINT16      init_credits;          /* initial credits */
} tL2CAP_LE_CONN_INFO;

typedef struct
{
    BOOLEAN                 in_use;
    UINT16                  psm;
    UINT16                  lcid;
    tL2CAP_LE_CONN_INFO     loc_conn_info;
    tL2CAP_LE_CONN_INFO     rmt_conn_info;
    BOOLEAN                 is_server;
} t_le_chnl_info;

t_le_chnl_info le_chnl_conn_info[MAX_L2CAP_CLIENTS];
#define LE_ACL_MAX_BUFF_SIZE 4096
static int num_frames = 1;
static unsigned long g_delay = 1; /* Default delay before data transfer */
static int count = 1;
static UINT16 g_BleEncKeySize = 16;
//static int g_omps = 0;
//static int rcv_count = 0;
static int g_le_coc_if = 0;
static int rcv_itration = 0;
static volatile BOOLEAN cong_status = FALSE;
/* Control channel LE-L2CAP default options */
static tL2CAP_LE_CONN_INFO le_conn_info;
static tL2CAP_LE_CFG_INFO local_coc_cfg;

/* Main API */
static bluetooth_device_t* bt_device;

const bt_interface_t* sBtInterface = NULL;

static gid_t groups[] = { AID_NET_BT, AID_INET, AID_NET_BT_ADMIN,
                          AID_SYSTEM, AID_MISC, AID_SDCARD_RW,
                          AID_NET_ADMIN, AID_VPN};

enum {
    DISCONNECT,
    CONNECTING,
    CONNECTED,
    DISCONNECTING
};
static unsigned char bt_enabled = 0;
static int  g_ConnectionState   = DISCONNECT;
static int  g_AdapterState      = BT_STATE_OFF;
static int  g_PairState         = BT_BOND_STATE_NONE;


static int  g_conn_id        = 0;
static int  g_client_if      = 0;
static int  g_server_if      = 0;
static int  g_client_if_scan = 0;
static int  g_server_if_scan = 0;

const btgatt_test_interface_t     *sGattInterface = NULL;
const  btgatt_interface_t   *sGattIfaceScan = NULL;
const btsmp_interface_t    *sSmpIface             = NULL;
const btgap_interface_t    *sGapInterface         = NULL;
const btl2cap_interface_t *sL2capInterface = NULL;


int  Btif_gatt_layer = TRUE;
bt_bdaddr_t *remote_bd_address;

static UINT16 g_SecLevel = 0;
static BOOLEAN g_ConnType = TRUE;//DUT is initiating connection
static BOOLEAN g_Fcr_Present = FALSE;
static UINT8 g_Fcr_Mode = L2CAP_FCR_BASIC_MODE;
static UINT8 g_Ertm_AllowedMode = (L2CAP_FCR_CHAN_OPT_BASIC | L2CAP_FCR_CHAN_OPT_ERTM | L2CAP_FCR_CHAN_OPT_STREAM);


/* Default mtu */
static int g_imtu = 672;
static int g_omtu = 0;

enum {
L2CAP_NOT_CONNECTED,
L2CAP_CONN_SETUP,
L2CAP_CONNECTED
};

//static int L2cap_conn_state = L2CAP_NOT_CONNECTED;
static tL2CAP_CFG_INFO tl2cap_cfg_info;
static long data_size = -1;
static UINT16           g_PSM           = 0;
static UINT16           g_lcid          = 0;


enum {
    SEND,
    RECEIVE,
    WAITANDSEND,
    PAIR,
    PING,
    CONNECT,
};


/* Control channel eL2CAP default options */
tL2CAP_FCR_OPTS ertm_fcr_opts_def = {
    L2CAP_FCR_ERTM_MODE,
    3, /* Tx window size */
    MCA_FCR_OPT_MAX_TX_B4_DISCNT, /* Maximum transmissions before disconnecting */
    2000, /* Retransmission timeout (2 secs) */
    MCA_FCR_OPT_MONITOR_TOUT, /* Monitor timeout (12 secs) */
    100 /* MPS segment size */
};

tL2CAP_FCR_OPTS stream_fcr_opts_def = {
    L2CAP_FCR_STREAM_MODE,
    3,/* Tx window size */
    MCA_FCR_OPT_MAX_TX_B4_DISCNT, /* Maximum transmissions before disconnecting */
    2000, /* Retransmission timeout (2 secs) */
    MCA_FCR_OPT_MONITOR_TOUT, /* Monitor timeout (12 secs) */
    100 /* MPS segment size */
};
static tL2CAP_ERTM_INFO t_ertm_info = {0, 0, 0, 0, 0, 0};


/************************************************************************************
**  Static functions
************************************************************************************/

static void process_cmd(char *p, unsigned char is_job);
//static void job_handler(void *param);
static void bdt_log(const char *fmt_str, ...);
static void l2c_connect(bt_bdaddr_t *bd_addr);
static UINT16 do_l2cap_connect(bt_bdaddr_t * bd_addr);



int GetBdAddr(char *p, bt_bdaddr_t *pbd_addr);

/* LE L2CAP functions */
static t_le_chnl_info *le_allocate_conn_info(UINT16 psm, BOOLEAN is_server);
static t_le_chnl_info *le_get_conn_info(UINT16 psm, BOOLEAN is_server);
static t_le_chnl_info *le_get_conn_info_by_lcid(UINT16 lcid);
static BOOLEAN le_release_conn_info(t_le_chnl_info *le_conn_info);
UINT8 do_l2cap_DataWrite(UINT16 chnl_id, char *p , UINT32 len);
static int Send_Data();
static int send_file(char *p);
static void le_l2cap_coc_connect(char *svr);
UINT16 do_le_l2cap_coc_connect(char *p);
static void le_l2cap_coc_flow_ctrl(char *p);
UINT16 do_le_l2cap_coc_flow_ctrl(char *p);
static void do_le_coc_disconnect(char *p);
int GetFileName(char *p, char *filename);

/************************************************************************************
**  GATT Client Callbacks
************************************************************************************/
static void register_client_cb(int status, int client_if, bt_uuid_t *app_uuid)
{
    printf("%s:: status=%d, client_if=%d, uuid=%02x%02x%02x%02x %02x%02x%02x%02x %02x%02x%02x%02x %02x%02x%02x%02x \n", __FUNCTION__, status, client_if,
            app_uuid->uu[0], app_uuid->uu[1], app_uuid->uu[2], app_uuid->uu[3],
            app_uuid->uu[4], app_uuid->uu[5], app_uuid->uu[6], app_uuid->uu[7],
            app_uuid->uu[8], app_uuid->uu[9], app_uuid->uu[10], app_uuid->uu[11],
            app_uuid->uu[12], app_uuid->uu[13], app_uuid->uu[14], app_uuid->uu[15]);
    if(0 == status)    g_client_if_scan = client_if;
}

static void scan_result_cb(bt_bdaddr_t* remote_bd_addr, int rssi, uint8_t* adv_data)
{
    printf("%s:: remote_bd_addr= %02X%02X%02X%02X%02X%02X , adv_data=0x%x \n",  __FUNCTION__,
    remote_bd_addr->address[0], remote_bd_addr->address[1], remote_bd_addr->address[2],
    remote_bd_addr->address[3], remote_bd_addr->address[4], remote_bd_addr->address[5], *adv_data);
}

static void connect_cb(int conn_id, int status, int client_if, bt_bdaddr_t* remote_bd_addr)
{
    printf("%s:: remote_bd_addr=%02x:%02x:%02x:%02x:%02x:%02x, conn_id=0x%x, status=%d, client_if=%d\n",  __FUNCTION__,
    remote_bd_addr->address[0], remote_bd_addr->address[1], remote_bd_addr->address[2],
    remote_bd_addr->address[3], remote_bd_addr->address[4], remote_bd_addr->address[5], conn_id, status, client_if);

    g_conn_id = conn_id;
    sGapInterface->Gap_BleAttrDBUpdate(remote_bd_addr->address, 50, 70, 0, 1000);

}

/*
static void register_for_notification_cb(int conn_id, int registered, int status, btgatt_srvc_id_t *srvc_id, btgatt_gatt_id_t *char_id)
{
    printf("%s:: conn_id=%d, registered=%d, status=%d \n", __FUNCTION__, conn_id, registered, status);
}
*/

static void listen_cb(int status, int server_if)
{
    printf("%s:: status=%d, server_if=%d \n", __FUNCTION__, status, server_if);
    if(0 == status)    g_server_if = server_if;
}

static btgatt_client_callbacks_t sGattClient_cb =
{
    register_client_cb,
    scan_result_cb,
    connect_cb,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL, //register_for_notification_cb,
    NULL,
    NULL,
    NULL,
    listen_cb,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL
};


/************************************************************************************
**  GATT Server Callbacks
************************************************************************************/
static void register_server_cb(int status, int server_if, bt_uuid_t *app_uuid)
{
    printf("%s:: status=%d, server_if=%d \n", __FUNCTION__, status, server_if);
    if(0 == status)    g_server_if_scan = server_if;
}

static void server_connection_cb(int conn_id, int server_if, int connected, bt_bdaddr_t *bda)
{
    printf("%s:: conn_id=%d, server_if=%d \n", __FUNCTION__, conn_id, server_if);
    g_conn_id = conn_id;
}

static btgatt_server_callbacks_t     sGattServer_cb =
{
    register_server_cb,
    server_connection_cb, //connection_callback             connection_cb;
    NULL, //service_added_callback          service_added_cb;
    NULL, //included_service_added_callback included_service_added_cb;
    NULL, //characteristic_added_callback   characteristic_added_cb;
    NULL, //descriptor_added_callback       descriptor_added_cb;
    NULL, //service_started_callback        service_started_cb;
    NULL, //service_stopped_callback        service_stopped_cb;
    NULL, //service_deleted_callback        service_deleted_cb;
    NULL, //request_read_callback           request_read_cb;
    NULL, //request_write_callback          request_write_cb;
    NULL, //request_exec_write_callback     request_exec_write_cb;
    NULL, //response_confirmation_callback  response_confirmation_cb;
    NULL,
    NULL,
    NULL
};


/************************************************************************************
**  GATT Callbacks
************************************************************************************/
static void DiscoverRes_cb (UINT16 conn_id, tGATT_DISC_TYPE disc_type, tGATT_DISC_RES *p_data)
{
    printf("%s:: conn_id=%d, disc_type=%d\n", __FUNCTION__, conn_id, disc_type);
}

static void DiscoverCmpl_cb (UINT16 conn_id, tGATT_DISC_TYPE disc_type, tGATT_STATUS status)
{
    printf("%s:: conn_id=%d, disc_type=%d, status=%d\n", __FUNCTION__, conn_id, disc_type, status);
}

static void  OperationCmpl_cb(UINT16 conn_id, tGATTC_OPTYPE op, tGATT_STATUS status, tGATT_CL_COMPLETE *p_data)
{
    printf("%s:: conn_id=%d, op=%d, status=%d\n", __FUNCTION__, conn_id, op, status);
}

static void Connection_cb (tGATT_IF gatt_if, BD_ADDR bda, UINT16 conn_id, BOOLEAN connected, tGATT_DISCONN_REASON reason,tBT_TRANSPORT transport)
{
    printf("%s:: remote_bd_addr=%02x:%02x:%02x:%02x:%02x:%02x, conn_id=0x%x, connected=%d, reason=%d, gatt_if=%d \n", __FUNCTION__,
            bda[0], bda[1], bda[2], bda[3], bda[4], bda[5],
            conn_id, connected, reason, gatt_if);
    g_conn_id = conn_id;
}

static void AttributeReq_cb(UINT16 conn_id, UINT32 trans_id, tGATTS_REQ_TYPE type, tGATTS_DATA *p_data)
{
    printf("%s:: conn_id=%d, trans_id=%d, type=%u\n", __FUNCTION__, conn_id, trans_id, type);
}


static tGATT_CBACK sGattCB =
{
    Connection_cb,
    OperationCmpl_cb,
    DiscoverRes_cb,
    DiscoverCmpl_cb,
    AttributeReq_cb,
    NULL,
    NULL
};

/************************************************************************************
**  GAP Callbacks
************************************************************************************/
/*
static void gap_ble_s_attr_request_cback (UINT16 conn_id, UINT32 trans_id, tGATTS_REQ_TYPE op_code, tGATTS_DATA *p_data)
{
    printf("%s:: conn_id=%d, trans_id=%d, op_code=%u\n", __FUNCTION__, conn_id, trans_id, op_code);
}

// client connection callback

static void  gap_ble_c_connect_cback (tGATT_IF gatt_if, BD_ADDR bda, UINT16 conn_id, BOOLEAN connected, tGATT_DISCONN_REASON reason,tBT_TRANSPORT transport)
{
    printf("%s:: gatt_if=%d, remote_bd_addr=%02x:%02x:%02x:%02x:%02x:%02x, conn_id=%d, connected=%d, reason=%d\n", __FUNCTION__,
    gatt_if, bda[0], bda[1], bda[2], bda[3], bda[4], bda[5], conn_id, connected, reason);
    g_conn_id = conn_id;
}

static void  gap_ble_c_cmpl_cback (UINT16 conn_id, tGATTC_OPTYPE op, tGATT_STATUS status, tGATT_CL_COMPLETE *p_data)
{
    printf("%s:: conn_id=%d, op=%d, status=%d\n", __FUNCTION__, conn_id, op, status);
}


static tGATT_CBACK gap_cback =
{
    gap_ble_c_connect_cback,
    gap_ble_c_cmpl_cback,
    NULL,
    NULL,
    gap_ble_s_attr_request_cback,
    NULL,
    NULL
};
*/


/************************************************************************************
**  SMP Callbacks
************************************************************************************/
static UINT8 SMP_cb (tSMP_EVT event, BD_ADDR bda, tSMP_EVT_DATA *p_data)
{
    printf("%s:: event=%d(1-SMP_IO_CAP_REQ_EVT, 2-SMP_SEC_REQUEST_EVT,    \
                   3-SMP_PASSKEY_NOTIF_EVT, 4-SMP_PASSKEY_REQ_EVT, 6-SMP_COMPLT_EVT),   \
                  \nremote_bd_addr=%02x:%02x:%02x:%02x:%02x:%02x, PassKey=%u \n", __FUNCTION__, event,
            bda[0], bda[1], bda[2], bda[3], bda[4], bda[5], p_data->passkey);
    switch(event)
    {
    case SMP_IO_CAP_REQ_EVT:
        printf("Io_Caps=%d, auth_req=%d, max_key_size=%d, init_keys=%d, resp_keys=%d \n", p_data->io_req.io_cap, p_data->io_req.auth_req, p_data->io_req.max_key_size, p_data->io_req.init_keys, p_data->io_req.resp_keys);
        break;

    case SMP_PASSKEY_REQ_EVT:
    case SMP_PASSKEY_NOTIF_EVT:
        printf("passkey value=%u\n", p_data->passkey);
        sSmpIface->PasskeyReply(bda, SMP_SUCCESS, p_data->passkey);
        break;
    case SMP_OOB_REQ_EVT:
        //p_dev_rec->sec_flags |= BTM_SEC_LINK_KEY_AUTHED;
        break;
    case SMP_SEC_REQUEST_EVT:
    case SMP_COMPLT_EVT:
        printf("SMP Complete Event:: Reason=%d \n", p_data->cmplt.reason);
        if(p_data->cmplt.reason == SMP_SUCCESS)
        {
            sSmpIface->SecurityGrant(bda, p_data->cmplt.reason);
            printf("Granting Security \n");
        }
        break;
    }
    return 0;
}




/************************************************************************************
**  Shutdown helper functions
************************************************************************************/

static void bdt_shutdown(void)
{
    bdt_log("shutdown bdroid test app\n");
    main_done = 1;
}


/*****************************************************************************
** Android's init.rc does not yet support applying linux capabilities
*****************************************************************************/

static void config_permissions(void)
{
    struct __user_cap_header_struct header;
    struct __user_cap_data_struct cap[2];

    bdt_log("set_aid_and_cap : pid %d, uid %d gid %d", getpid(), getuid(), getgid());

    header.pid = 0;

    prctl(PR_SET_KEEPCAPS, 1, 0, 0, 0);

    setuid(AID_BLUETOOTH);
    setgid(AID_BLUETOOTH);

    header.version = _LINUX_CAPABILITY_VERSION_3;

    cap[CAP_TO_INDEX(CAP_NET_RAW)].permitted |= CAP_TO_MASK(CAP_NET_RAW);
    cap[CAP_TO_INDEX(CAP_NET_ADMIN)].permitted |= CAP_TO_MASK(CAP_NET_ADMIN);
    cap[CAP_TO_INDEX(CAP_NET_BIND_SERVICE)].permitted |= CAP_TO_MASK(CAP_NET_BIND_SERVICE);
    cap[CAP_TO_INDEX(CAP_SYS_RAWIO)].permitted |= CAP_TO_MASK(CAP_SYS_RAWIO);
    cap[CAP_TO_INDEX(CAP_SYS_NICE)].permitted |= CAP_TO_MASK(CAP_SYS_NICE);
    cap[CAP_TO_INDEX(CAP_SETGID)].permitted |= CAP_TO_MASK(CAP_SETGID);
    cap[CAP_TO_INDEX(CAP_WAKE_ALARM)].permitted |= CAP_TO_MASK(CAP_WAKE_ALARM);

    cap[CAP_TO_INDEX(CAP_NET_RAW)].effective |= CAP_TO_MASK(CAP_NET_RAW);
    cap[CAP_TO_INDEX(CAP_NET_ADMIN)].effective |= CAP_TO_MASK(CAP_NET_ADMIN);
    cap[CAP_TO_INDEX(CAP_NET_BIND_SERVICE)].effective |= CAP_TO_MASK(CAP_NET_BIND_SERVICE);
    cap[CAP_TO_INDEX(CAP_SYS_RAWIO)].effective |= CAP_TO_MASK(CAP_SYS_RAWIO);
    cap[CAP_TO_INDEX(CAP_SYS_NICE)].effective |= CAP_TO_MASK(CAP_SYS_NICE);
    cap[CAP_TO_INDEX(CAP_SETGID)].effective |= CAP_TO_MASK(CAP_SETGID);
    cap[CAP_TO_INDEX(CAP_WAKE_ALARM)].effective |= CAP_TO_MASK(CAP_WAKE_ALARM);

    capset(&header, &cap[0]);
    setgroups(sizeof(groups)/sizeof(groups[0]), groups);
}



/*****************************************************************************
**   Logger API
*****************************************************************************/

void bdt_log(const char *fmt_str, ...)
{
    static char buffer[1024];
    va_list ap;

    va_start(ap, fmt_str);
    vsnprintf(buffer, 1024, fmt_str, ap);
    va_end(ap);

    fprintf(stdout, "%s\n", buffer);
}

/*******************************************************************************
 ** Misc helper functions
 *******************************************************************************/
static const char* dump_bt_status(bt_status_t status)
{
    switch(status)
    {
        CASE_RETURN_STR(BT_STATUS_SUCCESS)
        CASE_RETURN_STR(BT_STATUS_FAIL)
        CASE_RETURN_STR(BT_STATUS_NOT_READY)
        CASE_RETURN_STR(BT_STATUS_NOMEM)
        CASE_RETURN_STR(BT_STATUS_BUSY)
        CASE_RETURN_STR(BT_STATUS_UNSUPPORTED)

        default:
            return "unknown status code";
    }
}

/*
static void hex_dump(char *msg, void *data, int size, int trunc)
{
    unsigned char *p = data;
    unsigned char c;
    int n;
    char bytestr[4] = {0};
    char addrstr[10] = {0};
    char hexstr[ 16*3 + 5] = {0};
    char charstr[16*1 + 5] = {0};

    bdt_log("%s  \n", msg);

    // truncate
    if(trunc && (size>32))
        size = 32;

    for(n=1;n<=size;n++) {
        if (n%16 == 1) {
            // store address for this line
            snprintf(addrstr, sizeof(addrstr), "%.4x",
               ((intptr_t)p-(intptr_t)data) );
        }

        c = *p;
        if (isalnum(c) == 0) {
            c = '.';
        }

        // store hex str (for left side)
        snprintf(bytestr, sizeof(bytestr), "%02X ", *p);
        strncat(hexstr, bytestr, sizeof(hexstr)-strlen(hexstr)-1);

        // store char str (for right side)
        snprintf(bytestr, sizeof(bytestr), "%c", c);
        strncat(charstr, bytestr, sizeof(charstr)-strlen(charstr)-1);

        if(n%16 == 0) {
            // line completed
            bdt_log("[%4.4s]   %-50.50s  %s\n", addrstr, hexstr, charstr);
            hexstr[0] = 0;
            charstr[0] = 0;
        } else if(n%8 == 0) {
            // half line: add whitespaces
            strncat(hexstr, "  ", sizeof(hexstr)-strlen(hexstr)-1);
            strncat(charstr, " ", sizeof(charstr)-strlen(charstr)-1);
        }
        p++; // next byte
    }

    if (strlen(hexstr) > 0) {
        // print rest of buffer if not empty
        bdt_log("[%4.4s]   %-50.50s  %s\n", addrstr, hexstr, charstr);
    }
}
*/

/*******************************************************************************
 ** Console helper functions
 *******************************************************************************/

void skip_blanks(char **p)
{
    while (**p == ' ')
    (*p)++;
}

uint32_t get_int(char **p, int DefaultValue)
{
    uint32_t Value = 0;
    unsigned char   UseDefault;

    UseDefault = 1;
    skip_blanks(p);

    while ( ((**p)<= '9' && (**p)>= '0') )
    {
        Value = Value * 10 + (**p) - '0';
        UseDefault = 0;
        (*p)++;
    }
   if (UseDefault)
       return DefaultValue;
   else
       return Value;
}

int get_signed_int(char **p, int DefaultValue)
{
    int    Value = 0;
    unsigned char   UseDefault;
    unsigned char  NegativeNum = 0;

    UseDefault = 1;
    skip_blanks(p);

    if ((**p) == '-')
    {
        NegativeNum = 1;
        (*p)++;
    }
    while ( ((**p)<= '9' && (**p)>= '0') )
    {
        Value = Value * 10 + (**p) - '0';
        UseDefault = 0;
        (*p)++;
    }

    if (UseDefault)
        return DefaultValue;
    else
        return ((NegativeNum == 0)? Value : -Value);
}

void get_str(char **p, char *Buffer)
{
    skip_blanks(p);
    while (**p != 0 && **p != ' ')
    {
        *Buffer = **p;
        (*p)++;
        Buffer++;
    }

    *Buffer = 0;
}

uint32_t get_hex_any(char **p, int DefaultValue, unsigned int NumOfNibble)
{
    uint32_t Value = 0;
    unsigned char   UseDefault;

    UseDefault = 1;
    skip_blanks(p);

    while ((NumOfNibble) && (((**p)<= '9' && (**p)>= '0') ||
          ((**p)<= 'f' && (**p)>= 'a') ||
          ((**p)<= 'F' && (**p)>= 'A')) )
    {
        if (**p >= 'a')
            Value = Value * 16 + (**p) - 'a' + 10;
        else if (**p >= 'A')
            Value = Value * 16 + (**p) - 'A' + 10;
        else
        Value = Value * 16 + (**p) - '0';
        UseDefault = 0;
        (*p)++;
        NumOfNibble--;
    }

    if (UseDefault)
        return DefaultValue;
    else
        return Value;
}
uint32_t get_hex(char **p, int DefaultValue)
{
    return (get_hex_any(p, DefaultValue, 8));
}
uint32_t get_hex_byte(char **p, int DefaultValue)
{
    return (get_hex_any(p, DefaultValue, 2));
}

void get_bdaddr(const char *str, bt_bdaddr_t *bd) {
    char *d = ((char *)bd), *endp;
    int i;
    for(i = 0; i < 6; i++) {
        *d++ = strtol(str, &endp, 16);
        if (*endp != ':' && i != 5) {
            memset(bd, 0, sizeof(bt_bdaddr_t));
            return;
        }
        str = endp + 1;
    }
}

#define is_cmd(str) ((strlen(str) == strlen(cmd)) && strncmp((const char *)&cmd, str, strlen(str)) == 0)
#define if_cmd(str)  if (is_cmd(str))

typedef void (t_console_cmd_handler) (char *p);

typedef struct {
    const char *name;
    t_console_cmd_handler *handler;
    const char *help;
    unsigned char is_job;
} t_cmd;


const t_cmd console_cmd_list[];
static int console_cmd_maxlen = 0;

static void cmdjob_handler(void *param)
{
    char *job_cmd = (char*)param;

    bdt_log("cmdjob starting (%s)", job_cmd);

    process_cmd(job_cmd, 1);

    bdt_log("cmdjob terminating");

    free(job_cmd);
}

static int create_cmdjob(char *cmd)
{
    pthread_t thread_id;
    char *job_cmd;

    job_cmd = (char*)calloc(1, strlen(cmd)+1); /* freed in job handler */
    if (job_cmd) {
       strlcpy(job_cmd, cmd,(strlen(cmd)+1));
      if (pthread_create(&thread_id, NULL,
                       (void*)cmdjob_handler, (void*)job_cmd)!=0)
         perror("pthread_create");
      return 0;
    }
    else
       perror("create_Cmdjob malloc failed ");
    return -1;
}

/*******************************************************************************
 ** Load stack lib
 *******************************************************************************/

int HAL_load(void)
{
    int err = 0;

    hw_module_t* module;
    hw_device_t* device;

    bdt_log("Loading HAL lib + extensions");

    err = hw_get_module(BT_HARDWARE_MODULE_ID, (hw_module_t const**)&module);
    if (err == 0)
    {

        err = module->methods->open(module, BT_HARDWARE_MODULE_ID, &device);
        bdt_log("HAL library open (%s)", strerror(err));
        if (err == 0) {
            bt_device = (bluetooth_device_t *)device;
            sBtInterface = bt_device->get_bluetooth_interface();
        }
    }

    bdt_log("HAL library loaded (%s) interface pointer =%x ", strerror(err), sBtInterface);

    return err;
}

int HAL_unload(void)
{
    int err = 0;

    bdt_log("Unloading HAL lib");

    sBtInterface = NULL;

    bdt_log("HAL library unloaded (%s)", strerror(err));

    return err;
}

/*******************************************************************************
 ** HAL test functions & callbacks
 *******************************************************************************/

void setup_test_env(void)
{
    int i = 0;

    while (console_cmd_list[i].name != NULL)
    {
        console_cmd_maxlen = MAX(console_cmd_maxlen, (int)strlen(console_cmd_list[i].name));
        i++;
    }
}

void check_return_status(bt_status_t status)
{
    if (status != BT_STATUS_SUCCESS)
    {
        bdt_log("HAL REQUEST FAILED status : %d (%s)", status, dump_bt_status(status));
    }
    else
    {
        bdt_log("HAL REQUEST SUCCESS");
    }
}
/*
static void do_set_adv_params(char *p)
{
    bt_bdaddr_t bd_addr = {{0}};
    int int_min = 0x0, int_max = 0x0, addr_type = 0;
    int_max = get_int(&p, -1);
    int_min = get_int(&p, -1);
    if(int_max < int_min)
        return;
//    if(FALSE == GetBdAddr(p, &bd_addr))    return;
//    sBtInterface->le_set_adv_params(int_min, int_max, &bd_addr, addr_type);
}
*/

static void do_set_localname(char *p)
{
    printf("set name in progress: %s\n", p);
    bt_property_t property = {BT_PROPERTY_BDNAME, strlen(p), p};
    status = sBtInterface->set_adapter_property(&property);
}

static void adapter_state_changed(bt_state_t state)
{
    int V1 = 1000, V2=2;
    bt_property_t property = {9 /*BT_PROPERTY_DISCOVERY_TIMEOUT*/, 4, &V1};
    bt_property_t property1 = {7 /*SCAN*/, 2, &V2};
    bt_property_t property2 ={1,9,"GATTTOOL"};
    printf("ADAPTER STATE UPDATED : %s\n", (state == BT_STATE_OFF)?"OFF":"ON");

    g_AdapterState = state;

    if (state == BT_STATE_ON) {
        bt_enabled = 1;
        status = sBtInterface->set_adapter_property(&property1);
        status = sBtInterface->set_adapter_property(&property);
        status = sBtInterface->set_adapter_property(&property2);
    } else {
        bt_enabled = 0;
    }
}

static void adapter_properties_changed(bt_status_t status, int num_properties, bt_property_t *properties)
{
 char Bd_addr[15] = {0};
    if(NULL == properties)
    {
        printf("properties is null\n");
        return;
    }
    switch(properties->type)
    {
    case BT_PROPERTY_BDADDR:
        memcpy(Bd_addr, properties->val, properties->len);
        break;
//    case BT_PROPERTY_ADAPTER_BLE_ADV_MODE:
 //       printf("Set in advertisement mode\n");
 //       break;
    default:
        printf("property type not used\n");
    }
    return;
}

static void discovery_state_changed(bt_discovery_state_t state)
{
    printf("Discovery State Updated : %s\n", (state == BT_DISCOVERY_STOPPED)?"STOPPED":"STARTED");
}


static void pin_request_cb(bt_bdaddr_t *remote_bd_addr, bt_bdname_t *bd_name, uint32_t cod, bool min_16_digit )
{
    remote_bd_address = remote_bd_addr;
    //bt_pin_code_t pincode = {{0x31, 0x32, 0x33, 0x34}};
    printf("Enter the pin key displayed in the remote device and terminate the key entry with .\n");

    /*if(BT_STATUS_SUCCESS != sBtInterface->pin_reply(remote_bd_addr, TRUE, 4, &pincode))
    {
        printf("Pin Reply failed\n");
    }*/
}
static void ssp_request_cb(bt_bdaddr_t *remote_bd_addr, bt_bdname_t *bd_name,
                           uint32_t cod, bt_ssp_variant_t pairing_variant, uint32_t pass_key)
{
    printf("ssp_request_cb : name=%s variant=%d passkey=%u\n", bd_name->name, pairing_variant, pass_key);
    if(BT_STATUS_SUCCESS != sBtInterface->ssp_reply(remote_bd_addr, pairing_variant, TRUE, pass_key))
    {
        printf("SSP Reply failed\n");
    }
}

static void bond_state_changed_cb(bt_status_t status, bt_bdaddr_t *remote_bd_addr, bt_bond_state_t state)
{
    g_PairState = state;
}

static void acl_state_changed(bt_status_t status, bt_bdaddr_t *remote_bd_addr, bt_acl_state_t state)
{
    printf("acl_state_changed : remote_bd_addr=%02x:%02x:%02x:%02x:%02x:%02x, acl status=%s \n",
    remote_bd_addr->address[0], remote_bd_addr->address[1], remote_bd_addr->address[2],
    remote_bd_addr->address[3], remote_bd_addr->address[4], remote_bd_addr->address[5],
    (state == BT_ACL_STATE_CONNECTED)?"ACL Connected" :"ACL Disconnected"
    );
}
static void dut_mode_recv(uint16_t opcode, uint8_t *buf, uint8_t len)
{
    bdt_log("DUT MODE RECV : NOT IMPLEMENTED");
}

static void le_test_mode(bt_status_t status, uint16_t packet_count)
{
    bdt_log("LE TEST MODE END status:%s number_of_packets:%d", dump_bt_status(status), packet_count);
}

extern int timer_create (clockid_t, struct sigevent *__restrict, timer_t *__restrict);
extern int timer_settime (timer_t, int, const struct itimerspec *__restrict, struct itimerspec *__restrict);

static bool set_wake_alarm(uint64_t delay_millis, bool should_wake, alarm_cb cb, void *data)
{

   static timer_t timer;
   static bool timer_created;

   if (!timer_created) {
      struct sigevent sigevent;
      memset(&sigevent, 0, sizeof(sigevent));
      sigevent.sigev_notify = SIGEV_THREAD;
      sigevent.sigev_notify_function = (void (*)(union sigval))cb;
      sigevent.sigev_value.sival_ptr = data;
      timer_create(CLOCK_MONOTONIC, &sigevent, &timer);
      timer_created = true;
   }

   struct itimerspec new_value;
   new_value.it_value.tv_sec = delay_millis / 1000;
   new_value.it_value.tv_nsec = (delay_millis % 1000) * 1000 * 1000;
   new_value.it_interval.tv_sec = 0;
   new_value.it_interval.tv_nsec = 0;
   timer_settime(timer, 0, &new_value, NULL);
   return true;
}

static int acquire_wake_lock(const char *lock_name)
{
    return BT_STATUS_SUCCESS;
}

static int release_wake_lock(const char *lock_name)
{
    return BT_STATUS_SUCCESS;
}

static bt_callbacks_t bt_callbacks = {
    sizeof(bt_callbacks_t),
    adapter_state_changed,
    adapter_properties_changed, /*adapter_properties_cb */
    NULL, /* remote_device_properties_cb */
    NULL, /* device_found_cb */
    discovery_state_changed, /* discovery_state_changed_cb */
    pin_request_cb, /* pin_request_cb  */
    ssp_request_cb, /* ssp_request_cb  */
    bond_state_changed_cb, /*bond_state_changed_cb */
    acl_state_changed, /* acl_state_changed_cb */
    NULL, /* thread_evt_cb */
    dut_mode_recv, /*dut_mode_recv_cb */
    le_test_mode, /* le_test_mode_cb */
    NULL,      /*energy_info_cb*/
    NULL       /* hci_event_recv_cb */
};

static bt_os_callouts_t bt_os_callbacks = {
     sizeof(bt_os_callouts_t),
     set_wake_alarm,
     acquire_wake_lock,
     release_wake_lock
};

static void l2test_l2c_connect_ind_cb(BD_ADDR bd_addr, UINT16 lcid, UINT16 psm, UINT8 id)
{

    UINT16 result;

    local_coc_cfg.credits = L2CAP_LE_DEFAULT_CREDIT;
    local_coc_cfg.mtu = L2CAP_LE_DEFAULT_MTU;
    local_coc_cfg.mps = L2CAP_LE_DEFAULT_MPS;
    /* Verify if LE PSM  */
   if (L2C_IS_VALID_LE_PSM(psm))
   {
       if (psm == 200)
       {
           printf("No Resources Available\n");
           result = L2CAP_LE_NO_RESOURCES;
           sL2capInterface->LeConnectRsp (bd_addr, id, lcid, result,L2CAP_LE_CONN_OK,&local_coc_cfg);
       }
       else if(psm == 201)
       {
           printf("L2CAP_LE_CONN_INSUFFI_AUTHORIZATION \n");
           result = L2CAP_LE_INSUFFICIENT_AUTHORIZATION;
           sL2capInterface->LeConnectRsp (bd_addr, id, lcid, result,L2CAP_LE_CONN_OK,&local_coc_cfg);
       }
       else
       {

           result = L2CAP_LE_CONN_OK;
           sL2capInterface->LeConnectRsp (bd_addr, id, lcid, result,L2CAP_LE_CONN_OK,&local_coc_cfg);
       }
       return;
   }
   if((L2CAP_FCR_ERTM_MODE == g_Fcr_Mode) || (L2CAP_FCR_STREAM_MODE == g_Fcr_Mode)) {
        sL2capInterface->ErtmConnectRsp(bd_addr, id, lcid, L2CAP_CONN_OK, L2CAP_CONN_OK, &t_ertm_info);
    } else {
        sL2capInterface->ConnectRsp(bd_addr, id, lcid, L2CAP_CONN_OK, L2CAP_CONN_OK);
    }
    {
        tL2CAP_CFG_INFO cfg;
        memcpy (&cfg ,&tl2cap_cfg_info,sizeof(tl2cap_cfg_info));
        if ((!sL2capInterface->ConfigReq (lcid, &cfg)) && cfg.fcr_present
              && cfg.fcr.mode != L2CAP_FCR_BASIC_MODE) {
            cfg.fcr.mode = L2CAP_FCR_BASIC_MODE;
            cfg.fcr_present = FALSE;
            sL2capInterface->ConfigReq (lcid, &cfg);
        }
    }
    g_ConnectionState = CONNECT;
    g_lcid = lcid;
}

static void l2test_l2c_connect_cfm_cb(UINT16 lcid, UINT16 result)
{
    t_le_chnl_info *le_conn_info = le_get_conn_info_by_lcid(lcid);
    if (le_conn_info&&L2C_IS_VALID_LE_PSM(le_conn_info->psm))
    {

        if (result == L2CAP_LE_CONN_OK) {
            g_ConnectionState = CONNECT;
        }
        else if(le_conn_info && !le_conn_info->is_server)
        {
            le_release_conn_info(le_conn_info);
        }
        return;
    }
#if 0
    if (result == L2CAP_CONN_OK) {
        L2cap_conn_state = L2CAP_CONN_SETUP;
        tL2CAP_CFG_INFO cfg;
        memcpy (&cfg ,&tl2cap_cfg_info,sizeof(tl2cap_cfg_info));
        sL2capInterface->ConfigReq (lcid, &cfg);
        g_imtu = cfg.mtu;
        g_ConnectionState = CONNECT;
        g_lcid = lcid;
    }
#endif
}

static void l2test_l2c_connect_pnd_cb(UINT16 lcid)
{
    g_ConnectionState = CONNECTING;
}
static void l2test_l2c_config_ind_cb(UINT16 lcid, tL2CAP_CFG_INFO *p_cfg)
{
    p_cfg->result = L2CAP_CFG_OK;
    p_cfg->fcr_present = FALSE;
    if(p_cfg->mtu_present) g_omtu = p_cfg->mtu;
    else g_omtu = L2CAP_DEFAULT_MTU;
    sL2capInterface->ConfigRsp (lcid, p_cfg);
    return;
}

static void l2test_l2c_config_cfm_cb(UINT16 lcid, tL2CAP_CFG_INFO *p_cfg)
{

    /* For now, always accept configuration from the other side */
    if (p_cfg->result == L2CAP_CFG_OK) {
        printf("\nl2test_l2c_config_cfm_cb Success\n");
    } else {

     /* If peer has rejected FCR and suggested basic then try basic */
    if (p_cfg->fcr_present) {
        tL2CAP_CFG_INFO cfg;
        memcpy (&cfg ,&tl2cap_cfg_info,sizeof(tl2cap_cfg_info));
        cfg.fcr_present = FALSE;
        sL2capInterface->ConfigReq (lcid, &cfg);
        // Remain in configure state
        return;
    }
    sL2capInterface->DisconnectReq(lcid);
    }
    if(0 == g_omtu) g_omtu = L2CAP_DEFAULT_MTU;
}

static void l2test_l2c_disconnect_ind_cb(UINT16 lcid, BOOLEAN ack_needed)
{
    t_le_chnl_info *le_conn_info = le_get_conn_info_by_lcid(lcid);
    /* release the conn info entry if it'a  client */
    if(le_conn_info &&  !le_conn_info->is_server)
    {
        le_release_conn_info(le_conn_info);
    }
    printf("l2test_le_l2c_disconnect_ind_cb, cid=0x%x, acks=%d\n", lcid, ack_needed);
    if (ack_needed)
    {
        /* send L2CAP disconnect response */
        sL2capInterface->DisconnectRsp(lcid);
    }
    g_ConnectionState = DISCONNECTING;
    g_lcid = 0;
}
static void l2test_l2c_disconnect_cfm_cb(UINT16 lcid, UINT16 result)
{
    t_le_chnl_info *le_conn_info = le_get_conn_info_by_lcid(lcid);
    /* release the conn info entry if it'a  client */
    if(le_conn_info &&  !le_conn_info->is_server)
    {
        le_release_conn_info(le_conn_info);
    }

    printf("l2test_le_l2c_disconnect_cfm_cb, cid=0x%x, result=%d\n", lcid, result);
    g_ConnectionState = DISCONNECT;
    g_lcid = 0;
}
static void l2test_l2c_QoSViolationInd(BD_ADDR bd_addr)
{
    printf("l2test_l2c_QoSViolationInd\n");
}
static void l2test_l2c_data_ind_cb(UINT16 lcid, BT_HDR *p_buf)
{
    rcv_itration++;
    printf("l2test_l2c_data_ind_cb:: itration=%d, event=%u, len=%u, "\
            "offset=%u, layer_specific=%u\n",rcv_itration, p_buf->event,
            p_buf->len, p_buf->offset, p_buf->layer_specific);
    sL2capInterface->LeFreeBuf(p_buf);
    printf("l2test_l2c_data_ind_cb:: event=%u, len=%u, offset=%u, layer_specific=%u\n",
            p_buf->event, p_buf->len, p_buf->offset, p_buf->layer_specific);
}
static void l2test_l2c_congestion_ind_cb(UINT16 lcid, BOOLEAN is_congested)
{
    cong_status = is_congested;
    printf("l2test_l2c_congestion_ind_cb is_congested %d\n ", is_congested);
}

static void l2test_l2c_tx_complete_cb (UINT16 lcid, UINT16 NoOfSDU)
{
    printf("l2test_l2c_tx_complete_cb, cid=0x%x, SDUs=%u\n", lcid, NoOfSDU);
}

/*
static void l2c_echo_rsp_cb(UINT16 p)
{
    printf("Ping Response = %s\n", (L2CAP_PING_RESULT_OK==p) ?"Ping Reply OK" :(L2CAP_PING_RESULT_NO_LINK==p) ?"Link Could Not be setup" :"Remote L2cap did not reply");
}
*/
/*LE-L2CAP Callback*/

#if 0

static void l2test_le_connect_ind_cb(BD_ADDR bd_addr, UINT16 lcid, UINT8 id,
                                    tL2CAP_LE_CONN_INFO *conn_info)
{
    printf(" l2test_le_connect_ind_cb\n lcid=%u\n id=%u\n ls_psm %d\n "\
            "le_mtu %d\n le_mps %d\n", lcid, id, conn_info->le_psm,
            conn_info->le_mtu, conn_info->le_mps);
    t_le_chnl_info *le_conn_info;
    if (conn_info->le_psm == 200)
    {
        printf("No Resources Available\n");
        conn_info->result = L2CAP_LE_NO_RESOURCES;
        sL2capInterface->LeConnectRsp (bd_addr, id, lcid, conn_info);
        return;
    }
    else if(conn_info->le_psm == 201)
    {
        printf("L2CAP_LE_CONN_INSUFFI_AUTHORIZATION \n");
        conn_info->result = L2CAP_LE_INSUFFICIENT_AUTHORIZATION;
        sL2capInterface->LeConnectRsp (bd_addr, id, lcid, conn_info);
        return;
    }

    le_conn_info = le_get_conn_info(conn_info->le_psm, TRUE);

    if(le_conn_info)
    {
        le_conn_info->loc_conn_info.result = L2CAP_LE_CONN_OK;
        le_conn_info->lcid = lcid;
        sL2capInterface->LeConnectRsp (bd_addr, id, lcid,
                &le_conn_info->loc_conn_info);
        memcpy(&le_conn_info->rmt_conn_info, conn_info,
                sizeof(tL2CAP_LE_CONN_INFO));
        g_ConnectionState = CONNECT;
    }
    else
    {
        printf("No PSM registered \n");
        conn_info->result = L2CAP_LE_NO_PSM;
        sL2capInterface->LeConnectRsp (bd_addr, id, lcid, conn_info);
        return;
    }
}

static void l2test_le_connect_cfm_cb(UINT16 lcid,
        tL2CAP_LE_CONN_INFO *conn_info)
{
    t_le_chnl_info *le_conn_info;

    printf(" l2test_le_connect_cfm_cb\n lcid=%u\n ls_psm %d\n le_mtu %d\n "\
            "le_mps %d\n result %d\n",lcid, conn_info->le_psm,
            conn_info->le_mtu, conn_info->le_mps, conn_info->result);

    le_conn_info = le_get_conn_info(conn_info->le_psm, FALSE);

    if (conn_info->result == L2CAP_LE_CONN_OK) {
        g_ConnectionState = CONNECT;
        if(le_conn_info)
        {
            memcpy(&le_conn_info->rmt_conn_info, conn_info,
                    sizeof(tL2CAP_LE_CONN_INFO));
            le_conn_info->lcid = lcid;
        }
    }
    else if(le_conn_info && !le_conn_info->is_server)
    {
        le_release_conn_info(le_conn_info);
    }
}
#endif

/* L2CAP callback function structure */
static tL2CAP_APPL_INFO l2test_l2c_appl = {
  //  sizeof(l2test_l2c_appl),
    l2test_l2c_connect_ind_cb,
    l2test_l2c_connect_cfm_cb,
    l2test_l2c_connect_pnd_cb,
    l2test_l2c_config_ind_cb,
    l2test_l2c_config_cfm_cb,
    l2test_l2c_disconnect_ind_cb,
    l2test_l2c_disconnect_cfm_cb,
    l2test_l2c_QoSViolationInd,
    l2test_l2c_data_ind_cb,
    l2test_l2c_congestion_ind_cb,
    l2test_l2c_tx_complete_cb
};




void bdt_init(void)
{
    bdt_log("INIT BT ");
    status = sBtInterface->init(&bt_callbacks);
    status = sBtInterface->set_os_callouts(&bt_os_callbacks);
    check_return_status(status);
}

void bdt_enable(void)
{
    bdt_log("ENABLE BT");
    if (bt_enabled) {
        bdt_log("Bluetooth is already enabled");
        return;
    }
    status = sBtInterface->enable(false);

    check_return_status(status);
}

void bdt_disable(void)
{
    bdt_log("DISABLE BT");
    if (!bt_enabled) {
        bdt_log("Bluetooth is already disabled");
        return;
    }
    status = sBtInterface->disable();

    check_return_status(status);
}

void do_pairing(char *p)
{
    bt_bdaddr_t bd_addr = {{0}};
    int transport = GATT_TRANSPORT_LE;
    if(FALSE == GetBdAddr(p, &bd_addr))    return;    // arg1
    if(BT_STATUS_SUCCESS != sBtInterface->create_bond(&bd_addr, transport))
    {
        printf("Failed to Initiate Pairing \n");
        return;
    }
}

void bdt_dut_mode_configure(char *p)
{
    int32_t mode = -1;

    bdt_log("BT DUT MODE CONFIGURE");
    if (!bt_enabled) {
        bdt_log("Bluetooth must be enabled for test_mode to work.");
        return;
    }
    mode = get_signed_int(&p, mode);
    if ((mode != 0) && (mode != 1)) {
        bdt_log("Please specify mode: 1 to enter, 0 to exit");
        return;
    }
    status = sBtInterface->dut_mode_configure(mode);

    check_return_status(status);
}

#define HCI_LE_RECEIVER_TEST_OPCODE 0x201D
#define HCI_LE_TRANSMITTER_TEST_OPCODE 0x201E
#define HCI_LE_END_TEST_OPCODE 0x201F

void bdt_le_test_mode(char *p)
{
    int cmd;
    unsigned char buf[3];
    int arg1, arg2, arg3;

    bdt_log("BT LE TEST MODE");
    if (!bt_enabled) {
        bdt_log("Bluetooth must be enabled for le_test to work.");
        return;
    }

    memset(buf, 0, sizeof(buf));
    cmd = get_int(&p, 0);
    switch (cmd)
    {
        case 0x1: /* RX TEST */
           arg1 = get_int(&p, -1);
           if (arg1 < 0) bdt_log("%s Invalid arguments", __FUNCTION__);
           buf[0] = arg1;
           status = sBtInterface->le_test_mode(HCI_LE_RECEIVER_TEST_OPCODE, buf, 1);
           break;
        case 0x2: /* TX TEST */
            arg1 = get_int(&p, -1);
            arg2 = get_int(&p, -1);
            arg3 = get_int(&p, -1);
            if ((arg1 < 0) || (arg2 < 0) || (arg3 < 0))
                bdt_log("%s Invalid arguments", __FUNCTION__);
            buf[0] = arg1;
            buf[1] = arg2;
            buf[2] = arg3;
            status = sBtInterface->le_test_mode(HCI_LE_TRANSMITTER_TEST_OPCODE, buf, 3);
           break;
        case 0x3: /* END TEST */
            status = sBtInterface->le_test_mode(HCI_LE_END_TEST_OPCODE, buf, 0);
           break;
        default:
            bdt_log("Unsupported command");
            return;
            break;
    }
    if (status != BT_STATUS_SUCCESS)
    {
        bdt_log("%s Test 0x%x Failed with status:0x%x", __FUNCTION__, cmd, status);
    }
    return;
}

void bdt_cleanup(void)
{
    bdt_log("CLEANUP");
    sBtInterface->cleanup();
}

/*******************************************************************************
 ** Console commands
 *******************************************************************************/

void do_help(char *p)
{
    int i = 0;
    char line[128];
//    int pos = 0;

    while (console_cmd_list[i].name != NULL)
    {
        snprintf(line, 128,"%s", (char*)console_cmd_list[i].name);
        bdt_log("%s %s\n", (char*)line, (char*)console_cmd_list[i].help);
        i++;
    }
}

void do_quit(char *p)
{
    bdt_shutdown();
}

/*******************************************************************
 *
 *  BT TEST  CONSOLE COMMANDS
 *
 *  Parses argument lists and passes to API test function
 *
*/

void do_init(char *p)
{
    bdt_init();
}

void do_enable(char *p)
{
    bdt_enable();
}

void do_disable(char *p)
{
    bdt_disable();
}
void do_dut_mode_configure(char *p)
{
    bdt_dut_mode_configure(p);
}

void do_le_test_mode(char *p)
{
    bdt_le_test_mode(p);
}

void do_cleanup(char *p)
{
    bdt_cleanup();
}


void do_le_client_register(char *p)
{
    bt_status_t        Ret;
    int Idx;
    tBT_UUID    uuid;
    bt_uuid_t    bt_uuid;

    skip_blanks(&p);
    Idx = atoi(p);

    switch(Idx)
    {
    case 1:
        uuid.len = LEN_UUID_128;
        memcpy(&uuid.uu.uuid128, "\x00\x00\xA0\x0C\x00\x00\x00\x00\x01\x23\x45\x67\x89\xAB\xCD\xEF", 16); //0000A00C-0000-0000-0123-456789ABCDEF
        memcpy(&bt_uuid.uu, "\x00\x00\xA0\x0C\x00\x00\x00\x00\x01\x23\x45\x67\x89\xAB\xCD\xEF", 16); //0000A00C-0000-0000-0123-456789ABCDEF
        break;
    case 2:
        uuid.len = LEN_UUID_128;
        memcpy(&uuid.uu.uuid128, "\x11\x22\xA0\x0C\x00\x00\x00\x00\x01\x23\x45\x67\x89\xAB\xCD\xEF", 16); //1122A00C-0000-0000-0123-456789ABCDEF
        memcpy(&bt_uuid.uu, "\x11\x22\xA0\x0C\x00\x00\x00\x00\x01\x23\x45\x67\x89\xAB\xCD\xEF", 16); //1122A00C-0000-0000-0123-456789ABCDEF
        break;
    default:
        printf("%s:: ERROR: no matching uuid \n", __FUNCTION__);
        return;
    }
    if(Btif_gatt_layer)
    {
        Ret = sGattIfaceScan->client->register_client(&bt_uuid);
        printf("%s:: ret value %d\n", __FUNCTION__,Ret);
    }
    else
    {
        g_client_if = sGattInterface->Register(&uuid, &sGattCB);
        sleep(2);
        sGattInterface->StartIf(g_client_if);
    }
}

void do_le_client_deregister(char *p)
{
    bt_status_t        Ret;

    if(Btif_gatt_layer)
    {
        if(0 == g_client_if_scan)
        {
            printf("%s:: ERROR: no application registered\n", __FUNCTION__);
            return;
        }
        Ret = sGattIfaceScan->client->unregister_client(g_client_if_scan);
        printf("%s:: Ret=%d\n", __FUNCTION__, Ret);
    }
    else
    {
        if(0 == g_client_if)
        {
            printf("%s:: ERROR: no application registered\n", __FUNCTION__);
            return;
        }
        sGattInterface->Deregister(g_client_if);
    }
}

void do_le_client_connect (char *p)
{
    BOOLEAN        Ret = false;
    bt_bdaddr_t bd_addr = {{0}};
    int transport = BT_TRANSPORT_BR_EDR;
    transport = get_int(&p, -1);
    if(FALSE == GetBdAddr(p, &bd_addr))    return;

    if(transport == BT_TRANSPORT_BR_EDR)
    {
        //Outgoing Connection

        //    g_SecLevel |= BTM_SEC_OUT_AUTHENTICATE;
        //     g_SecLevel |= BTM_SEC_OUT_ENCRYPT ;
        g_PSM= 1;
        g_SecLevel = 0;
        printf("g_SecLevel = %d \n", g_SecLevel);
        sL2capInterface->RegisterPsm(g_PSM, g_ConnType, g_SecLevel /*BTM_SEC_IN_AUTHORIZE */);
        sleep(3);

        l2c_connect(&bd_addr);
    }
    else if(Btif_gatt_layer)
    {
        Ret = sGattIfaceScan->client->connect(g_client_if_scan, &bd_addr, TRUE, transport);
    }
    else
    {
        Ret = sGattInterface->Connect(g_client_if, bd_addr.address, TRUE, transport);
    }
    printf("%s:: Ret=%d \n", __FUNCTION__, Ret);
}

void do_le_client_refresh (char *p)
{
    BOOLEAN        Ret;
    bt_bdaddr_t bd_addr = {{0}};
    if(FALSE == GetBdAddr(p, &bd_addr))    return;

    if(Btif_gatt_layer)
    {
        Ret = sGattIfaceScan->client->refresh(g_client_if_scan, &bd_addr);
        printf("%s:: Ret=%d \n", __FUNCTION__, Ret);
    }
}

void do_le_conn_param_update(char *p)
{
    BOOLEAN        Ret;
    bt_bdaddr_t bd_addr = {{0}};
    int min_interval = 24;
    int max_interval = 40;
    int latency = 0;
    int timeout = 2000;
    min_interval =  get_int(&p, -1);
    max_interval =  get_int(&p, -1);
    latency      =  get_int(&p, -1);
    if(!min_interval)
        min_interval = 24;
    if(!max_interval)
        max_interval = 40;
    if(FALSE == GetBdAddr(p, &bd_addr))    return;
    Ret = sGattIfaceScan->client->conn_parameter_update(&bd_addr,min_interval,max_interval,latency,timeout);
    printf("%s:: Ret=%d \n", __FUNCTION__, Ret);

}

void do_le_client_connect_auto (char *p)
{
    BOOLEAN        Ret;
    bt_bdaddr_t bd_addr = {{0}};
    if(FALSE == GetBdAddr(p, &bd_addr))    return;

    if(Btif_gatt_layer)
    {
        Ret = sGattIfaceScan->client->connect(g_client_if_scan, &bd_addr, FALSE,BT_TRANSPORT_LE);
    }
    else
    {
        Ret = sGattInterface->Connect(g_client_if, bd_addr.address, FALSE,BT_TRANSPORT_LE);
    }
    printf("%s:: Ret=%d \n", __FUNCTION__,Ret );
}


void do_le_client_disconnect (char *p)
{
    bt_status_t        Ret;
    bt_bdaddr_t bd_addr = {{0}};
    int transport = BT_TRANSPORT_BR_EDR;
    transport = get_int(&p, -1);
    if(FALSE == GetBdAddr(p, &bd_addr))    return;

    if(transport == BT_TRANSPORT_BR_EDR)
    {
        Ret = sL2capInterface->DisconnectReq(g_lcid);
    }
    else if(Btif_gatt_layer)
    {
        Ret = sGattIfaceScan->client->disconnect(g_client_if_scan, &bd_addr, g_conn_id);
    }
    else
    {
        Ret = sGattInterface->Disconnect(g_conn_id);
    }
    printf("%s:: Ret=%d \n", __FUNCTION__,Ret );
}

void do_le_client_scan_start (char *p)
{
    bt_status_t        Ret;
    Ret = sGattIfaceScan->client->scan(TRUE);
    printf("%s:: Ret=%d \n", __FUNCTION__,Ret );
}

void do_le_client_scan_stop (char *p)
{
    bt_status_t        Ret;
    Ret = sGattIfaceScan->client->scan(FALSE);
    printf("%s:: Ret=%d \n", __FUNCTION__,Ret );
}

void do_le_client_listen_start (char *p)
{
    bt_status_t        Ret;
    Ret = sGattIfaceScan->client->listen(g_client_if_scan,TRUE);
    printf("%s:: Ret=%d \n", __FUNCTION__,Ret );
}

void do_le_client_listen_stop (char *p)
{
    bt_status_t        Ret;
    Ret = sGattIfaceScan->client->listen(g_client_if_scan,FALSE);
    printf("%s:: Ret=%d \n", __FUNCTION__,Ret );
}

void do_le_client_set_adv_data(char *p)
{
    bt_status_t        Ret;
    bool              SetScanRsp        = FALSE;
    bool              IncludeName        = TRUE;
    bool              IncludeTxPower    = FALSE;
    int               min_conn_interval = 100;
    int               max_conn_interval = 1000;

    SetScanRsp         = get_int(&p, -1);  // arg1  Other than zero will be considered as true.
    IncludeName     = get_int(&p, -1);  // arg2  Other than zero will be considered as true.
    IncludeTxPower     = get_int(&p, -1);  // arg3  Other than zero will be considered as true.
    min_conn_interval     = get_int(&p, -1);  // arg3  Other than zero will be considered as true.
    max_conn_interval     = get_int(&p, -1);  // arg3  Other than zero will be considered as true.

    //To start with we are going with hard-code values.
    Ret = sGattIfaceScan->client->set_adv_data(/*g_server_if*/ g_server_if_scan /*g_client_if_scan*/, SetScanRsp, IncludeName, IncludeTxPower, min_conn_interval, max_conn_interval, 0,8, "QUALCOMM", 0, NULL,0,NULL);
    printf("%s:: Ret=%d \n", __FUNCTION__,Ret );
}
void do_le_client_set_adv_mode(char *p)
{
    tBTA_DM_DISC disc_mode;
    tBTA_DM_CONN conn_mode;

    disc_mode = get_int(&p,-1);
    conn_mode = get_int(&p,-1);
    printf("%s:: discoverable  mode=%d  connectable _mode=%d \n", __FUNCTION__,disc_mode,conn_mode );
    sGattInterface->cSetVisibility(disc_mode,conn_mode);

}

void do_le_client_multi_adv_set_inst_data(char *p)
{
    bt_status_t        Ret;
    bool              SetScanRsp        = FALSE;
    bool              IncludeName        = TRUE;
    bool              IncludeTxPower    = TRUE;

    SetScanRsp         = get_int(&p, -1);  // arg1  Other than zero will be considered as true.
    IncludeName     = get_int(&p, -1);  // arg2  Other than zero will be considered as true.
    IncludeTxPower     = get_int(&p, -1);  // arg3  Other than zero will be considered as true.

    //To start with we are going with hard-code values.
    Ret = sGattIfaceScan->client->multi_adv_set_inst_data(g_client_if_scan /*g_client_if_scan*/, SetScanRsp, IncludeName, IncludeTxPower,0,8, "QUALCOMM", 0, NULL,0,NULL);
    printf("%s:: Ret=%d \n", __FUNCTION__,Ret );
}

void do_le_client_adv_update(char *p)
{
    bt_status_t        Ret;
    int               TxPower    = 3;
    int               chnlMap    = 7;
    int               min_interval = 160;
    int               max_interval = 240;
    int               adv_type    = 3 ;//non-connectable undirect
    int               adv_if   =  g_server_if_scan;
    int               timeout_s =   30;

    adv_if       =  get_int(&p, -1);
    min_interval =  get_int(&p, -1);
    max_interval =  get_int(&p, -1);
    adv_type     =  get_int(&p, -1);
    chnlMap      =  get_int(&p, -1);
    TxPower      =  get_int(&p, -1);
    timeout_s    =  get_int(&p, -1);
    //To start with we are going with hard-code values.
    Ret = sGattIfaceScan->client->multi_adv_update(adv_if, min_interval, max_interval,adv_type,chnlMap,TxPower, timeout_s);
    printf("%s:: Ret=%d \n", __FUNCTION__,Ret );
}

void do_le_client_adv_enable(char *p)
{
    bt_status_t       Ret;
    int               TxPower    = 4;
    int               chnlMap    = 7;
    int               min_interval = 30;
    int               max_interval = 60;
    int               adv_type    = 0; //connectable undirect
    int               adv_if   =  g_server_if_scan;
    int               timeout_s = 30;

    adv_if       =  get_int(&p, -1);
    min_interval =  get_int(&p, -1);
    max_interval =  get_int(&p, -1);
    adv_type     =  get_int(&p, -1);
    chnlMap      =  get_int(&p, -1);
    TxPower      =  get_int(&p, -1);
    timeout_s    =  get_int(&p, -1);
    Ret = sGattIfaceScan->client->multi_adv_enable(adv_if,min_interval,max_interval,adv_type,chnlMap,TxPower, timeout_s);
    printf("%s:: Ret=%d \n", __FUNCTION__, Ret);
}

void do_le_client_adv_disable(char *p)
{
    bt_status_t   Ret;
    int           adv_if = g_server_if_scan;

    adv_if = get_int(&p, -1);
    Ret = sGattIfaceScan->client->multi_adv_disable(adv_if);
    printf("%s:: Ret=%d \n", __FUNCTION__, Ret);
}

void do_le_client_configureMTU(char *p)
{
    tGATT_STATUS Ret =0;
    UINT16 mtu = 23;

    printf("%s:: mtu :%d\n", __FUNCTION__, mtu);
    Ret = sGattInterface->cConfigureMTU(g_conn_id, mtu);
    printf("%s:: Ret=%d \n", __FUNCTION__, Ret);
}

void do_le_client_discover(char *p)
{
    int        uuid_len = 0;
    tGATT_STATUS Ret =0;
    tGATT_DISC_PARAM param;
    tGATT_DISC_TYPE disc_type; //GATT_DISC_SRVC_ALL , GATT_DISC_SRVC_BY_UUID

    disc_type = get_int(&p, -1);  // arg1
    param.s_handle = get_hex(&p, -1);  // arg2
    param.e_handle = get_hex(&p, -1);  // arg3

    uuid_len    = get_int(&p, -1);  // arg4 - Size in bits for the uuid (16, 32, or 128)
    if((16==uuid_len) || (32==uuid_len) || (128==uuid_len))
    {
        param.service.len = uuid_len/8;
    }
    else
    {
        printf("%s::ERROR - Invalid Parameter. UUID Len should be either 16/32/128 \n",__FUNCTION__);
        return;
        }

    switch(param.service.len)
    {
        case 2: //16 bit uuid
            param.service.uu.uuid16 = get_hex(&p, -1); // arg5
            break;

        case 4: //32 bit uuid
            param.service.uu.uuid32 = get_hex(&p, -1); // arg5
            break;

        case 16: //128 bit uuid
            *((unsigned int*)&param.service.uu.uuid128[12]) = get_hex(&p, -1);
            *((unsigned int*)&param.service.uu.uuid128[8]) = get_hex(&p, -1);
            *((unsigned int*)&param.service.uu.uuid128[4]) = get_hex(&p, -1);
            *((unsigned int*)param.service.uu.uuid128) = get_hex(&p, -1);    //arg5

            break;
        default:
            printf("%s::ERROR - Invalid Parameter. UUID Len should  \n",__FUNCTION__);
            return;
    }

    printf("%s:: disc_type = %d, uuid=%04x \n", __FUNCTION__, disc_type, param.service.uu.uuid16);

    //if(FALSE == GetDiscType(p, &disc_type))    return;        //TODO - add the function if user input is needed
    Ret = sGattInterface->cDiscover(g_conn_id, disc_type, &param);
    printf("%s:: Ret=%d \n", __FUNCTION__, Ret);
}


void do_le_client_read(char *p)
{
    int i =0;
    int        uuid_len = 0;
    tGATT_STATUS Ret = 0;
    tGATT_READ_TYPE read_type;
    int auth_req;
    tGATT_READ_PARAM readBuf;// = {GATT_AUTH_REQ_NONE, 0x201};

    //Parse and copy command line arguments
    read_type = get_int(&p, -1); // arg2
    auth_req = get_int(&p, -1); // arg2

    switch(read_type)
    {
    case GATT_READ_BY_TYPE:
    case GATT_READ_CHAR_VALUE:

        readBuf.service.auth_req     = auth_req;
        readBuf.service.s_handle     = get_hex(&p, -1);  // arg2
        readBuf.service.e_handle     = get_hex(&p, -1);  // arg3

        uuid_len    = get_int(&p, -1);  // arg4 - Size in bits for the uuid (16, 32, or 128)
        if((16==uuid_len) || (32==uuid_len) || (128==uuid_len))
        {
            readBuf.service.uuid.len = uuid_len/8;
        }
        else
        {
            printf("%s::ERROR - Invalid Parameter. UUID Len should be either 16/32/128 \n",__FUNCTION__);
            return;
        }

        switch(readBuf.service.uuid.len)
        {
            case 2: //16 bit uuid
                readBuf.service.uuid.uu.uuid16 = get_hex(&p, -1); // arg5
                break;

            case 4: //32 bit uuid
                readBuf.service.uuid.uu.uuid32 = get_hex(&p, -1); // arg5
                break;

            case 16: //128 bit uuid
                *((unsigned int*)&readBuf.service.uuid.uu.uuid128[12]) = get_hex(&p, -1);
                *((unsigned int*)&readBuf.service.uuid.uu.uuid128[8]) = get_hex(&p, -1);
                *((unsigned int*)&readBuf.service.uuid.uu.uuid128[4]) = get_hex(&p, -1);
                *((unsigned int*)readBuf.service.uuid.uu.uuid128) = get_hex(&p, -1);    //arg5

                break;
            default:
                printf("%s::ERROR - Invalid Parameter. UUID Len should be either 4/8/32characters, which corresponds <16/32/128> bits \n",__FUNCTION__);
                return;
        }
        break;


    case GATT_READ_BY_HANDLE:
        readBuf.by_handle.handle = get_hex(&p, -1);
        readBuf.by_handle.auth_req = auth_req;
        break;

    case GATT_READ_MULTIPLE:
        readBuf.read_multiple.auth_req = auth_req;
        readBuf.read_multiple.num_handles = get_hex(&p, -1); //arg 2
        if(readBuf.read_multiple.num_handles > 10)
        {
            printf(":: ERROR - invalid param. Max handle value is 10. \n");
            return;
        }
        for(i=0; i<readBuf.read_multiple.num_handles; i++)
        {
            readBuf.read_multiple.handles[i] = get_hex(&p, -1); //arg 3 ... N
        }
        printf("%s:: Read by MultipleHandle \t Number of handles=%04x \n", __FUNCTION__, readBuf.read_multiple.num_handles);
        break;

    case GATT_READ_PARTIAL:
        readBuf.partial.auth_req = auth_req;
        readBuf.partial.handle = get_hex(&p, -1); //arg 2
        readBuf.partial.offset = get_hex(&p, -1); //arg 3
        printf("%s:: Read by Descriptor \t handle=%04x \t offset=%04x \n", __FUNCTION__, readBuf.partial.handle, readBuf.partial.offset);
        break;

    }

    Ret = sGattInterface->cRead(g_conn_id, read_type, &readBuf);
    printf("%s:: Ret=%d \n", __FUNCTION__, Ret);
}

void copy_string(char *dest, char *source)
{
   int i = 2;
   while(i)
   {
      *dest = *source;
      source++;
      dest++;
      i--;
   }
   *dest = '\0';
}

void do_le_client_write(char *p)
{
    int i;
    tGATT_STATUS Ret = 0;
    tGATT_WRITE_TYPE write_type;
    int auth_req = 0;
    tGATT_VALUE writeBuf;// = {GATT_AUTH_REQ_NONE, 0x201};

    write_type = get_int(&p, -1); // arg1
    auth_req = get_int(&p, -1); // arg2

    writeBuf.conn_id = g_conn_id;
    writeBuf.auth_req = auth_req;
    writeBuf.handle     = get_hex(&p, -1);  // arg3
    writeBuf.offset     = get_hex(&p, -1);  //arg4
    writeBuf.len         = get_int(&p, -1); //arg5


    if(writeBuf.len > GATT_MAX_ATTR_LEN )
    {
        printf("%s:: ERROR - invalid param. Max length for Write is 600 \n",__FUNCTION__);
        return;
    }
    memset(&(writeBuf.value[0]), 0, GATT_MAX_ATTR_LEN);
    for (i = 0; i < writeBuf.len; i++)
    {
        writeBuf.value[i] = get_hex_byte(&p, 0);
    }

    Ret = sGattInterface->cWrite(g_conn_id, write_type, &writeBuf);
    printf("%s:: Ret=%d \n", __FUNCTION__, Ret);
}
void do_le_execute_write(char *p)
{
    BOOLEAN is_execute;
    tGATT_STATUS Ret = 0;

    is_execute = get_int(&p, -1); // arg1

    printf("%s:: is_execute=%d \n", __FUNCTION__, is_execute);
    Ret = sGattInterface->cExecuteWrite(g_conn_id, is_execute);
    printf("%s:: Ret=%d \n", __FUNCTION__, Ret);
}
void do_le_set_idle_timeout(char *p)
{
    int idle_timeout;
    bt_bdaddr_t bd_addr = {{0}};
        if(FALSE == GetBdAddr(p, &bd_addr))    return;
    idle_timeout = get_int(&p, -1); //arg2
    sGattInterface->cSetIdleTimeout(bd_addr.address, idle_timeout);

}


/*******************************************************************************
 ** GATT SERVER API commands
 *******************************************************************************/
void do_le_server_register(char *p)
{
    bt_status_t        Ret;
    int Idx;
    tBT_UUID    uuid;
    bt_uuid_t    bt_uuid;
    skip_blanks(&p);
    Idx = atoi(p);
    switch(Idx)
    {
    case 1:
        uuid.len = LEN_UUID_128;
        memcpy(&uuid.uu.uuid128, "\x00\x00\xA0\x0C\x00\x00\x00\x00\x01\x23\x45\x67\x89\xAB\xCD\xEF", 16); //0000A00C-0000-0000-0123-456789ABCDEF
        memcpy(&bt_uuid.uu, "\x00\x00\xA0\x0C\x00\x00\x00\x00\x01\x23\x45\x67\x89\xAB\xCD\xEF", 16); //0000A00C-0000-0000-0123-456789ABCDEF
        break;
    case 2:
        uuid.len = LEN_UUID_128;
        memcpy(&uuid.uu.uuid128, "\x11\x22\xA0\x0C\x00\x00\x00\x00\x01\x23\x45\x67\x89\xAB\xCD\xEF", 16); //1122A00C-0000-0000-0123-456789ABCDEF
        memcpy(&bt_uuid.uu, "\x11\x22\xA0\x0C\x00\x00\x00\x00\x01\x23\x45\x67\x89\xAB\xCD\xEF", 16); //1122A00C-0000-0000-0123-456789ABCDEF
        break;
    default:
        printf("%s:: ERROR: no matching uuid \n", __FUNCTION__);
        return;
    }

    if(Btif_gatt_layer)
    {
        Ret = sGattIfaceScan->server->register_server(&bt_uuid);
        printf("%s:: Ret=%d \n", __FUNCTION__, Ret);
    }
    else
    {
        g_server_if = sGattInterface->Register(&uuid, &sGattCB);
        printf("%s:: g_server_if=%d \n", __FUNCTION__, g_server_if);
    }

}

void do_le_server_deregister(char *p)
{
    bt_status_t        Ret;
    if(0 == g_server_if)
    {
        printf("%s:: ERROR: no application registered\n", __FUNCTION__);
        return;
    }
    sGattInterface->Deregister(g_server_if);
    Ret = sGattIfaceScan->server->unregister_server(g_server_if_scan);
    printf("%s::Ret = %d\n", __FUNCTION__,Ret);
}

void do_le_server_add_service(char *p)
{
    bt_status_t     Ret = 0;

    //Later take this value as cmd line
    btgatt_srvc_id_t    srvc_id;
    memcpy(&srvc_id.id.uuid.uu, "\x00\x00\x18\x00\x00\x00\x10\x00\x80\x00\x00\x80\x5f\x9b\x34\xfb", 16);  //00001800-0000-1000-8000-00805f9b34fb


    srvc_id.id.inst_id    = 1;//
    srvc_id.is_primary    = BTGATT_SERVICE_TYPE_PRIMARY; // BTGATT_SERVICE_TYPE_SECONDARY
    Ret = sGattIfaceScan->server->add_service(g_server_if_scan, &srvc_id, 1/*num_handles*/);
    printf("%s:: Ret=%d \n", __FUNCTION__,Ret );
}

void do_le_server_connect (char *p)
{
    BOOLEAN        Ret;
    bt_bdaddr_t bd_addr = {{0}};
    if(FALSE == GetBdAddr(p, &bd_addr))    return;
    Ret = sGattIfaceScan->server->connect(g_server_if_scan, &bd_addr, TRUE, BT_TRANSPORT_LE);
    printf("%s:: Ret=%d \n", __FUNCTION__,Ret );
}

void do_le_server_connect_auto (char *p)
{
    BOOLEAN        Ret;
    bt_bdaddr_t bd_addr = {{0}};
    if(FALSE == GetBdAddr(p, &bd_addr))    return;
    Ret = sGattIfaceScan->server->connect(g_server_if_scan, &bd_addr, FALSE, BT_TRANSPORT_LE);
    printf("%s:: Ret=%d \n", __FUNCTION__,Ret );
}


void do_le_server_disconnect (char *p)
{
    bt_status_t        Ret;
    bt_bdaddr_t bd_addr = {{0}};
    if(FALSE == GetBdAddr(p, &bd_addr))    return;
    Ret = sGattIfaceScan->server->disconnect(g_server_if_scan, &bd_addr, g_conn_id);
    printf("%s:: Ret=%d \n", __FUNCTION__,Ret );
}

/**************************************************
**L2CAP for BR/EDR conn
***************************************************/



void do_l2cap_init(char *p)
{

    memset(&tl2cap_cfg_info, 0, sizeof(tl2cap_cfg_info));
    memset(&le_conn_info, 0, sizeof(le_conn_info));
    //Use macros for the constants
    tl2cap_cfg_info.mtu_present = TRUE;
    tl2cap_cfg_info.mtu = g_imtu;
    tl2cap_cfg_info.flush_to_present = TRUE;
    tl2cap_cfg_info.flush_to = 0xffff;
    //use other param if needed
    tl2cap_cfg_info.fcr_present = g_Fcr_Present;
    tl2cap_cfg_info.fcr.mode = g_Fcr_Mode;
    tl2cap_cfg_info.fcs = 0;
    tl2cap_cfg_info.fcs_present = 1;
    le_conn_info.init_credits = L2CAP_LE_MAX_CREDIT;
    le_conn_info.le_mtu = L2CAP_LE_DEFAULT_MTU;
    le_conn_info.le_mps = L2CAP_LE_DEFAULT_MPS;
    le_conn_info.le_psm = 0x80; //dynamic le psm starts from 0x80
    if(L2CAP_FCR_ERTM_MODE == tl2cap_cfg_info.fcr.mode)
    {
        tl2cap_cfg_info.fcr = ertm_fcr_opts_def;
    }
    else if(L2CAP_FCR_STREAM_MODE == tl2cap_cfg_info.fcr.mode)
    {
        tl2cap_cfg_info.fcr = stream_fcr_opts_def;
    }
    //Load L2cap Interface
    else if(NULL == sL2capInterface)
    {
        printf("Get L2cap testapp interfaces\n");
        sL2capInterface = sBtInterface->get_testapp_interface(TEST_APP_L2CAP);
    }
    tl2cap_cfg_info.fcr.tx_win_sz = 3;
    //Initialize ERTM Parameters
    t_ertm_info.preferred_mode = g_Fcr_Mode;
    t_ertm_info.allowed_modes = g_Ertm_AllowedMode;
    t_ertm_info.user_rx_buf_size = BT_DEFAULT_BUFFER_SIZE;
    t_ertm_info.user_tx_buf_size = BT_DEFAULT_BUFFER_SIZE;
    t_ertm_info.fcr_rx_buf_size = BT_DEFAULT_BUFFER_SIZE;
    t_ertm_info.fcr_tx_buf_size = BT_DEFAULT_BUFFER_SIZE;
    //Load L2cap Interface
    sL2capInterface->Init(&l2test_l2c_appl);
}


/*
static int l2c_pair(char *p)
{
    bt_bdaddr_t bd_addr = {{0}};
    int transport = BT_TRANSPORT_BR_EDR;
    transport = get_int(&p, -1);
    GetBdAddr(p, &bd_addr);
    if(BT_STATUS_SUCCESS != sBtInterface->create_bond(&bd_addr,transport))
    {
        printf("Failed to Initiate Pairing \n");
        return FALSE;
    }
     sleep(20);
     return TRUE;
}
*/

static UINT16 do_l2cap_connect(bt_bdaddr_t * bd_addr)
{

    if((L2CAP_FCR_STREAM_MODE == g_Fcr_Mode) || (L2CAP_FCR_ERTM_MODE == g_Fcr_Mode)) {
        return sL2capInterface->ErtmConnectReq(g_PSM, bd_addr->address, &t_ertm_info);
    } else {
        return sL2capInterface->Connect(g_PSM, bd_addr);
    }
}

static void l2c_connect(bt_bdaddr_t *bd_addr)
{
    do_l2cap_connect(bd_addr);
}


BOOLEAN do_l2cap_disconnect(char *p)
{
    return sL2capInterface->DisconnectReq(g_lcid);
}

/* LE-L2CAP functionalities */
static t_le_chnl_info *le_allocate_conn_info(UINT16 psm, BOOLEAN is_server)
{
    t_le_chnl_info    *p_le_chnl_info = &le_chnl_conn_info[0];
    UINT16      i;

    for (i = 0; i < MAX_L2CAP_CLIENTS; i++, p_le_chnl_info++)
    {
        if (!p_le_chnl_info->in_use)
        {
            p_le_chnl_info->in_use = TRUE;
            p_le_chnl_info->is_server = is_server;
            p_le_chnl_info->psm    = psm;
            return p_le_chnl_info;
        }
    }
    return (NULL);

}

static t_le_chnl_info *le_get_conn_info(UINT16 psm, BOOLEAN is_server)
{
    t_le_chnl_info    *p_le_chnl_info = &le_chnl_conn_info[0];
    UINT16      i;

    for (i = 0; i < MAX_L2CAP_CLIENTS; i++, p_le_chnl_info++)
    {
        if ((p_le_chnl_info->in_use) && (p_le_chnl_info->psm == psm)
                && (is_server == p_le_chnl_info->is_server))
        {
            return p_le_chnl_info;
        }
    }
    return NULL;

}

static t_le_chnl_info *le_get_conn_info_by_lcid(UINT16 lcid)
{
    t_le_chnl_info    *p_le_chnl_info = &le_chnl_conn_info[0];
    UINT16      i;

    for (i = 0; i < MAX_L2CAP_CLIENTS; i++, p_le_chnl_info++)
    {
        if ((p_le_chnl_info->in_use) && (p_le_chnl_info->lcid == lcid))
        {
            return p_le_chnl_info;
        }
    }
    return NULL;

}

static BOOLEAN le_release_conn_info(t_le_chnl_info *le_conn_info)
{
    if (le_conn_info && (le_conn_info->in_use))
    {
        le_conn_info->in_use = FALSE;
        le_conn_info->is_server = 0;
        le_conn_info->psm    = 0;
        memset(le_conn_info, 0, sizeof(tL2CAP_LE_CONN_INFO));
        return TRUE;
    }
    return FALSE;
}

static void do_start_advertisment(char *p)
{
    tBT_UUID    uuid;
    int option = get_int(&p, -1);
    int start = get_int(&p, -1);

    uuid.len = LEN_UUID_128;
    memcpy(&uuid.uu.uuid128, "\x11\x22\xA0\x0D\x00\x00\x00\x00\x01\x23\x45\x67\x89\xAB\xCD\xEF", 16); //1122A00D-0000-0000-0123-456789ABCDEF

    if ((g_le_coc_if == 0) && option)
        g_le_coc_if = sGattInterface->Register(&uuid, &sGattCB);
    printf("Gatt Registration Done\n");

    if( option)
    {
        if (start == 1)
            status = sGattInterface->Listen(g_le_coc_if, start, NULL);
        else if (start == 0)
            status = sGattInterface->Listen(g_le_coc_if, start, NULL);
        else
        {
            printf("Unknown parameter\n");
            return;
        }
    }
    else
    {
        if (start == 1)
            status = sGattInterface->Listen(g_server_if_scan, start, NULL);
        else if (start == 0)
            status = sGattInterface->Listen(g_server_if_scan, start, NULL);
        else
        {
            printf("Unknown parameter\n");
            return;
        }
    }
    printf("Gatt Listen status is %d\n", status);
}

UINT16 do_le_l2cap_coc_flow_ctrl(char *p)
{
    UINT16 lcid = get_int(&p, -1);

    UINT16 credits = get_int(&p, -1);

    printf("\ndo_le_l2cap_coc_flow_ctrl lcid = %d, credits = %d\n", lcid, credits);
    return sL2capInterface->LeFlowControl(lcid, credits);
}
static void le_l2cap_coc_flow_ctrl(char *p)
{
    printf("In le_l2cap_coc_flow_ctrl\n");
    do_le_l2cap_coc_flow_ctrl(p);
}

UINT16 do_le_l2cap_coc_connect(char *p)
{
    int le_initiator_sec_level;
    UINT16 le_coc_seclevel = 0;
    bt_bdaddr_t bd_addr = {{0}};
    UINT16 le_psm = get_int(&p, -1);


    t_le_chnl_info *le_conn_info = le_allocate_conn_info(le_psm, FALSE);

    if(le_conn_info)
    {
        le_conn_info->loc_conn_info.le_psm = le_psm;
        le_conn_info->loc_conn_info.le_mtu = get_int(&p, -1);
        le_conn_info->loc_conn_info.le_mps = get_int(&p, -1);
        le_conn_info->loc_conn_info.init_credits = get_int(&p, -1);
        le_initiator_sec_level = get_int(&p, -1);
        if(FALSE == GetBdAddr(p, &bd_addr))    return FALSE;
    }
    else
        return FALSE;

    if (le_initiator_sec_level == 0)
    {
        le_coc_seclevel |= BTM_SEC_NONE;
    }
    else if (le_initiator_sec_level == 1)
    {
        le_coc_seclevel |= BTM_SEC_OUT_AUTHENTICATE;
    }
    else if (le_initiator_sec_level == 2)
    {
        le_coc_seclevel |= BTM_SEC_OUT_ENCRYPT;
        le_coc_seclevel |= BTM_SEC_OUT_AUTHENTICATE;
    }
    else
    {
        printf("Security level not supported");
        return FALSE;
    }

    printf("g_SecLevel = %d \n", le_coc_seclevel);
    sL2capInterface->RegisterLePsm(le_conn_info->loc_conn_info.le_psm, TRUE,
            le_coc_seclevel, g_BleEncKeySize);
    sleep(3);
    local_coc_cfg.credits = le_conn_info->loc_conn_info.init_credits;
    local_coc_cfg.mtu = le_conn_info->loc_conn_info.le_mtu;
    local_coc_cfg.mps = le_conn_info->loc_conn_info.le_mps;
    printf("\ndo_l2cap_connect:::::::: psm %d mtu %d mps %d init_credit %d \n",
            le_conn_info->loc_conn_info.le_psm, le_conn_info->loc_conn_info.le_mtu,
            le_conn_info->loc_conn_info.le_mps, le_conn_info->loc_conn_info.init_credits);

    return sL2capInterface->LeConnect(le_conn_info->loc_conn_info.le_psm, ( UINT8 * )&bd_addr.address, &local_coc_cfg);
}

static void le_l2cap_coc_connect(char *svr)
{
    do_le_l2cap_coc_connect(svr);
}

static void le_l2cap_listen(char *p)
{
    int le_rspndr_sec_level;
    UINT16 le_coc_seclevel = 0;
    UINT16 le_psm = get_int(&p, -1);
    t_le_chnl_info *le_conn_info = le_get_conn_info(le_psm, TRUE);

    if(!le_conn_info)
    {
        le_conn_info = le_allocate_conn_info(le_psm, TRUE);
    }
    else
    {
        printf("ALready listening on same channel");
        return;
    }

    if(le_conn_info)
    {
        le_conn_info->loc_conn_info.le_psm = le_psm;
        le_conn_info->loc_conn_info.le_mtu = get_int(&p, -1);
        le_conn_info->loc_conn_info.le_mps = get_int(&p, -1);
        le_conn_info->loc_conn_info.init_credits = get_int(&p, -1);
        le_rspndr_sec_level = get_int(&p, -1);
    }
    else
        return;

    if (le_rspndr_sec_level == 0)
    {
        le_coc_seclevel |= BTM_SEC_NONE;
    }
    else if (le_rspndr_sec_level == 1)
    {
        le_coc_seclevel |= BTM_SEC_IN_AUTHENTICATE;
    }
    else if (le_rspndr_sec_level == 2)
    {
        le_coc_seclevel |= BTM_SEC_IN_ENCRYPT;
        le_coc_seclevel |= BTM_SEC_IN_AUTHENTICATE;
    }
    else
    {
        printf("Security level not supported");
        return ;
    }
    printf("g_SecLevel = %d \n", le_coc_seclevel);

    sL2capInterface->RegisterLePsm(le_conn_info->loc_conn_info.le_psm, FALSE,
                                           le_coc_seclevel, g_BleEncKeySize);

    printf("Waiting for Incoming connection for LE PSM %d... \n",
                              le_conn_info->loc_conn_info.le_psm);
}

UINT8 do_l2cap_DataWrite(UINT16 chnl_id, char *p , UINT32 len)
{
    return sL2capInterface->DataWrite(chnl_id, p, len);
}

static int send_file(char *p)
{
    uint32_t seq = 0, itration = 1;
    int fd, size;
    char filename[] = {0};
    char tmpBuf[LE_ACL_MAX_BUFF_SIZE];
    UINT16 lcid;

    lcid = get_int(&p, -1);
    t_le_chnl_info *le_conn_info = le_get_conn_info_by_lcid(lcid);
    GetFileName(p, filename);

    if(!le_conn_info)
    {
        printf("No conn info, exit \n");
        return FALSE;
    }

    g_omtu =  le_conn_info->rmt_conn_info.le_mtu;

    if(g_omtu < LE_ACL_MAX_BUFF_SIZE)
        data_size = g_omtu;
    else
        data_size = LE_ACL_MAX_BUFF_SIZE;

    printf("data_size(max patload size) = %ld, g_omtu(max ttansmission unit) = %d",
            data_size, g_omtu);

    printf("Filename for input data = %s \n", filename);

    if ((fd = open(filename, O_RDONLY)) < 0)
    {
        printf("Open failed: %s (%d)\n", strerror(errno), errno);
        exit(1);
    }
    while (1)
    {
        while(cong_status)
        {
            usleep(50 * 1000);
        }
        if((size = read(fd, tmpBuf, data_size)) <= 0)
        {
            printf("\n File end ");
            break;
        }
        printf("Sending data :: itration %d, omtu %d, writing data size %d\n",
                itration, g_omtu, size);
        do_l2cap_DataWrite(lcid, tmpBuf, size);
        itration++;
    }

    if (num_frames && g_delay && count && !(seq % count))
        usleep(g_delay);
    return TRUE;
}

static int Send_Data(char *p)
{
    //uint32_t seq =0;
    int send_mode;
    UINT16 lcid;

    lcid = get_int(&p, -1);
    send_mode = get_int(&p, -1);

    char tmpBuffer_1[] = {
        0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,
        0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,
        0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,
        0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,
        0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,
        0x7F
    };
    char tmpBuffer_2[] = {
        0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,
        0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,
        0x7F,0x7F,0x7F
    };

    if(send_mode == 1)  // segmented
    {
        printf("Sending Segmented data...\nData written len %d...\n",
                sizeof(tmpBuffer_1) );
        do_l2cap_DataWrite(lcid, tmpBuffer_1, sizeof(tmpBuffer_1));
    }
    else if( send_mode == 0) // unsegmented
    {
        printf("Sending Unsegmented data...\nData written len %d...\n",
                sizeof(tmpBuffer_2) );
        do_l2cap_DataWrite(lcid, tmpBuffer_2, sizeof(tmpBuffer_2));
    }
    return TRUE;
}

static void send_data_on_le_coc(char *svr)
{
    printf("Sending data on LE L2CAP CoC...\n");
    Send_Data(svr);
}
static void do_send_file(char *svr)
{
    printf("Sending file on LE L2CAP CoC...\n");
    send_file(svr);
}

BOOLEAN le_coc_disc(char *p)
{
    UINT16 cid = get_int(&p, -1);
    return sL2capInterface->DisconnectReq(cid);
}

static void do_le_coc_disconnect(char *p)
{
    le_coc_disc(p);
}

/*******************************************************************************
 ** SMP API commands
 *******************************************************************************/
void do_smp_init(char *p)
{
    sSmpIface->init();
    sleep(1);
    sSmpIface->Register(SMP_cb);
    sleep(1);
}

void do_smp_pair(char *p)
{
    tSMP_STATUS Ret = 0;
    bt_bdaddr_t bd_addr = {{0}};
    if(FALSE == GetBdAddr(p, &bd_addr))    return;
    Ret = sSmpIface->Pair(bd_addr.address);
    printf("%s:: Ret=%d \n", __FUNCTION__, Ret);
}

void do_smp_pair_cancel(char *p)
{
    BOOLEAN Ret = 0;
    bt_bdaddr_t bd_addr = {{0}};
    if(FALSE == GetBdAddr(p, &bd_addr))    return;
    Ret = sSmpIface->PairCancel(bd_addr.address);
    printf("%s:: Ret=%d \n", __FUNCTION__, Ret);
}

void do_smp_security_grant(char *p)
{
    UINT8    res;
    bt_bdaddr_t bd_addr = {{0}};
    if(FALSE == GetBdAddr(p, &bd_addr))    return; //arg1
    res = get_int(&p, -1); // arg2
    sSmpIface->SecurityGrant(bd_addr.address, res);
    printf("%s:: Ret=%d \n", __FUNCTION__,res);
}

void do_smp_passkey_reply(char *p)
{
    UINT32 passkey;
    UINT8    res;
    bt_bdaddr_t bd_addr = {{0}};
    if(FALSE == GetBdAddr(p, &bd_addr))    return; //arg1
        printf("get res value\n");
    res = get_int(&p, -1); // arg2
        printf("res value=%d\n", res);
    passkey = get_int(&p, -1); // arg3
        printf("passkey value=%d\n", passkey);
    sSmpIface->PasskeyReply(bd_addr.address, res, passkey);
    printf("%s:: Ret=%d \n", __FUNCTION__,res);
}

void do_smp_encrypt(char *p)
{
    BOOLEAN Ret = 0;
    UINT8    res;
    bt_bdaddr_t bd_addr = {{0}};
    if(FALSE == GetBdAddr(p, &bd_addr))    return; //arg1
    res = get_int(&p, -1); // arg2
    printf("%s:: res =%d Ret=%d \n", __FUNCTION__,res, Ret);
}

void do_le_gap_conn_param_update(char *p)
{
    //attr_uuid = get_int(&p, -1);
    tGAP_BLE_ATTR_VALUE attr_value;
    attr_value.conn_param.int_min = 50;
    attr_value.conn_param.int_max = 70;
    attr_value.conn_param.latency = 0;
    attr_value.conn_param.sp_tout = 10;
    bt_bdaddr_t bd_addr = {{0}};
    if(FALSE == GetBdAddr(p, &bd_addr))    return; //arg1
    //attr_uuid = get_hex(&p, -1);
    //L2CA_UpdateBleConnParams(bd_addr.address, 50, 70, 0, 1000);
    printf("stage 1\n");
    sGapInterface->Gap_BleAttrDBUpdate(bd_addr.address, attr_value.conn_param.int_min, attr_value.conn_param.int_max,attr_value.conn_param.latency ,attr_value.conn_param.sp_tout);
    printf("%s:: GAP connection parameter Update\n", __FUNCTION__);

}
void do_le_gap_attr_init(char *p)
{
    sGapInterface->Gap_AttrInit();
    printf("%s:: GAP Initialization\n", __FUNCTION__);

}
/*
void do_le_gap_set_disc(char *p)
{
    UINT16 Ret = 0;
    UINT16 mode;
    UINT16 duration;
    UINT16 interval;

    mode = get_int(&p, -1);
    if(1 == mode)         mode = GAP_NON_DISCOVERABLE;
    else if(2 == mode)  mode = GAP_LIMITED_DISCOVERABLE;
    else                mode = GAP_GENERAL_DISCOVERABLE;

    duration = get_int(&p, -1);
    if((12 > duration) || (duration > 1000))    duration = 0; //if 0 is passed, stack will take 12 as default

    interval = get_int(&p, -1);
    if((12 > interval) || (interval > 1000))    interval = 0; //if 0 is passed, stack will take 800 as default



    sGapInterface->Gap_SetDiscoverableMode(mode, duration, interval);
    printf("%s:: Ret=%d\n", __FUNCTION__, Ret);
}

void do_le_gap_set_conn(char *p)
{
    UINT16 Ret=0;
    UINT16 mode;
    UINT16 duration;
    UINT16 interval;

    mode = get_int(&p, -1);
    if(1 == mode)
           mode = GAP_NON_CONNECTABLE;
    else
           mode = GAP_CONNECTABLE;

    duration = get_int(&p, -1);
        if((12 > duration) || (duration > 1000))    duration = 0; //if 0 is passed, stack will take 12 as default

    interval = get_int(&p, -1);
    if((12 > interval) || (interval > 1000))    interval = 0; //if 0 is passed, stack will take 800 as default

    sGapInterface->Gap_SetConnectableMode(mode, duration, interval);
    printf("%s:: Ret=%d\n", __FUNCTION__, Ret);
}
*/
void do_l2cap_send_data_cid(char *p)
{
    UINT16        cid        = 0;
    BT_HDR        bt_hdr;
    UINT16         Ret = 0;
    bt_bdaddr_t bd_addr = {{0}};
    if(FALSE == GetBdAddr(p, &bd_addr))    return; //arg1
    cid = get_int(&p, -1); // arg2

    bt_hdr.event     = 0;
    bt_hdr.len         = 1;
    bt_hdr.offset     = 0;
    bt_hdr.layer_specific = 0;


    Ret = sL2capInterface->SendFixedChnlData(cid, bd_addr.address, &bt_hdr);
    printf("%s:: Ret=%d \n", __FUNCTION__, Ret);

}
/*******************************************************************
 *
 *  CONSOLE COMMAND TABLE
 *
*/

const t_cmd console_cmd_list[] =
{
    /*
     * INTERNAL
     */

    { "help", do_help, "lists all available console commands", 0 },
    { "quit", do_quit, "", 0},

    /*
     * API CONSOLE COMMANDS
     */

     /* Init and Cleanup shall be called automatically */
    { "enable", do_enable, ":: enables bluetooth", 0 },
    { "disable", do_disable, ":: disables bluetooth", 0 },
    { "dut_mode_configure", do_dut_mode_configure, ":: DUT mode - 1 to enter,0 to exit", 0 },
    { "c_register", do_le_client_register, "::UUID: 1<1111..> 2<12323..> 3<321111..>", 0 },
    { "c_deregister", do_le_client_deregister, "::UUID: 1<1111..> 2<12323..> 3<321111..>", 0 },
    { "c_connect", do_le_client_connect, ":: transport-type<0,1...> , BdAddr<00112233445566>", 0 },
    { "c_refresh", do_le_client_refresh, ":: BdAddr<00112233445566>", 0 },
    { "c_conn_param_update", do_le_conn_param_update, ":: int min_interval, int max_interval,int latency, BdAddr<00112233445566>", 0 },
    { "c_connect_auto", do_le_client_connect_auto, ":: BdAddr<00112233445566>", 0 },
    { "c_disconnect", do_le_client_disconnect, ":: BdAddr<00112233445566>", 0 },
    { "c_configureMTU", do_le_client_configureMTU, ":: 23", 0 },
    { "c_discover", do_le_client_discover, "type(1-PrimaryService, 2-PrimaryService using UUID, 3-Included Service, 4-Characteristic, 5-Characteristic Descriptor) \
                                            \n\t s.handle(hex) e.handle(hex) UUIDLen(16/32/128) UUID(hex)", 0 },
    { "c_read", do_le_client_read, "Type(1-ByType, 2-ByHandle, 3-ByMultiple, 4-CharValue, 5-Partial (blob)) Auth_Req \
                                    \n\t ByType       :: s.handle(hex) e.handle(hex) UUIDLen(16/32/128) UUID(hex) \
                                    \n\t ByHandle     :: Handle(hex) \
                                    \n\t ByMultiple   :: NumOfHandle<1-10> Handle_1(hex) Handle_2(hex) ... Handle_N(hex) \
                                    \n\t CharValue    :: s.handle(hex) e.handle(hex) UUIDLen(16/32/128) UUID(hex) \
                                    \n\t Partial/Blob :: Handle(hex) Offset(hex)", 0 },
    { "c_write", do_le_client_write, "Type(1-No response, 2-write, 3-prepare write), Auth_req, Handle, Offset, Len(0-600), Value(hex)", 0 },
    { "c_execute_write", do_le_execute_write, "is_execute", 0 },
    { "c_scan_start", do_le_client_scan_start, "::", 0 },
    { "c_scan_stop", do_le_client_scan_stop, "::", 0 },
    { "c_listen_start", do_le_client_listen_start, "::", 0 },
    { "c_listen_stop", do_le_client_listen_stop, "::", 0 },
    { "c_set_adv_mode", do_le_client_set_adv_mode, ":: Discoverability mode,Connectable_mode,", 0 },
    { "c_set_adv_data", do_le_client_set_adv_data, "::EnableScanrsp<0/1>, IncludeName<0/1> IncludeTxPower<0/1>,min_conn_interval,int  max_conn_interval", 0 },
    { "c_set_multi_adv_data", do_le_client_multi_adv_set_inst_data, "::EnableScanrsp<0/1>, IncludeName<0/1> IncludeTxPower<0/1>", 0 },
    { "start_advertising", do_le_client_adv_enable, "::int client_if,int min_interval,int max_interval,int adv_type,int chnl_map, int tx_power timeout",0},
    { "c_adv_update", do_le_client_adv_update, "::int client_if, int min_interval,int max_interval,int adv_type,int chnl_map, int tx_power, int timeout",0},
    { "stop_advertising", do_le_client_adv_disable, "::int adv_if",0},
    { "c_set_idle_timeout", do_le_set_idle_timeout, "bd_addr, time_out(int)", 0 },
    { "c_gap_attr_init", do_le_gap_attr_init, "::", 0 },
    { "c_gap_conn_param_update", do_le_gap_conn_param_update, "::", 0 },

    { "s_register", do_le_server_register, "::UUID: 1<1111..> 2<12323..> 3<321111..>", 0 },
    { "s_connect", do_le_server_connect, ":: BdAddr<00112233445566>", 0 },
    { "s_connect_auto", do_le_server_connect_auto, ":: BdAddr<00112233445566>", 0 },
    { "s_disconnect", do_le_server_disconnect, ":: BdAddr<00112233445566>", 0 },
    { "s_add_service", do_le_server_add_service, "::", 0 },

    { "pair", do_pairing, ":: BdAddr<00112233445566>", 0 },

    { "smp_init", do_smp_init, "::", 0 }, //Here itself we will register.
    { "smp_pair", do_smp_pair, ":: BdAddr<00112233445566>", 0 },
    { "smp_pair_cancel", do_smp_pair_cancel, ":: BdAddr<00112233445566>", 0 },
    { "smp_security_grant", do_smp_security_grant, ":: BdAddr<00112233445566>, res<>", 0 },
    { "smp_passkey_reply", do_smp_passkey_reply, ":: BdAddr<00112233445566>, res<>, passkey<>", 0 },
    //{ "smp_encrypt", do_smp_encrypt, "::", 0 },
    { "l2cap_send_data_cid", do_l2cap_send_data_cid, ":: BdAddr<00112233445566>, CID<>", 0 },

    { "set_local_name", do_set_localname, ":: setName<name>", 0 },
    /* LE-L2CAP cmds */
    { " ", NULL, "\n\t\t\033[0m\033[34mLE L2CAP CoC Commands\033[0m", 0 },
    { " ", NULL, "\033[0m\033[34mCommands\t\t\tParameters\033[0m", 0 },

    { "start_adv", do_start_advertisment, "\t\t::\tuuid [0 - register"\
        "none (or) 1 - register uuid] , \n\t\t\t\tflag [0- stop adv (or) "\
        "1 - start adv] \n  " , 0},

    { "le_l2cap_listen", le_l2cap_listen, "\t::\tle_psm [1 to 255], "\
        "\n\t\t\t\tle_mtu [23 to 65535], \n\t\t\t\tle_mps [23 to 65533],"\
        " \n\t\t\t\tinit_credits [0 to 65535], \n\t\t\t\tsec_level "\
        "[0 - None, 1 - Authentication, 2 - Auth and Encryption]", 0},

    { "le_l2cap_coc_connect", le_l2cap_coc_connect, "\t::\tle_psm [128 to 255],"\
        "\n\t\t\t\tle_mtu [23 to 65535], \n\t\t\t\tle_mps [23 to 65533], "\
        "\n\t\t\t\tinit_credits [0 to 65535], \n\t\t\t\tsec_level [0 - None,"\
        " 1 - Authentication, 2 - Encryption], \n\t\t\t\tbd_addr [001122334455] ", 0},

    { "le_l2cap_coc_flow_ctrl", le_l2cap_coc_flow_ctrl, "\t::\tchnl_id [chnl id"\
        "info from conn_ind or conn_cnf], \n\t\t\t\tcredits [1 to 65535]", 0},

    { "send_data_on_le_coc", send_data_on_le_coc, "\t::\tchnl_id [chnl id info "\
        "from conn_ind or conn_cnf] , \n\t\t\t\tdata_type [0 - Unsegmented data ,"\
        "1 - Segmented data]", 0},

    { "send_file", do_send_file, "\t\t::\tchnl_id [chnl_id info from conn_ind"\
        "or conn_cnf], \n\t\t\t\tfile_name", 0},

    { "le_coc_disconnect", do_le_coc_disconnect, "\t::\tchnl_id [chnl_id info"\
        "from conn_ind or conn_cnf]", 0},
    /* add here */

    /* last entry */
    {NULL, NULL, "", 0},
};

/*
 * Main console command handler
*/

static void process_cmd(char *p, unsigned char is_job)
{
    char cmd[2048];
    int i = 0;
    bt_pin_code_t pincode;
    char *p_saved = p;

    get_str(&p, cmd);

    /* table commands */
    while (console_cmd_list[i].name != NULL)
    {
        if (is_cmd(console_cmd_list[i].name))
        {
            if (!is_job && console_cmd_list[i].is_job)
                create_cmdjob(p_saved);
            else
            {
                console_cmd_list[i].handler(p);
            }
            return;
        }
        i++;
    }
    //pin key
    if(cmd[6] == '.') {
        for(i=0; i<6; i++) {
            pincode.pin[i] = cmd[i];
        }
        pincode.pin[i] = '\0';
        if(BT_STATUS_SUCCESS != sBtInterface->pin_reply(remote_bd_address, TRUE, strlen((const char*)pincode.pin), &pincode)) {
            printf("Pin Reply failed\n");
        }
        else
            printf("Pin Reply done and paired\n");
        //flush the char for pinkey
        cmd[6] = 0;
    }
    else {
        bdt_log("%s : unknown command\n", p_saved);
        do_help(NULL);
    }
}

int main (int argc, char * argv[])
{
    static btgatt_callbacks_t    sGatt_cb = {sizeof(btgatt_callbacks_t), &sGattClient_cb, &sGattServer_cb};

    config_permissions();
    bdt_log("\n:::::::::::::::::::::::::::::::::::::::::::::::::::");
    bdt_log(":: Bluedroid test app starting");

    if ( HAL_load() < 0 ) {
        perror("HAL failed to initialize, exit\n");
        unlink(PID_FILE);
        exit(0);
    }

    setup_test_env();

    /* Automatically perform the init */
    bdt_init();
    sleep(5);
    bdt_enable();
    sleep(5);
    bdt_log("Get SMP IF BT Interface = %x \n", sBtInterface);
    sGattInterface   = sBtInterface->get_testapp_interface(TEST_APP_GATT);
    sSmpIface        = sBtInterface->get_testapp_interface(TEST_APP_SMP);
    bdt_log("Get GAP IF");
    sGapInterface    = sBtInterface->get_testapp_interface(TEST_APP_GAP);

    bdt_log("Get GATT IF");
    sGattIfaceScan     = sBtInterface->get_profile_interface(BT_PROFILE_GATT_ID);

    bdt_log("Get L2CAP IF");
    sL2capInterface        = sBtInterface->get_testapp_interface(TEST_APP_L2CAP);

    sGattIfaceScan->init(&sGatt_cb);
    bdt_log("GATT IF INIT Done");

    printf("\n Before l2cap init\n");
    do_l2cap_init(NULL);
    printf("\n after l2cap init\n");

    while(!main_done)
    {
        char line[2048];

        /* command prompt */
        printf( ">" );
        fflush(stdout);

        fgets (line, 2048, stdin);

        if (line[0]!= '\0')
        {
            /* remove linefeed */
            line[strlen(line)-1] = 0;

            process_cmd(line, 0);
            memset(line, '\0', 2048);
        }
    }

    /* FIXME: Commenting this out as for some reason, the application does not exit otherwise*/
    //bdt_cleanup();

    HAL_unload();

    bdt_log(":: Bluedroid test app terminating");

    return 0;
}
int GetFileName(char *p, char *filename)
{
//    uint8_t  i;
    int len;

    skip_blanks(&p);

    printf("Input file name = %s\n", p);

    if (p == NULL)
    {
        printf("\nInvalid File Name... Please enter file name\n");
        return FALSE;
    }
    len = strlen(p);

    memcpy(filename, p, len);
    filename[len] = '\0';

    return TRUE;
}
int GetBdAddr(char *p, bt_bdaddr_t *pbd_addr)
{
    char Arr[13] = {0};
    char *pszAddr = NULL;
    uint8_t k1 = 0;
    uint8_t k2 = 0;
    uint8_t  i;

    skip_blanks(&p);

    printf("Input=%s\n", p);

    if(12 > strlen(p))
    {
        printf("\nInvalid Bd Address. Format[112233445566]\n");
        return FALSE;
    }
    memcpy(Arr, p, 12);

    for(i=0; i<12; i++)
    {
        Arr[i] = tolower(Arr[i]);
    }
    pszAddr = Arr;

    for(i=0; i<6; i++)
    {
        k1 = (uint8_t) ( (*pszAddr >= 'a') ? ( 10 + (uint8_t)( *pszAddr - 'a' )) : (*pszAddr - '0') );
        pszAddr++;
        k2 = (uint8_t) ( (*pszAddr >= 'a') ? ( 10 + (uint8_t)( *pszAddr - 'a' )) : (*pszAddr - '0') );
        pszAddr++;

        if ( (k1>15)||(k2>15) )
        {
            return FALSE;
        }
        pbd_addr->address[i] = (k1<<4 | k2);
    }
    return TRUE;
}
#endif //TEST_APP_INTERFACE
