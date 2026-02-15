/******************************************************************************
 *
 *  Copyright (C) 2008-2012 Broadcom Corporation
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
 *  This is the implementation for the audio/video registration module.
 *
 ******************************************************************************/

#include <string.h>
#include "bta_ar_api.h"
#include "bta_ar_int.h"
#include "btm_api.h"
#include "bta_ar_int_ext.h"

/* AV control block */
#if BTA_DYNAMIC_MEMORY == FALSE
tBTA_AR_CB  bta_ar_cb;
#endif

/*******************************************************************************
**
** Function         bta_ar_id
**
** Description      This function maps sys_id to ar id mask.
**
** Returns          void
**
*******************************************************************************/
static UINT8 bta_ar_id(tBTA_SYS_ID sys_id)
{
    UINT8   mask = 0;
    if (sys_id == BTA_ID_AV)
    {
        mask = BTA_AR_AV_MASK;
    }
    else if (sys_id == BTA_ID_AVK)
    {
        mask = BTA_AR_AVK_MASK;
    }

    return mask;
}

/*******************************************************************************
**
** Function         bta_ar_init
**
** Description      This function is called to register to AVDTP.
**
** Returns          void
**
*******************************************************************************/
void bta_ar_init(void)
{
    /* initialize control block */
    memset(&bta_ar_cb, 0, sizeof(tBTA_AR_CB));
    bta_ar_ext_init();
}

