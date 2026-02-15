/******************************************************************************
 *
 *  Copyright (C) 2003-2012 Broadcom Corporation
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
 *  This module contains action functions of the link control state machine.
 *
 ******************************************************************************/

#include <string.h>
#include "bt_types.h"
#include "bt_target.h"
#include "bt_utils.h"
#include "avct_api.h"
#include "avct_int.h"

#include "bt_common.h"
#include "btm_api.h"
#include "bta_ar_int_ext.h"
/* packet header length lookup table */
const UINT8 avct_lcb_pkt_type_len[] = {
    AVCT_HDR_LEN_SINGLE,
    AVCT_HDR_LEN_START,
    AVCT_HDR_LEN_CONT,
    AVCT_HDR_LEN_END
};

/*******************************************************************************
**
** Function         avct_lcb_msg_asmbl
**
** Description      Reassemble incoming message.
**
**
** Returns          Pointer to reassembled message;  NULL if no message
**                  available.
**
*******************************************************************************/
static BT_HDR *avct_lcb_msg_asmbl(tAVCT_LCB *p_lcb, BT_HDR *p_buf)
{
    UINT8   *p;
    UINT8   pkt_type;
    BT_HDR  *p_ret;

    /* parse the message header */
    p = (UINT8 *)(p_buf + 1) + p_buf->offset;
    AVCT_PRS_PKT_TYPE(p, pkt_type);

    /* quick sanity check on length */
    if (p_buf->len < avct_lcb_pkt_type_len[pkt_type])
    {
        osi_free(p_buf);
        AVCT_TRACE_WARNING("Bad length during reassembly");
        p_ret = NULL;
    }
    /* single packet */
    else if (pkt_type == AVCT_PKT_TYPE_SINGLE)
    {
        /* if reassembly in progress drop message and process new single */
        if (p_lcb->p_rx_msg != NULL)
            AVCT_TRACE_WARNING("Got single during reassembly");

        osi_free_and_reset((void **)&p_lcb->p_rx_msg);

        p_ret = p_buf;
    }
    /* start packet */
    else if (pkt_type == AVCT_PKT_TYPE_START)
    {
        /* if reassembly in progress drop message and process new start */
        if (p_lcb->p_rx_msg != NULL)
            AVCT_TRACE_WARNING("Got start during reassembly");

        osi_free(p_lcb->p_rx_msg);

        /*
         * Allocate bigger buffer for reassembly. As lower layers are
         * not aware of possible packet size after reassembly, they
         * would have allocated smaller buffer.
         */
        p_lcb->p_rx_msg = (BT_HDR *)osi_malloc(BT_DEFAULT_BUFFER_SIZE);
        memcpy(p_lcb->p_rx_msg, p_buf,
               sizeof(BT_HDR) + p_buf->offset + p_buf->len);

        /* Free original buffer */
        osi_free(p_buf);

        /* update p to point to new buffer */
        p = (UINT8 *)(p_lcb->p_rx_msg + 1) + p_lcb->p_rx_msg->offset;

        /* copy first header byte over nosp */
        *(p + 1) = *p;

        /* set offset to point to where to copy next */
        p_lcb->p_rx_msg->offset += p_lcb->p_rx_msg->len;

        /* adjust length for packet header */
        p_lcb->p_rx_msg->len -= 1;

        p_ret = NULL;
    }
    /* continue or end */
    else
    {
        /* if no reassembly in progress drop message */
        if (p_lcb->p_rx_msg == NULL)
        {
            osi_free(p_buf);
            AVCT_TRACE_WARNING("Pkt type=%d out of order", pkt_type);
            p_ret = NULL;
        }
        else
        {
            /* get size of buffer holding assembled message */
            /*
             * NOTE: The buffer is allocated above at the beginning of the
             * reassembly, and is always of size BT_DEFAULT_BUFFER_SIZE.
             */
            UINT16 buf_len = BT_DEFAULT_BUFFER_SIZE - sizeof(BT_HDR);

            /* adjust offset and len of fragment for header byte */
            p_buf->offset += AVCT_HDR_LEN_CONT;
            p_buf->len -= AVCT_HDR_LEN_CONT;

            /* verify length */
            if ((p_lcb->p_rx_msg->offset + p_buf->len) > buf_len) {
                /* won't fit; free everything */
                AVCT_TRACE_WARNING("%s: Fragmented message too big!", __func__);
                osi_free_and_reset((void **)&p_lcb->p_rx_msg);
                osi_free(p_buf);
                p_ret = NULL;
            } else {
                /* copy contents of p_buf to p_rx_msg */
                memcpy((UINT8 *)(p_lcb->p_rx_msg + 1) + p_lcb->p_rx_msg->offset,
                       (UINT8 *)(p_buf + 1) + p_buf->offset, p_buf->len);

                if (pkt_type == AVCT_PKT_TYPE_END)
                {
                    p_lcb->p_rx_msg->offset -= p_lcb->p_rx_msg->len;
                    p_lcb->p_rx_msg->len += p_buf->len;
                    p_ret = p_lcb->p_rx_msg;
                    p_lcb->p_rx_msg = NULL;
                }
                else
                {
                    p_lcb->p_rx_msg->offset += p_buf->len;
                    p_lcb->p_rx_msg->len += p_buf->len;
                    p_ret = NULL;
                }
                osi_free(p_buf);
            }
        }
    }
    return p_ret;
}

#if (AVCT_BROWSE_INCLUDED == TRUE)

/*******************************************************************************
**
** Function         avct_bcb_msg_asmbl
**
** Description      Reassemble incoming message.
**
**
** Returns          Pointer to reassembled message;  NULL if no message
**                  available.
**
*******************************************************************************/
static BT_HDR *avct_bcb_msg_asmbl(tAVCT_BCB *p_bcb, BT_HDR *p_buf)
{
    UINT8   *p;
    UINT8   pkt_type;
    BT_HDR  *p_ret;

    /* parse the message header */
    AVCT_TRACE_DEBUG("bcb_msg_asmbl peer_mtu:%x, ch_lcid:%x",p_bcb->peer_mtu, \
                              p_bcb->ch_lcid);
    p = (UINT8 *)(p_buf + 1) + p_buf->offset;
    AVCT_PRS_PKT_TYPE(p, pkt_type);
    AVCT_TRACE_DEBUG("bcb_msg_asmbl pkt_type:%d, p_buf->offset:%x",pkt_type, \
                               p_buf->offset);
    /* quick sanity check on length */
    if (p_buf->len < avct_lcb_pkt_type_len[pkt_type])
    {
        osi_free_and_reset((void**)&p_buf);
        AVCT_TRACE_WARNING("### Bad length during reassembly");
        p_ret = NULL;
    }

    /* As Per AVRCP 1.5 Spec,Fragmentation is not allowed
     * Section 7.2 AVCTP fragmentation shall not be used on the AVCTP Browsing Channel.
     * packet type wil be single else return error
    */
    else if (pkt_type == AVCT_PKT_TYPE_SINGLE)
    {
        AVCT_TRACE_DEBUG("Got single during reassembly");
        p_ret = p_buf;
    }
    else
    {
        osi_free_and_reset((void**)&p_buf);
        p_ret =NULL;
        AVCT_TRACE_WARNING("### Got Fragmented packet");
    }
    return p_ret;
}
#endif