/*******************************************************************************
**
** Function         bta_ar_reg_avdt
**
** Description      This function is called to register to AVDTP.
**
** Returns          void
**
*******************************************************************************/
static void bta_ar_avdt_cback(UINT8 handle, BD_ADDR bd_addr, UINT8 event, tAVDT_CTRL *p_data)
{
    /* route the AVDT registration callback to av or avk */
    // TODO: SRC_SNK: add cod here
    DEV_CLASS device_class;
    UINT16 service_class; UINT8 major_class, minor_class;
    BOOLEAN sink_only_device = false;
    BOOLEAN src_only_device = false;
    BTM_GetCOD(bd_addr, device_class);
    UINT8 sep_type; UINT8 set_config_sep_type;
    APPL_TRACE_DEBUG("%s BD_ADDR [%x] [%x] [%x] [%x] [%x] [%x] event %d COD = [%x][%x][%x]", __FUNCTION__,
        bd_addr[0], bd_addr[1], bd_addr[2], bd_addr[3],bd_addr[4], bd_addr[5], event,
        device_class[2],device_class[1],device_class[0]);
    BTM_COD_MINOR_CLASS(minor_class, device_class);
    BTM_COD_MAJOR_CLASS(major_class, device_class);
    BTM_COD_SERVICE_CLASS(service_class, device_class);
    sink_only_device = (service_class & BTM_COD_SERVICE_RENDERING) && !(service_class & BTM_COD_SERVICE_CAPTURING);
    src_only_device = (service_class & BTM_COD_SERVICE_CAPTURING) && !(service_class & BTM_COD_SERVICE_RENDERING);
    APPL_TRACE_DEBUG(" %s Major_Class [%x], Minor Class [%x], Service_Class [%x], src_only %d, sink_only %d",__FUNCTION__,
         major_class, minor_class, service_class, src_only_device, sink_only_device);

    switch(event)
    {
    case AVDT_CONNECT_IND_EVT:
        if(src_only_device)
        {
            /* Remote is a src only device, send indication only to avk */
            if (bta_ar_cb.p_avk_conn_cback)
            {
                update_avdtp_connection_info(bd_addr, AVDT_AR_EXT_CONNECT_IND_EVT, BTA_AR_EXT_AVK_MASK);
                APPL_TRACE_DEBUG(" %s Calling AVK Conn Cback ", __FUNCTION__);
                (*bta_ar_cb.p_avk_conn_cback)(handle, bd_addr, event, p_data);
            }
            break;
        }
        if(sink_only_device)
        {
            /* Remote is a sink only device, send indication only to av */
            if (bta_ar_cb.p_av_conn_cback)
            {
                update_avdtp_connection_info(bd_addr, AVDT_AR_EXT_CONNECT_IND_EVT, BTA_AR_EXT_AV_MASK);
                APPL_TRACE_DEBUG(" %s Calling AV Conn Cback ", __FUNCTION__);
                (*bta_ar_cb.p_av_conn_cback)(handle, bd_addr, event, p_data);
            }
            break;
        }
        /* didn't fall in any of the above condition, send it to both av and avk */
        if (bta_ar_cb.p_av_conn_cback && bta_av_is_scb_available())
        {
            update_avdtp_connection_info(bd_addr, AVDT_AR_EXT_CONNECT_IND_EVT, BTA_AR_EXT_AV_MASK);
            APPL_TRACE_DEBUG(" %s Calling AV Conn Cback ", __FUNCTION__);
            (*bta_ar_cb.p_av_conn_cback)(handle, bd_addr, event, p_data);
        }
        if (bta_ar_cb.p_avk_conn_cback && bta_avk_is_scb_available())
        {
            update_avdtp_connection_info(bd_addr, AVDT_AR_EXT_CONNECT_IND_EVT, BTA_AR_EXT_AVK_MASK);
            APPL_TRACE_DEBUG(" %s Calling AVK Conn Cback ", __FUNCTION__);
            (*bta_ar_cb.p_avk_conn_cback)(handle, bd_addr, event, p_data);
        }
        break;
    case AVDT_DISCONNECT_IND_EVT:
        sep_type = get_remote_sep_type(bd_addr);
        if(sep_type & BTA_AR_EXT_AV_MASK)
        {
            /* connection ind was sent to AV earlier */
            if (bta_ar_cb.p_av_conn_cback)
            {
                APPL_TRACE_DEBUG(" %s Calling AV DiscConn Cback ", __FUNCTION__);
                (*bta_ar_cb.p_av_conn_cback)(handle, bd_addr, event, p_data);
            }
        }
        if (sep_type & BTA_AR_EXT_AVK_MASK)
        {
            /* connection ind was sent to AVK earlier */
            if (bta_ar_cb.p_avk_conn_cback)
            {
                APPL_TRACE_DEBUG(" %s Calling AVK DisConn Cback ", __FUNCTION__);
                (*bta_ar_cb.p_avk_conn_cback)(handle, bd_addr, event, p_data);
            }
        }
        break;
    case AVDT_SETCONFIG_CMD_EVT:
        set_config_sep_type = p_data->setconf_cmd_ind.sep_configured;
        sep_type = get_remote_sep_type(bd_addr);
        APPL_TRACE_DEBUG(" %s current_sep_mask %d  set_config_set_type %d",__FUNCTION__
                ,sep_type, set_config_sep_type);
        if ((set_config_sep_type == AVDT_TSEP_SNK) && (sep_type & BTA_AR_EXT_AVK_MASK))
        {
            /* setconfig was done on SINK SEP */
            if (sep_type & BTA_AR_EXT_AV_MASK)
            {
                /* Connect ind was send to AV earlier, send disc to AV now. */
                if (bta_ar_cb.p_av_conn_cback)
                {
                    APPL_TRACE_DEBUG(" %s fake AV DiscConn Cback after AVK setconfig",__FUNCTION__);
                    /* remove AV Mask */
                    update_avdtp_connection_info(bd_addr, AVDT_AR_EXT_DISCONNECT_IND_EVT, BTA_AR_EXT_AV_MASK);
                    (*bta_ar_cb.p_av_conn_cback)(handle, bd_addr, AVDT_DISCONNECT_IND_EVT, p_data);
                }
            }
        }
        if ((set_config_sep_type == AVDT_TSEP_SRC) && (sep_type & BTA_AR_EXT_AV_MASK))
        {
            /* setconfig was done on SRC SEP */
            if (sep_type & BTA_AR_EXT_AVK_MASK)
            {
                /* Connect ind was send to AVK earlier, send disc to AVK now. */
                if (bta_ar_cb.p_avk_conn_cback)
                {
                    APPL_TRACE_DEBUG(" %s fake AVK DiscConn Cback after AV setconfig", __FUNCTION__);
                    /* remove AVK Mask */
                    update_avdtp_connection_info(bd_addr, AVDT_AR_EXT_DISCONNECT_IND_EVT, BTA_AR_EXT_AVK_MASK);
                    (*bta_ar_cb.p_avk_conn_cback)(handle, bd_addr, AVDT_DISCONNECT_IND_EVT, p_data);
                }
            }
        }
        break;
        /* for all other messages */
    default:
        if (bta_ar_cb.p_av_conn_cback)
        {
            APPL_TRACE_DEBUG(" %s Calling AV Conn Cback ", __FUNCTION__);
            (*bta_ar_cb.p_av_conn_cback)(handle, bd_addr, event, p_data);
        }
        if (bta_ar_cb.p_avk_conn_cback)
        {
            APPL_TRACE_DEBUG(" %s Calling AVK Conn Cback ", __FUNCTION__);
            (*bta_ar_cb.p_avk_conn_cback)(handle, bd_addr, event, p_data);
        }
        break;
    }
}

/*******************************************************************************
**
** Function         bta_ar_reg_avdt
**
** Description      AR module registration to AVDT.
**
** Returns          void
**
*******************************************************************************/
void bta_ar_reg_avdt(tAVDT_REG *p_reg, tAVDT_CTRL_CBACK *p_cback, tBTA_SYS_ID sys_id)
{
    UINT8   mask = 0;

    if (sys_id == BTA_ID_AV)
    {
        bta_ar_cb.p_av_conn_cback = p_cback;
        mask = BTA_AR_AV_MASK;
    }
    else if (sys_id == BTA_ID_AVK)
    {
        bta_ar_cb.p_avk_conn_cback = p_cback;
        mask = BTA_AR_AVK_MASK;
    }
#if (BTA_AR_DEBUG == TRUE)
    else
    {
        APPL_TRACE_ERROR("bta_ar_reg_avdt: the registration is from wrong sys_id:%d", sys_id);
    }
#endif

    if (mask)
    {
        if (bta_ar_cb.avdt_registered == 0)
        {
            AVDT_Register(p_reg, bta_ar_avdt_cback);
        }
        bta_ar_cb.avdt_registered |= mask;
    }
}

/*******************************************************************************
**
** Function         bta_ar_dereg_avdt
**
** Description      This function is called to de-register from AVDTP.
**
** Returns          void
**
*******************************************************************************/
void bta_ar_dereg_avdt(tBTA_SYS_ID sys_id)
{
    UINT8   mask = 0;

    if (sys_id == BTA_ID_AV)
    {
        bta_ar_cb.p_av_conn_cback = NULL;
        mask = BTA_AR_AV_MASK;
    }
    else if (sys_id == BTA_ID_AVK)
    {
        bta_ar_cb.p_avk_conn_cback = NULL;
        mask = BTA_AR_AVK_MASK;
    }
    bta_ar_cb.avdt_registered &= ~mask;

    if (bta_ar_cb.avdt_registered == 0)
        AVDT_Deregister();
}

/*******************************************************************************
**
** Function         bta_ar_avdt_conn
**
** Description      This function is called to let ar know that some AVDTP profile
**                  is connected for this sys_id.
**                  If the other sys modules started a timer for PENDING_EVT,
**                  the timer can be stopped now.
**
** Returns          void
**
*******************************************************************************/
void bta_ar_avdt_conn(tBTA_SYS_ID sys_id, BD_ADDR bd_addr)
{
    UINT8       event = BTA_AR_AVDT_CONN_EVT;
    tAVDT_CTRL  data;

    if (sys_id == BTA_ID_AV)
    {
        if (bta_ar_cb.p_avk_conn_cback)
        {
            (*bta_ar_cb.p_avk_conn_cback)(0, bd_addr, event, &data);
        }
    }
    else if (sys_id == BTA_ID_AVK)
    {
        if (bta_ar_cb.p_av_conn_cback)
        {
            (*bta_ar_cb.p_av_conn_cback)(0, bd_addr, event, &data);
        }
    }
}

/*******************************************************************************
**
** Function         bta_ar_reg_avct
**
** Description      This function is called to register to AVCTP.
**
** Returns          void
**
*******************************************************************************/
void bta_ar_reg_avct(UINT16 mtu, UINT16 mtu_br, UINT8 sec_mask, tBTA_SYS_ID sys_id)
{
    UINT8   mask = bta_ar_id (sys_id);

    if (mask)
    {
        if (bta_ar_cb.avct_registered == 0)
        {
            AVCT_Register(mtu, mtu_br, sec_mask);
        }
        bta_ar_cb.avct_registered |= mask;
    }
}

/*******************************************************************************
**
** Function         bta_ar_dereg_avct
**
** Description      This function is called to deregister from AVCTP.
**
** Returns          void
**
*******************************************************************************/
void bta_ar_dereg_avct(tBTA_SYS_ID sys_id)
{
    UINT8   mask = bta_ar_id (sys_id);

    bta_ar_cb.avct_registered &= ~mask;

    if (bta_ar_cb.avct_registered == 0)
        AVCT_Deregister();
}