/*******************************************************************************
**
** Function         avct_lcb_chnl_open
**
** Description      Open L2CAP channel to peer
**
**
** Returns          Nothing.
**
*******************************************************************************/
void avct_lcb_chnl_open(tAVCT_LCB *p_lcb, tAVCT_LCB_EVT *p_data)
{
    UINT16 result = AVCT_RESULT_FAIL;
    UNUSED(p_data);

    BTM_SetOutService(p_lcb->peer_addr, BTM_SEC_SERVICE_AVCTP, 0);
    /* call l2cap connect req */
    p_lcb->ch_state = AVCT_CH_CONN;
    if ((p_lcb->ch_lcid = L2CA_ConnectReq(AVCT_PSM, p_lcb->peer_addr)) == 0)
    {
        /* if connect req failed, send ourselves close event */
        avct_lcb_event(p_lcb, AVCT_LCB_LL_CLOSE_EVT, (tAVCT_LCB_EVT *) &result);
    }
}

#if (AVCT_BROWSE_INCLUDED == TRUE)
/*******************************************************************************
**
** Function         avct_bcb_chnl_open
**
** Description
**
** Returns          Nothing.
**
*******************************************************************************/
void avct_bcb_chnl_open(tAVCT_BCB *p_bcb, tAVCT_LCB_EVT *p_data)
{
    AVCT_TRACE_DEBUG("avct_bcb_chnl_open BEGIN!~");

    uint16_t result = AVCT_RESULT_FAIL;
    tAVCT_LCB* p_lcb = avct_lcb_by_bcb(p_bcb);
    tL2CAP_ERTM_INFO ertm_info;

    BTM_SetOutService(p_lcb->peer_addr, BTM_SEC_SERVICE_AVCTP_BROWSE, 0);

    /* Set the FCR options: Browsing channel mandates ERTM */
    ertm_info.preferred_mode = avct_l2c_br_fcr_opts_def.mode;
    ertm_info.allowed_modes = L2CAP_FCR_CHAN_OPT_ERTM;
    ertm_info.user_rx_buf_size = BT_DEFAULT_BUFFER_SIZE;
    ertm_info.user_tx_buf_size = BT_DEFAULT_BUFFER_SIZE;
    ertm_info.fcr_rx_buf_size = BT_DEFAULT_BUFFER_SIZE;
    ertm_info.fcr_tx_buf_size = BT_DEFAULT_BUFFER_SIZE;

    /* call l2cap connect req */
    p_bcb->ch_state = AVCT_CH_CONN;
    p_bcb->ch_lcid =
        L2CA_ErtmConnectReq(AVCT_BR_PSM, p_lcb->peer_addr, &ertm_info);
    if (p_bcb->ch_lcid == 0) {
      /* if connect req failed, send ourselves close event */
      avct_bcb_event(p_bcb, AVCT_LCB_LL_CLOSE_EVT, (tAVCT_LCB_EVT*)&result);
    }


    AVCT_TRACE_DEBUG("avct_bcb_chnl_open END!~");

}


/********************************************************************************
**
** Function        avct_close_bcb
**
** Description     Clear BCB data structure
**
**
** Returns         void.
**
******************************************************************************/
void avct_close_bcb(tAVCT_LCB *p_lcb, tAVCT_LCB_EVT *p_data)
{
    tAVCT_BCB        *p_bcb = NULL;

    AVCT_TRACE_DEBUG("avct_close_bcb");
    p_bcb  = avct_bcb_by_lcb(p_lcb);
    if (p_bcb != NULL)
    {
        AVCT_TRACE_DEBUG("Send Disconnect Event");
   //     p_bcb->allocated = 0;
        avct_bcb_event( p_bcb, AVCT_LCB_INT_CLOSE_EVT, p_data);
    }
}

#endif

/*******************************************************************************
**
** Function         avct_lcb_unbind_disc
**
** Description      Deallocate ccb and call callback with disconnect event.
**
**
** Returns          Nothing.
**
*******************************************************************************/
void avct_lcb_unbind_disc(tAVCT_LCB *p_lcb, tAVCT_LCB_EVT *p_data)
{
    UNUSED(p_lcb);

    avct_ccb_dealloc(p_data->p_ccb, AVCT_DISCONNECT_CFM_EVT, 0, NULL);
}

#if (AVCT_BROWSE_INCLUDED == TRUE)
/*******************************************************************************
**
** Function         avct_bcb_unbind_disc
**
** Description      Deallocate ccb and call callback with disconnect event.
**
**
** Returns          Nothing.
**
*******************************************************************************/
void avct_bcb_unbind_disc(tAVCT_BCB *p_bcb, tAVCT_LCB_EVT *p_data)
{
    AVCT_TRACE_DEBUG("avct_bcb_unbind_disc !~");

    p_data->p_ccb->p_bcb = NULL;
    (*p_data->p_ccb->cc.p_ctrl_cback)(avct_ccb_to_idx(p_data->p_ccb),
                                      AVCT_BROWSE_DISCONN_CFM_EVT, 0, NULL);

}
#endif


/*******************************************************************************
**
** Function         avct_lcb_open_ind
**
** Description      Handle an LL_OPEN event.  For each allocated ccb already
**                  bound to this lcb, send a connect event.  For each
**                  unbound ccb with a new PID, bind that ccb to this lcb and
**                  send a connect event.
**
**
** Returns          Nothing.
**
*******************************************************************************/
void avct_lcb_open_ind(tAVCT_LCB *p_lcb, tAVCT_LCB_EVT *p_data)
{
    tAVCT_CCB   *p_ccb = &avct_cb.ccb[0];
    int         i;
    BOOLEAN     bind = FALSE;
    DEV_CLASS device_class;
    UINT16 service_class; UINT8 major_class = 0, minor_class = 0;
    BOOLEAN sink_only_cod = false;
    BOOLEAN src_only_cod = false;

    /* First check, if AVDTP was done earlier and we have any info from avdtp */
    UINT8 avdtp_conn_type = get_remote_sep_type(p_lcb->peer_addr);
    AVCT_TRACE_DEBUG(" avdtp_conn_type %d ", avdtp_conn_type);

    if(avdtp_conn_type == 0)
    {
        /* In this case, index was not found, Check COD once */
        BTM_GetCOD(p_lcb->peer_addr, device_class);
        BTM_COD_SERVICE_CLASS(service_class, device_class);
        sink_only_cod = (service_class & BTM_COD_SERVICE_RENDERING) && !(service_class & BTM_COD_SERVICE_CAPTURING);
        src_only_cod = (service_class & BTM_COD_SERVICE_CAPTURING) && !(service_class & BTM_COD_SERVICE_RENDERING);
        APPL_TRACE_DEBUG(" %s Major_Class [%x], Minor Class [%x], Service_Class [%x], src_only %d, sink_only %d",__FUNCTION__,
                 major_class, minor_class, service_class, src_only_cod, sink_only_cod);
        /* If from COD we come to know that its src or Sink only */
        if(sink_only_cod)
        {
            /* remote is SInk only, this indication should go to AV */
            avdtp_conn_type = BTA_AR_EXT_AV_MASK;
        }
        if(src_only_cod)
        {
            /* remote is Src only, this indication should go to AVK */
            avdtp_conn_type = BTA_AR_EXT_AVK_MASK;
        }
    }
    AVCT_TRACE_DEBUG(" avdtp_conn_type COD update %d ", avdtp_conn_type);

    for (i = 0; i < AVCT_NUM_CONN; i++, p_ccb++)
    {
        AVCT_TRACE_DEBUG("avct_lcb_open_ind, %d index = %d ", p_ccb->allocated, i);
        /* if ccb allocated and */
        if (p_ccb->allocated)
        {
            /* if bound to this lcb send connect confirm event */
            AVCT_TRACE_DEBUG("%s profile id 0x%0X : role %s ccb_sep = %d",__FUNCTION__, p_ccb->cc.pid,
                (p_ccb->cc.role) ? "AVCT_ACP" : "AVCT_INT", p_ccb->cc.av_sep_type);
            if (p_ccb->p_lcb == p_lcb)
            {
                AVCT_TRACE_DEBUG("avct_lcb_open_ind, bind true");
                bind = TRUE;
                L2CA_SetTxPriority(p_lcb->ch_lcid, L2CAP_CHNL_PRIORITY_HIGH);
                p_ccb->cc.p_ctrl_cback(avct_ccb_to_idx(p_ccb), AVCT_CONNECT_CFM_EVT,
                                       0, p_lcb->peer_addr);
            }
            /* if unbound acceptor and lcb doesn't already have a ccb for this PID */
            else if ((p_ccb->p_lcb == NULL) && (p_ccb->cc.role == AVCT_ACP) &&
                     (avct_lcb_has_pid(p_lcb, p_ccb->cc.pid) == NULL))
            {
                if((avdtp_conn_type & BTA_AR_EXT_AV_MASK) && !(avdtp_conn_type & BTA_AR_EXT_AVK_MASK))
                {
                    /* Remote is pure A2DP Src, we should send indication to AV profile only */
                    if(p_ccb->cc.av_sep_type != BTA_AV_RC_PROFILE_SRC)
                        continue;
                }
                if((avdtp_conn_type & BTA_AR_EXT_AVK_MASK) && !(avdtp_conn_type & BTA_AR_EXT_AV_MASK))
                {
                    /* Remote is pure A2DP Sink, we should send indication to AV profile only */
                    if(p_ccb->cc.av_sep_type != BTA_AV_RC_PROFILE_SINK)
                        continue;
                }
                /* bind ccb to lcb and send connect ind event */
                AVCT_TRACE_DEBUG("avct_lcb_open_ind, bind and update");
                bind = TRUE;
                p_ccb->p_lcb = p_lcb;
                L2CA_SetTxPriority(p_lcb->ch_lcid, L2CAP_CHNL_PRIORITY_HIGH);
                p_ccb->cc.p_ctrl_cback(avct_ccb_to_idx(p_ccb), AVCT_CONNECT_IND_EVT,
                                    0, p_lcb->peer_addr);
            }
        }
    }

    /* if no ccbs bound to this lcb, disconnect */
    if (bind == FALSE)
    {
        AVCT_TRACE_DEBUG("avct_lcb_open_ind, send disconnect");
        avct_lcb_event(p_lcb, AVCT_LCB_INT_CLOSE_EVT, p_data);
    }
}

#if (AVCT_BROWSE_INCLUDED == TRUE)
/*******************************************************************************
**
** Function         avct_bcb_open_ind
**
** Description      Handle an LL_OPEN event.  For each allocated ccb already
**                  bound to this lcb, send a connect event.  For each
**                  unbound ccb with a new PID, bind that ccb to this lcb and
**                  send a connect event.
**
**
** Returns          Nothing.
**
*******************************************************************************/
void avct_bcb_open_ind(tAVCT_BCB *p_bcb, tAVCT_LCB_EVT *p_data)
{
    tAVCT_CCB   *p_ccb = &avct_cb.ccb[0];
    tAVCT_LCB   *p_lcb =NULL;
    int         i;
    BOOLEAN     bind = FALSE;

    for (i = 0; i < AVCT_NUM_CONN; i++, p_ccb++)
    {
        AVCT_TRACE_DEBUG("avct_bcb_open_ind: %d",p_ccb->allocated);
        /*if ccb allocated */
        if (p_ccb->allocated & AVCT_ALOC_BCB)
        {
            /* if bound to this bcb then send connect confirm event */
            if (p_ccb->p_bcb == p_bcb)
            {
                AVCT_TRACE_DEBUG("open_ind success");
                p_lcb   = avct_cb.ccb[i].p_lcb;
                if (p_lcb != NULL)
                {
                    bind = TRUE;
                    p_ccb->cc.p_ctrl_cback(avct_ccb_to_idx(p_ccb), AVCT_BROWSE_CONN_CFM_EVT,
                                          0, p_lcb->peer_addr);
                    break;
                }
            }
        }

    }
    if (bind == FALSE)
    {
        AVCT_TRACE_ERROR("### open_ind error");
        avct_bcb_event(p_bcb, AVCT_LCB_INT_CLOSE_EVT, p_data);
    }
}
#endif