/******************************************************************************
**
** Function         bta_ar_reg_avrc
**
** Description      This function is called to register an SDP record for AVRCP.
**
** Returns          void
**
******************************************************************************/
void bta_ar_reg_avrc(UINT16 service_uuid, char *service_name, char *provider_name,
                     UINT16 categories, tBTA_SYS_ID sys_id, BOOLEAN browse_supported,
                     UINT16 profile_version)
{
    UINT8   mask = bta_ar_id (sys_id);
    UINT8   temp[8], *p;

    if (!mask || !categories)
        return;

    if (service_uuid == UUID_SERVCLASS_AV_REM_CTRL_TARGET)
    {
        if (bta_ar_cb.sdp_tg_handle == 0)
        {
            bta_ar_cb.tg_registered = mask;
            bta_ar_cb.sdp_tg_handle = SDP_CreateRecord();
            AVRC_AddRecord(service_uuid, service_name, provider_name, categories,
                           bta_ar_cb.sdp_tg_handle, browse_supported, profile_version);
            bta_sys_add_uuid(service_uuid);
        }
        else
        {
            /* multiple TG registration is allowed since we support both
               A2dp SRC+SNK simultaneously. Change supported categories for
               the second registration since it will overwrite the categories
               for merged SDP record of TG.*/
            p = temp;
            categories = AVRC_SUPF_TG_CAT2 | AVRC_SUPF_TG_CAT1;
            UINT16_TO_BE_STREAM(p, categories);
            SDP_AddAttribute(bta_ar_cb.sdp_tg_handle, ATTR_ID_SUPPORTED_FEATURES, UINT_DESC_TYPE,
                      (UINT32)2, (UINT8*)temp);
        }
    }
    else if ((service_uuid == UUID_SERVCLASS_AV_REMOTE_CONTROL)||
             (service_uuid == UUID_SERVCLASS_AV_REM_CTRL_CONTROL))
    {
        bta_ar_cb.ct_categories [mask - 1] = categories;
        categories = bta_ar_cb.ct_categories[0]|bta_ar_cb.ct_categories[1];
        if (bta_ar_cb.sdp_ct_handle == 0)
        {
            bta_ar_cb.sdp_ct_handle = SDP_CreateRecord();
            AVRC_AddRecord(service_uuid, service_name, provider_name, categories,
                           bta_ar_cb.sdp_ct_handle, browse_supported, profile_version);
            bta_sys_add_uuid(service_uuid);
        }
        else
        {
            /* multiple CT registration is allowed since we support both
               A2dp SRC+SNK simultaneously. Change supported categories for
               the second registration since it will overwrite the categories
               for merged SDP record of CR.*/
            p = temp;
            categories = AVRC_SUPF_CT_CAT2 | AVRC_SUPF_CT_CAT1;
            UINT16_TO_BE_STREAM(p, categories);
            SDP_AddAttribute(bta_ar_cb.sdp_ct_handle, ATTR_ID_SUPPORTED_FEATURES, UINT_DESC_TYPE,
                      (UINT32)2, (UINT8*)temp);
        }
    }
}

/******************************************************************************
**
** Function         bta_ar_dereg_avrc
**
** Description      This function is called to de-register/delete an SDP record for AVRCP.
**
** Returns          void
**
******************************************************************************/
void bta_ar_dereg_avrc(UINT16 service_uuid, tBTA_SYS_ID sys_id)
{
    UINT8   mask = bta_ar_id (sys_id);
    UINT16  categories = 0;
    UINT8   temp[8], *p;

    if (!mask)
        return;

    if (service_uuid == UUID_SERVCLASS_AV_REM_CTRL_TARGET)
    {
        if (bta_ar_cb.sdp_tg_handle && mask == bta_ar_cb.tg_registered)
        {
            bta_ar_cb.tg_registered = 0;
            SDP_DeleteRecord(bta_ar_cb.sdp_tg_handle);
            bta_ar_cb.sdp_tg_handle = 0;
            bta_sys_remove_uuid(service_uuid);
        }
    }
    else if (service_uuid == UUID_SERVCLASS_AV_REMOTE_CONTROL)
    {
        if (bta_ar_cb.sdp_ct_handle)
        {
            bta_ar_cb.ct_categories [mask - 1] = 0;
            categories = bta_ar_cb.ct_categories[0]|bta_ar_cb.ct_categories[1];
            if (!categories)
            {
                /* no CT is still registered - cleaup */
                SDP_DeleteRecord(bta_ar_cb.sdp_ct_handle);
                bta_ar_cb.sdp_ct_handle = 0;
                bta_sys_remove_uuid(service_uuid);
            }
            else
            {
                /* change supported categories to the remaning one */
                p = temp;
                UINT16_TO_BE_STREAM(p, categories);
                SDP_AddAttribute(bta_ar_cb.sdp_ct_handle, ATTR_ID_SUPPORTED_FEATURES, UINT_DESC_TYPE,
                          (UINT32)2, (UINT8*)temp);
            }
        }
    }

}