/*******************************************************************************
**
** Function         avct_lcb_open_fail
**
** Description      L2CAP channel open attempt failed.  Deallocate any ccbs
**                  on this lcb and send connect confirm event with failure.
**
**
** Returns          Nothing.
**
*******************************************************************************/
void avct_lcb_open_fail(tAVCT_LCB *p_lcb, tAVCT_LCB_EVT *p_data)
{
    tAVCT_CCB           *p_ccb = &avct_cb.ccb[0];
    int                 i;

    for (i = 0; i < AVCT_NUM_CONN; i++, p_ccb++)
    {
        if (p_ccb->allocated && (p_ccb->p_lcb == p_lcb))
        {
            avct_ccb_dealloc(p_ccb, AVCT_CONNECT_CFM_EVT,
                             p_data->result, p_lcb->peer_addr);
        }
    }
}
#if (AVCT_BROWSE_INCLUDED == TRUE)
/*******************************************************************************
**
** Function         avct_bcb_open_fail
**
** Description      L2CAP channel open attempt failed.  Deallocate any ccbs
**                  on this lcb and send connect confirm event with failure.
**
**
** Returns          Nothing.
**
*******************************************************************************/
void avct_bcb_open_fail(tAVCT_BCB *p_bcb, tAVCT_LCB_EVT *p_data)
{
    AVCT_TRACE_DEBUG("avct_bcb_open_fail!~");

    tAVCT_CCB* p_ccb = &avct_cb.ccb[0];

    for (int idx = 0; idx < AVCT_NUM_CONN; idx++, p_ccb++) {
      if (p_ccb->allocated && (p_ccb->p_bcb == p_bcb)) {
        p_ccb->p_bcb = NULL;
      }
    }

}

#endif
/*******************************************************************************
**
** Function         avct_lcb_close_ind
**
** Description      L2CAP channel closed by peer.  Deallocate any initiator
**                  ccbs on this lcb and send disconnect ind event.
**
**
** Returns          Nothing.
**
*******************************************************************************/
void avct_lcb_close_ind(tAVCT_LCB *p_lcb, tAVCT_LCB_EVT *p_data)
{
    tAVCT_CCB           *p_ccb = &avct_cb.ccb[0];
    int                 i;
    UNUSED(p_data);

    for (i = 0; i < AVCT_NUM_CONN; i++, p_ccb++)
    {
        if (p_ccb->allocated && (p_ccb->p_lcb == p_lcb))
        {
            if (p_ccb->cc.role == AVCT_INT)
            {
                avct_ccb_dealloc(p_ccb, AVCT_DISCONNECT_IND_EVT,
                                 0, p_lcb->peer_addr);
            }
            else
            {
                p_ccb->p_lcb = NULL;
                (*p_ccb->cc.p_ctrl_cback)(avct_ccb_to_idx(p_ccb), AVCT_DISCONNECT_IND_EVT,
                                          0, p_lcb->peer_addr);
            }
        }
    }
}

#if (AVCT_BROWSE_INCLUDED == TRUE)
/*******************************************************************************
**
** Function         avct_lcb_close_ind
**
** Description      L2CAP channel closed by peer.  Deallocate any initiator
**                  ccbs on this lcb and send disconnect ind event.
**
**
** Returns          Nothing.
**
*******************************************************************************/
void avct_bcb_close_ind(tAVCT_BCB *p_bcb, tAVCT_LCB_EVT *p_data)
{
    AVCT_TRACE_DEBUG("avct_bcb_close_ind!~");

    tAVCT_CCB* p_ccb = &avct_cb.ccb[0];
    tAVCT_LCB* p_lcb = avct_lcb_by_bcb(p_bcb);

    for (int idx = 0; idx < AVCT_NUM_CONN; idx++, p_ccb++) {
      if (p_ccb->allocated && (p_ccb->p_bcb == p_bcb)) {
        if (p_ccb->cc.role == AVCT_INT) {
          (*p_ccb->cc.p_ctrl_cback)(avct_ccb_to_idx(p_ccb),
                                    AVCT_BROWSE_DISCONN_CFM_EVT, 0,
                                    p_lcb->peer_addr);
        } else {
          (*p_ccb->cc.p_ctrl_cback)(avct_ccb_to_idx(p_ccb),
                                    AVCT_BROWSE_DISCONN_IND_EVT, 0, NULL);
        }
        p_ccb->p_bcb = NULL;
      }
    }

/*    tAVCT_CCB           *p_ccb = &avct_cb.ccb[0];
    int                 i;
    AVCT_TRACE_DEBUG("avct_bcb_close_ind");
    for (i = 0; i < AVCT_NUM_CONN; i++, p_ccb++)
    {
        if (p_ccb->allocated && (p_ccb->p_bcb == p_bcb))
        {
            //set avct_cb.bcb to 0
            memset(p_ccb->p_bcb, 0 ,sizeof(tAVCT_BCB));
            p_ccb->p_bcb = NULL;
            AVCT_TRACE_DEBUG("**close_ind");
        }
    }*/
}
#endif


/*******************************************************************************
**
** Function         avct_lcb_close_cfm
**
** Description      L2CAP channel closed by us.  Deallocate any initiator
**                  ccbs on this lcb and send disconnect ind or cfm event.
**
**
** Returns          Nothing.
**
*******************************************************************************/
void avct_lcb_close_cfm(tAVCT_LCB *p_lcb, tAVCT_LCB_EVT *p_data)
{
    tAVCT_CCB           *p_ccb = &avct_cb.ccb[0];
    int                 i;
    UINT8               event;

    for (i = 0; i < AVCT_NUM_CONN; i++, p_ccb++)
    {
        if (p_ccb->allocated && (p_ccb->p_lcb == p_lcb))
        {
            /* if this ccb initiated close send disconnect cfm otherwise ind */
            if (p_ccb->ch_close)
            {
                p_ccb->ch_close = FALSE;
                event = AVCT_DISCONNECT_CFM_EVT;
            }
            else
            {
                event = AVCT_DISCONNECT_IND_EVT;
            }

            if (p_ccb->cc.role == AVCT_INT)
            {
                avct_ccb_dealloc(p_ccb, event, p_data->result, p_lcb->peer_addr);
            }
            else
            {
                p_ccb->p_lcb = NULL;
                (*p_ccb->cc.p_ctrl_cback)(avct_ccb_to_idx(p_ccb), event,
                                       p_data->result, p_lcb->peer_addr);
            }
        }
    }
}

#if (AVCT_BROWSE_INCLUDED == TRUE)
/*******************************************************************************
**
** Function         avct_bcb_close_cfm
**
** Description
**
**
** Returns          Nothing.
**
*******************************************************************************/
void avct_bcb_close_cfm(tAVCT_BCB *p_bcb, tAVCT_LCB_EVT *p_data)
{
    AVCT_TRACE_DEBUG("avct_bcb_close_cfm!~");
    tAVCT_CCB* p_ccb = &avct_cb.ccb[0];
    uint8_t event = 0;
    /* Whether BCB initiated channel close */
    bool ch_close = p_bcb->ch_close;
    tAVCT_CTRL_CBACK* p_cback;

    p_bcb->ch_close = false;
    p_bcb->allocated = 0;
    for (int idx = 0; idx < AVCT_NUM_CONN; idx++, p_ccb++) {
      if (p_ccb->allocated && (p_ccb->p_bcb == p_bcb)) {
        /* if this ccb initiated close send disconnect cfm otherwise ind */
        if (ch_close) {
          event = AVCT_BROWSE_DISCONN_CFM_EVT;
        } else {
          event = AVCT_BROWSE_DISCONN_IND_EVT;
        }

        p_cback = p_ccb->cc.p_ctrl_cback;
        p_ccb->p_bcb = NULL;
        if (p_ccb->p_lcb == NULL) avct_ccb_dealloc(p_ccb, AVCT_NO_EVT, 0, NULL);
        (*p_cback)(avct_ccb_to_idx(p_ccb), event, p_data->result,
                   p_bcb->peer_addr);
      }
    }

}
#endif


/*******************************************************************************
**
** Function         avct_lcb_bind_conn
**
** Description      Bind ccb to lcb and send connect cfm event.
**
**
** Returns          Nothing.
**
*******************************************************************************/
void avct_lcb_bind_conn(tAVCT_LCB *p_lcb, tAVCT_LCB_EVT *p_data)
{
    p_data->p_ccb->p_lcb = p_lcb;
    (*p_data->p_ccb->cc.p_ctrl_cback)(avct_ccb_to_idx(p_data->p_ccb),
                                      AVCT_CONNECT_CFM_EVT, 0, p_lcb->peer_addr);
}

#if (AVCT_BROWSE_INCLUDED == TRUE)
/*******************************************************************************
**
** Function         avct_lcb_bind_conn
**
** Description      Bind ccb to lcb and send connect cfm event.
**
**
** Returns          Nothing.
**
*******************************************************************************/
void avct_bcb_bind_conn(tAVCT_BCB *p_bcb, tAVCT_LCB_EVT *p_data)
{
    AVCT_TRACE_DEBUG("avct_bcb_bind_conn !~");

    tAVCT_LCB* p_lcb = avct_lcb_by_bcb(p_bcb);
    p_data->p_ccb->p_bcb = p_bcb;
    (*p_data->p_ccb->cc.p_ctrl_cback)(avct_ccb_to_idx(p_data->p_ccb),
                                      AVCT_BROWSE_CONN_CFM_EVT, 0,
                                      p_lcb->peer_addr);

}

#endif
/*******************************************************************************
**
** Function         avct_lcb_chk_disc
**
** Description      A ccb wants to close; if it is the last ccb on this lcb,
**                  close channel.  Otherwise just deallocate and call
**                  callback.
**
**
** Returns          Nothing.
**
*******************************************************************************/
void avct_lcb_chk_disc(tAVCT_LCB *p_lcb, tAVCT_LCB_EVT *p_data)
{
    AVCT_TRACE_WARNING("avct_lcb_chk_disc");
#if (AVCT_BROWSE_INCLUDED == TRUE)
    avct_close_bcb(p_lcb, p_data);
#endif
    if (avct_lcb_last_ccb(p_lcb, p_data->p_ccb))
    {
        AVCT_TRACE_WARNING("closing");
        p_data->p_ccb->ch_close = TRUE;
        avct_lcb_event(p_lcb, AVCT_LCB_INT_CLOSE_EVT, p_data);
    }
    else
    {
        AVCT_TRACE_WARNING("dealloc ccb");
        avct_lcb_unbind_disc(p_lcb, p_data);
    }
}

#if (AVCT_BROWSE_INCLUDED == TRUE)
/*******************************************************************************
**
** Function         avct_bcb_chk_disc
**
** Description
**
** Returns          Nothing.
**
*******************************************************************************/
void avct_bcb_chk_disc(tAVCT_BCB *p_bcb, tAVCT_LCB_EVT *p_data)
{
    AVCT_TRACE_WARNING("avct_bcb_chk_disc !~");

    p_bcb->ch_close = avct_bcb_get_last_ccb_index(p_bcb, p_data->p_ccb);
    if (p_bcb->ch_close) {
      avct_bcb_event(p_bcb, AVCT_LCB_INT_CLOSE_EVT, p_data);
      return;
    }

    avct_bcb_unbind_disc(p_bcb, p_data);

}
#endif

/*******************************************************************************
**
** Function         avct_lcb_chnl_disc
**
** Description      Disconnect L2CAP channel.
**
**
** Returns          Nothing.
**
*******************************************************************************/
void avct_lcb_chnl_disc(tAVCT_LCB *p_lcb, tAVCT_LCB_EVT *p_data)
{
    UNUSED(p_data);
    AVCT_TRACE_DEBUG("avct_lcb_chnl_disc");
    L2CA_DisconnectReq(p_lcb->ch_lcid);
}

#if (AVCT_BROWSE_INCLUDED == TRUE)
/*******************************************************************************
**
** Function         avct_bcb_chnl_disc
**
** Description      Disconnect L2CAP channel.
**
**
** Returns          Nothing.
**
*******************************************************************************/
void avct_bcb_chnl_disc(tAVCT_BCB *p_bcb, tAVCT_LCB_EVT *p_data)
{
    AVCT_TRACE_ERROR("avct_bcb_chnl_disc");
    /* Disconnect L2CAP Browsing channel */
    L2CA_DisconnectReq(p_bcb->ch_lcid);
}
#endif

/*******************************************************************************
**
** Function         avct_lcb_bind_fail
**
** Description      Deallocate ccb and call callback with connect event
**                  with failure result.
**
**
** Returns          Nothing.
**
*******************************************************************************/
void avct_lcb_bind_fail(tAVCT_LCB *p_lcb, tAVCT_LCB_EVT *p_data)
{
    UNUSED(p_lcb);

    avct_ccb_dealloc(p_data->p_ccb, AVCT_CONNECT_CFM_EVT, AVCT_RESULT_FAIL, NULL);
}

#if (AVCT_BROWSE_INCLUDED == TRUE)
/*******************************************************************************
**
** Function         avct_bcb_bind_fail
**
** Description
**
** Returns          Nothing.
**
*******************************************************************************/
void avct_bcb_bind_fail(tAVCT_BCB *p_bcb, tAVCT_LCB_EVT *p_data)
{
    AVCT_TRACE_DEBUG("avct_bcb_bind_fail !~");

    p_data->p_ccb->p_bcb = NULL;
    (*p_data->p_ccb->cc.p_ctrl_cback)(avct_ccb_to_idx(p_data->p_ccb),
                                      AVCT_BROWSE_CONN_CFM_EVT, AVCT_RESULT_FAIL,
                                      NULL);

}
#endif

/*******************************************************************************
**
** Function         avct_lcb_cong_ind
**
** Description      Handle congestion indication from L2CAP.
**
**
** Returns          Nothing.
**
*******************************************************************************/
void avct_lcb_cong_ind(tAVCT_LCB *p_lcb, tAVCT_LCB_EVT *p_data)
{
    tAVCT_CCB           *p_ccb = &avct_cb.ccb[0];
    int                 i;
    UINT8               event;
    BT_HDR          *p_buf;

    /* set event */
    event = (p_data->cong) ? AVCT_CONG_IND_EVT : AVCT_UNCONG_IND_EVT;
    p_lcb->cong = p_data->cong;
    if (p_lcb->cong == FALSE && !fixed_queue_is_empty(p_lcb->tx_q))
    {
        while (!p_lcb->cong &&
               (p_buf = (BT_HDR *)fixed_queue_try_dequeue(p_lcb->tx_q)) != NULL)
        {
            if (L2CA_DataWrite(p_lcb->ch_lcid, p_buf) == L2CAP_DW_CONGESTED)
            {
                p_lcb->cong = TRUE;
            }
        }
    }

    /* send event to all ccbs on this lcb */
    for (i = 0; i < AVCT_NUM_CONN; i++, p_ccb++)
    {
        if (p_ccb->allocated && (p_ccb->p_lcb == p_lcb))
        {
            (*p_ccb->cc.p_ctrl_cback)(avct_ccb_to_idx(p_ccb), event, 0, p_lcb->peer_addr);
        }
    }
}

#if (AVCT_BROWSE_INCLUDED == TRUE)
/*******************************************************************************
**
** Function         avct_bcb_cong_ind
**
** Description      Handle congestion indication from L2CAP.
**
**
** Returns          Nothing.
**
*******************************************************************************/
void avct_bcb_cong_ind(tAVCT_BCB *p_bcb, tAVCT_LCB_EVT *p_data)
{
    UINT8         event;
    BT_HDR       *p_buf;

    AVCT_TRACE_DEBUG("avct_bcb_cong_ind");
    if (p_bcb != NULL)
    {
        AVCT_TRACE_DEBUG("avct_bcb_cong_ind = %d", p_data->cong);

        event = (p_data->cong) ? AVCT_CONG_IND_EVT : AVCT_UNCONG_IND_EVT;
        p_bcb->cong = p_data->cong;
        if (p_bcb->cong == FALSE && !fixed_queue_is_empty(p_bcb->tx_q))
        {
            while (!p_bcb->cong &&
                   (p_buf = (BT_HDR *)fixed_queue_try_dequeue(p_bcb->tx_q)) != NULL)
            {
                if (L2CA_DataWrite(p_bcb->ch_lcid, p_buf) == L2CAP_DW_CONGESTED)
                {
                    p_bcb->cong = TRUE;
                }
            }
        }
    }
    else
    {
        AVCT_TRACE_ERROR("### bcb NULL");
    }
}
#endif

/*******************************************************************************
**
** Function         avct_lcb_discard_msg
**
** Description      Discard a message sent in from the API.
**
**
** Returns          Nothing.
**
*******************************************************************************/
void avct_lcb_discard_msg(tAVCT_LCB *p_lcb, tAVCT_LCB_EVT *p_data)
{
    UNUSED(p_lcb);

    AVCT_TRACE_WARNING("Dropping message");
    osi_free_and_reset((void **)&p_data->ul_msg.p_buf);
}

#if (AVCT_BROWSE_INCLUDED == TRUE)
/*******************************************************************************
**
** Function         avct_bcb_discard_msg
**
** Description      Discard a message sent in from the API.
**
**
** Returns          Nothing.
**
*******************************************************************************/
void avct_bcb_discard_msg(tAVCT_BCB *p_bcb, tAVCT_LCB_EVT *p_data)
{
    osi_free_and_reset((void**)&p_bcb->p_tx_msg);

    /* if control channel is up, save the message and open the browsing channel */
    if (p_data->ul_msg.p_ccb->p_lcb == NULL) {
      osi_free_and_reset((void**)&p_data->ul_msg.p_buf);
      return;
    }
    p_bcb->p_tx_msg = p_data->ul_msg.p_buf;

    if (p_bcb->p_tx_msg) {
      p_bcb->p_tx_msg->layer_specific =
          (p_data->ul_msg.cr << 8) + p_data->ul_msg.label;

      /* the channel is closed, opening or closing - open it again */
      AVCT_TRACE_DEBUG("ch_state: %d, allocated:%d->%d", p_bcb->ch_state,
                       p_bcb->allocated, p_data->ul_msg.p_ccb->p_lcb->allocated);
      p_bcb->allocated = p_data->ul_msg.p_ccb->p_lcb->allocated;
      avct_bcb_event(p_bcb, AVCT_LCB_UL_BIND_EVT,
                     (tAVCT_LCB_EVT*)p_data->ul_msg.p_ccb);
    }

}
#endif

/*******************************************************************************
**
** Function         avct_lcb_send_msg
**
** Description      Build and send an AVCTP message.
**
**
** Returns          Nothing.
**
*******************************************************************************/
void avct_lcb_send_msg(tAVCT_LCB *p_lcb, tAVCT_LCB_EVT *p_data)
{
    UINT16          curr_msg_len;
    UINT8           pkt_type;
    UINT8           hdr_len;
    UINT8           *p;
    UINT8           nosp = 0;       /* number of subsequent packets */
    UINT16          temp;
    UINT16          buf_size = p_lcb->peer_mtu + L2CAP_MIN_OFFSET + BT_HDR_SIZE;


    /* store msg len */
    curr_msg_len = p_data->ul_msg.p_buf->len;

    /* initialize packet type and other stuff */
    if (curr_msg_len <= (p_lcb->peer_mtu - AVCT_HDR_LEN_SINGLE))
    {
        pkt_type = AVCT_PKT_TYPE_SINGLE;
    }
    else
    {
        pkt_type = AVCT_PKT_TYPE_START;
        temp = (curr_msg_len + AVCT_HDR_LEN_START - p_lcb->peer_mtu);
        nosp = temp / (p_lcb->peer_mtu - 1) + 1;
        if ( (temp % (p_lcb->peer_mtu - 1)) != 0)
            nosp++;
    }

    /* while we haven't sent all packets */
    while (curr_msg_len != 0) {
        BT_HDR *p_buf;

        /* set header len */
        hdr_len = avct_lcb_pkt_type_len[pkt_type];

        /* if remaining msg must be fragmented */
        if (p_data->ul_msg.p_buf->len > (p_lcb->peer_mtu - hdr_len))
        {
            /* get a new buffer for fragment we are sending */
            p_buf = (BT_HDR *)osi_malloc(buf_size);

            /* copy portion of data from current message to new buffer */
            p_buf->offset = L2CAP_MIN_OFFSET + hdr_len;
            p_buf->len = p_lcb->peer_mtu - hdr_len;

            memcpy((UINT8 *)(p_buf + 1) + p_buf->offset,
                   (UINT8 *)(p_data->ul_msg.p_buf + 1) + p_data->ul_msg.p_buf->offset, p_buf->len);

            p_data->ul_msg.p_buf->offset += p_buf->len;
            p_data->ul_msg.p_buf->len -= p_buf->len;
        }
        else
        {
            p_buf = p_data->ul_msg.p_buf;
        }

        curr_msg_len -= p_buf->len;

        /* set up to build header */
        p_buf->len += hdr_len;
        p_buf->offset -= hdr_len;
        p = (UINT8 *)(p_buf + 1) + p_buf->offset;

        /* build header */
        AVCT_BLD_HDR(p, p_data->ul_msg.label, pkt_type, p_data->ul_msg.cr);
        if (pkt_type == AVCT_PKT_TYPE_START)
        {
            UINT8_TO_STREAM(p, nosp);
        }
        if ((pkt_type == AVCT_PKT_TYPE_START) || (pkt_type == AVCT_PKT_TYPE_SINGLE))
        {
            UINT16_TO_BE_STREAM(p, p_data->ul_msg.p_ccb->cc.pid);
        }

        if (p_lcb->cong == TRUE)
        {
            fixed_queue_enqueue(p_lcb->tx_q, p_buf);
        }

        /* send message to L2CAP */
        else
        {
            if (L2CA_DataWrite(p_lcb->ch_lcid, p_buf) == L2CAP_DW_CONGESTED)
            {
                p_lcb->cong = TRUE;
            }
        }

        /* update pkt type for next packet */
        if (curr_msg_len > (p_lcb->peer_mtu - AVCT_HDR_LEN_END))
        {
            pkt_type = AVCT_PKT_TYPE_CONT;
        }
        else
        {
            pkt_type = AVCT_PKT_TYPE_END;
        }
    }
    AVCT_TRACE_DEBUG ("avct_lcb_send_msg tx_q_count:%d",
                      fixed_queue_length(p_lcb->tx_q));
    return;
}
#if (AVCT_BROWSE_INCLUDED == TRUE)
/*******************************************************************************
**
** Function         avct_lcb_send_msg
**
** Description      Build and send an AVCTP message.
**
**
** Returns          Nothing.
**
*******************************************************************************/
void avct_bcb_send_msg(tAVCT_BCB *p_bcb, tAVCT_LCB_EVT *p_data)
{
    UINT16          curr_msg_len;
    UINT8           pkt_type;
    UINT8           *p;
    BT_HDR          *p_buf;
    /* store msg len */
    curr_msg_len = p_data->ul_msg.p_buf->len;
    AVCT_TRACE_DEBUG("avct_bcb_send_msg  length: %x",curr_msg_len);
    AVCT_TRACE_DEBUG("Remote PEER MTU: %x",p_bcb->peer_mtu);
    /* initialize packet type and other stuff */
    if (curr_msg_len <= (p_bcb->peer_mtu - AVCT_HDR_LEN_SINGLE))
    {
        pkt_type = AVCT_PKT_TYPE_SINGLE;
        //No need to do segmentation need to send data as a single packet
        p_buf = p_data->ul_msg.p_buf;

        /* set up to build header */
        p_buf->len += AVCT_HDR_LEN_SINGLE;
        p_buf->offset -= AVCT_HDR_LEN_SINGLE;
        p = (UINT8 *)(p_buf + 1) + p_buf->offset;

        /* build header */
 //       p_data->ul_msg.cr = AVCT_RSP ;
        AVCT_BLD_HDR(p, p_data->ul_msg.label, pkt_type, p_data->ul_msg.cr);
        //UINT8_TO_STREAM(p, nosp);
        p_data->ul_msg.p_ccb->cc.pid = 0x110E;
        UINT16_TO_BE_STREAM(p, p_data->ul_msg.p_ccb->cc.pid);

        p_buf->layer_specific = AVCT_DATA_BROWSE;
        if (p_bcb->cong == TRUE)
        {
            AVCT_TRACE_ERROR("L2CAP congestion");
            fixed_queue_enqueue(p_bcb->tx_q, p_buf);
        }
        else
        {
            if (L2CA_DataWrite(p_bcb->ch_lcid, p_buf) == L2CAP_DW_CONGESTED)
            {
                AVCT_TRACE_DEBUG("L2CAP Data Write");
                p_bcb->cong = TRUE;
                //Flag to be cleared
            }
        }
    }
    else
    {
        AVCT_TRACE_ERROR("### bcb_send_msg, length incorrect");
    }
}
#endif
/*******************************************************************************
**
** Function         avct_lcb_free_msg_ind
**
** Description      Discard an incoming AVCTP message.
**
**
** Returns          Nothing.
**
*******************************************************************************/
void avct_lcb_free_msg_ind(tAVCT_LCB *p_lcb, tAVCT_LCB_EVT *p_data)
{
    UNUSED(p_lcb);

    if (p_data == NULL)
        return;

    osi_free_and_reset((void **)&p_data->p_buf);
}

#if (AVCT_BROWSE_INCLUDED == TRUE)
/*******************************************************************************
**
** Function         avct_bcb_free_msg_ind
**
** Description      Discard an incoming AVCTP message.
**
**
** Returns          Nothing.
**
*******************************************************************************/
void avct_bcb_free_msg_ind(tAVCT_BCB *p_bcb, tAVCT_LCB_EVT *p_data)
{
    AVCT_TRACE_DEBUG("avct_bcb_free_msg_ind");
    if (p_data)
        osi_free_and_reset((void**)&p_data->p_buf);

}
#endif

/*******************************************************************************
**
** Function         avct_lcb_msg_ind
**
** Description      Handle an incoming AVCTP message.
**
**
** Returns          Nothing.
**
*******************************************************************************/
void avct_lcb_msg_ind(tAVCT_LCB *p_lcb, tAVCT_LCB_EVT *p_data)
{
    UINT8       *p;
    UINT8       label, type, cr_ipid;
    UINT16      pid;
    tAVCT_CCB   *p_ccb;

    /* this p_buf is to be reported through p_msg_cback. The layer_specific
     * needs to be set properly to indicate that it is received through
     * control channel */
    p_data->p_buf->layer_specific = AVCT_DATA_CTRL;

    /* reassemble message; if no message available (we received a fragment) return */
    if ((p_data->p_buf = avct_lcb_msg_asmbl(p_lcb, p_data->p_buf)) == NULL)
    {
        return;
    }

    p = (UINT8 *)(p_data->p_buf + 1) + p_data->p_buf->offset;

    /* parse header byte */
    AVCT_PRS_HDR(p, label, type, cr_ipid);
    UNUSED(type);

    /* check for invalid cr_ipid */
    if (cr_ipid == AVCT_CR_IPID_INVALID)
    {
        AVCT_TRACE_WARNING("Invalid cr_ipid", cr_ipid);
        osi_free_and_reset((void **)&p_data->p_buf);
        return;
    }

    /* parse and lookup PID */
    BE_STREAM_TO_UINT16(pid, p);
    if ((p_ccb = avct_lcb_has_pid(p_lcb, pid)) != NULL)
    {
        /* PID found; send msg up, adjust bt hdr and call msg callback */
        p_data->p_buf->offset += AVCT_HDR_LEN_SINGLE;
        p_data->p_buf->len -= AVCT_HDR_LEN_SINGLE;
        (*p_ccb->cc.p_msg_cback)(avct_ccb_to_idx(p_ccb), label, cr_ipid, p_data->p_buf);
        AVCT_TRACE_WARNING("avct_lcb_msg_ind p_ccb->cc.p_msg_cback end");
    }
    else
    {
        /* PID not found; drop message */
        AVCT_TRACE_WARNING("No ccb for PID=%x", pid);
        osi_free_and_reset((void **)&p_data->p_buf);

        /* if command send reject */
        if (cr_ipid == AVCT_CMD)
        {
            BT_HDR *p_buf = (BT_HDR *)osi_malloc(AVCT_CMD_BUF_SIZE);
            p_buf->len = AVCT_HDR_LEN_SINGLE;
            p_buf->offset = AVCT_MSG_OFFSET - AVCT_HDR_LEN_SINGLE;
            p = (UINT8 *)(p_buf + 1) + p_buf->offset;
            AVCT_BLD_HDR(p, label, AVCT_PKT_TYPE_SINGLE, AVCT_REJ);
            UINT16_TO_BE_STREAM(p, pid);
            L2CA_DataWrite(p_lcb->ch_lcid, p_buf);
        }
    }
}

#if (AVCT_BROWSE_INCLUDED == TRUE)
/*******************************************************************************
**
** Function         avct_bcb_msg_ind
**
** Description      Handle an incoming AVCTP message.
**
**
** Returns          Nothing.
**
*******************************************************************************/
void avct_bcb_msg_ind(tAVCT_BCB *p_bcb, tAVCT_LCB_EVT *p_data)
{
    UINT8       *p;
    UINT8        label, type, cr_ipid;
    UINT16       pid;
    tAVCT_LCB   *p_lcb;
    tAVCT_CCB   *p_ccb;
    BT_HDR      *p_buf;

    AVCT_TRACE_DEBUG("avct_bcb_msg_ind");
    /* Update layer specific information so that while
     * responding AVCT_MsgReq, AVCT layer knows to respond to
     * Browsing channel
    */
    p_data->p_buf->layer_specific = AVCT_DATA_BROWSE;

    /*
       AVCTP fragmentation shall not be used on the AVCTP Browsing Channel
    */

    if ((p_data->p_buf = avct_bcb_msg_asmbl(p_bcb, p_data->p_buf)) == NULL)
    {
        AVCT_TRACE_ERROR("### Error bcb_msg_asmbl");
        return;
    }

    /* Data passed is HCI+L2CAP+AVCT_AVRCP
     * Point to AVCT start
    */

    p = (UINT8 *)(p_data->p_buf + 1) + p_data->p_buf->offset;

    /* parse AVCT header byte */
    AVCT_PRS_HDR(p, label, type, cr_ipid);

    /* check for invalid cr_ipid */
    if (cr_ipid == AVCT_CR_IPID_INVALID)
    {
        AVCT_TRACE_WARNING("### Invalid cr_ipid", cr_ipid);
        osi_free_and_reset((void**)&p_data->p_buf);
        return;
    }
    /* parse and lookup PID */
    BE_STREAM_TO_UINT16(pid, p);
    p_lcb = avct_lcb_by_bcb(p_bcb);
    if (p_lcb == NULL)
    {
        AVCT_TRACE_ERROR("### Error lcb is NULL");
        osi_free_and_reset((void**)&p_data->p_buf);
    }
    else
    {
        AVCT_TRACE_DEBUG("p_lcb param: p[0]: %x, p[1]: %x,\
                  p[2] : %x, p[3] : %x ",p_lcb->peer_addr[0], p_lcb->peer_addr[1],p_lcb->peer_addr[2],\
                  p_lcb->peer_addr[3]);
        /* Irrespective of browsing or control
         * message recieved is passed on to above layer
        */

        /* Check if p_lcb is correct  */
        if ((p_ccb = avct_lcb_has_pid(p_lcb, pid)) != NULL)
        {
            /* PID found, send msg to upper laer, adjust
             * bt hdr and call msg callback
            */
            AVCT_TRACE_DEBUG("label: %x ,cr_ipid : %d ",label, cr_ipid );
            p_data->p_buf->offset += AVCT_HDR_LEN_SINGLE;
            p_data->p_buf->len    -= AVCT_HDR_LEN_SINGLE;
            (*p_ccb->cc.p_msg_cback)(avct_ccb_to_idx(p_ccb), label, cr_ipid, p_data->p_buf);
        }
        else
        {
            /* PID not found; drop message */
            AVCT_TRACE_WARNING("### No ccb for PID=%x", pid);
            osi_free_and_reset((void**)&p_data->p_buf);
            /* if command send reject */
            if (cr_ipid == AVCT_CMD)
            {
                if ((p_buf = (BT_HDR *) osi_malloc(AVCT_CMD_BUF_SIZE)) != NULL)
                {
                    p_buf->len = AVCT_HDR_LEN_SINGLE;
                    p_buf->offset = AVCT_MSG_OFFSET - AVCT_HDR_LEN_SINGLE;
                    p = (UINT8 *)(p_buf + 1) + p_buf->offset;
                    AVCT_BLD_HDR(p, label, AVCT_PKT_TYPE_SINGLE, AVCT_REJ);
                    UINT16_TO_BE_STREAM(p, pid);
                    L2CA_DataWrite(p_lcb->ch_lcid, p_buf);
                }
            }
        }
    }
}
#endif
