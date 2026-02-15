/******************************************************************************
 *
 *  Copyright (C) 2014  Broadcom Corporation
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

#include <string.h>
#include <pthread.h>

#include "bt_target.h"
#include "device/include/controller.h"
#include "stack_config.h"

#if (BLE_INCLUDED == TRUE)
#include "bt_types.h"
#include "hcimsgs.h"
#include "btu.h"
#include "btm_int.h"
#include "bt_utils.h"
#include "hcidefs.h"
#include "btm_ble_api.h"

/************************************************************************************
**  Constants & Macros
************************************************************************************/
/* length of each multi adv sub command */
#define BTM_BLE_MULTI_ADV_ENB_LEN                       3
#define BTM_BLE_MULTI_ADV_SET_PARAM_LEN                 24
#define BTM_BLE_MULTI_ADV_WRITE_DATA_LEN                (BTM_BLE_AD_DATA_LEN + 3)
#define BTM_BLE_MULTI_ADV_SET_RANDOM_ADDR_LEN           8
#define BTM_BLE_EXTENDED_ADV_TIMEOUT                    0x3C

#define BTM_BLE_MULTI_ADV_CB_EVT_MASK   0xF0
#define BTM_BLE_MULTI_ADV_SUBCODE_MASK  0x0F

#ifdef WIPOWER_SUPPORTED
#define WIPOWER_16_UUID_LSB 0xFE
#define WIPOWER_16_UUID_MSB 0xFF
static bool is_wipower_adv = false;
#endif

#define BTM_BLE_EXTENDED_LEGACY_MASK    0x10

/************************************************************************************
**  Static variables
************************************************************************************/
tBTM_BLE_MULTI_ADV_CB  btm_multi_adv_cb;
tBTM_BLE_MULTI_ADV_INST_IDX_Q btm_multi_adv_idx_q;
pthread_mutex_t btm_multi_adv_lock = PTHREAD_MUTEX_INITIALIZER;
tBTM_BLE_EXT_ADV_ENABLE_CB btm_ble_ext_enable_cb;

#ifdef WIPOWER_SUPPORTED
UINT8 wipower_inst_id = BTM_BLE_MULTI_ADV_DEFAULT_STD;
#endif

/************************************************************************************
**  Externs
************************************************************************************/
extern fixed_queue_t *btu_general_alarm_queue;
extern void btm_ble_update_dmt_flag_bits(UINT8 *flag_value,
                                               const UINT16 connect_mode, const UINT16 disc_mode);

static inline BOOLEAN is_btm_multi_adv_cb_valid()
{
    if (!btm_multi_adv_cb.p_adv_inst ||
        !btm_multi_adv_cb.op_q.p_sub_code ||
        !btm_multi_adv_cb.op_q.p_inst_id)
        return FALSE;
    else
        return TRUE;
}
#if (defined BLE_EXTENDED_ADV_SUPPORT && (BLE_EXTENDED_ADV_SUPPORT == TRUE))
/************************************************************************************
** Local declaration
************************************************************************************/
static tBTM_STATUS btm_ble_extended_adv_set_params (tBTM_BLE_MULTI_ADV_INST *p_inst,
                                             tBTM_BLE_ADV_PARAMS *p_params,
                                             UINT8 cb_evt);
static tBTM_STATUS btm_ble_enable_extended_adv (BOOLEAN enable, UINT8 inst_id, UINT16 duration, UINT8 max_ext_adv_evts, UINT8 cb_evt);
#endif

/*******************************************************************************
**
** Function         btm_ble_multi_adv_enq_op_q
**
** Description      enqueue a multi adv operation in q to check command complete
**                  status.
**
** Returns          void
**
*******************************************************************************/
void btm_ble_multi_adv_enq_op_q(UINT8 opcode, UINT8 inst_id, UINT8 cb_evt)
{
    tBTM_BLE_MULTI_ADV_OPQ  *p_op_q = &btm_multi_adv_cb.op_q;

    p_op_q->p_inst_id[p_op_q->next_idx] = inst_id;

    p_op_q->p_sub_code[p_op_q->next_idx] = (opcode |(cb_evt << 4));

    p_op_q->next_idx = (p_op_q->next_idx + 1) %  BTM_BleMaxMultiAdvInstanceCount();
}

/*******************************************************************************
**
** Function         btm_ble_multi_adv_deq_op_q
**
** Description      dequeue a multi adv operation from q when command complete
**                  is received.
**
** Returns          void
**
*******************************************************************************/
void btm_ble_multi_adv_deq_op_q(UINT8 *p_opcode, UINT8 *p_inst_id, UINT8 *p_cb_evt)
{
    tBTM_BLE_MULTI_ADV_OPQ  *p_op_q = &btm_multi_adv_cb.op_q;

    *p_inst_id = p_op_q->p_inst_id[p_op_q->pending_idx] & 0x7F;
    *p_cb_evt = (p_op_q->p_sub_code[p_op_q->pending_idx] >> 4);
    *p_opcode = (p_op_q->p_sub_code[p_op_q->pending_idx] & BTM_BLE_MULTI_ADV_SUBCODE_MASK);

    p_op_q->pending_idx = (p_op_q->pending_idx + 1) %  BTM_BleMaxMultiAdvInstanceCount();
}

/*******************************************************************************
**
** Function         btm_ble_multi_adv_vsc_cmpl_cback
**
** Description      Multi adv VSC complete callback
**
** Parameters
**
** Returns          void
**
*******************************************************************************/
void btm_ble_multi_adv_vsc_cmpl_cback (tBTM_VSC_CMPL *p_params)
{
    UINT8  status, subcode;
    UINT8  *p = p_params->p_param_buf, inst_id;
    UINT16  len = p_params->param_len;
    tBTM_BLE_MULTI_ADV_INST *p_inst ;
    UINT8   cb_evt = 0, opcode;

    if (len  < 2)
    {
        BTM_TRACE_ERROR("wrong length for btm_ble_multi_adv_vsc_cmpl_cback");
        return;
    }

    STREAM_TO_UINT8(status, p);
    STREAM_TO_UINT8(subcode, p);

    pthread_mutex_lock(&btm_multi_adv_lock);
    if (!is_btm_multi_adv_cb_valid())
        goto error;
    btm_ble_multi_adv_deq_op_q(&opcode, &inst_id, &cb_evt);

    BTM_TRACE_DEBUG("op_code = %02x inst_id = %d cb_evt = %02x", opcode, inst_id, cb_evt);

    if (opcode != subcode || inst_id == 0)
    {
        BTM_TRACE_ERROR("get unexpected VSC cmpl, expect: %d get: %d",subcode,opcode);
        goto error;
    }

    p_inst = &btm_multi_adv_cb.p_adv_inst[inst_id - 1];

    switch (subcode)
    {
        case BTM_BLE_MULTI_ADV_ENB:
        {
            BTM_TRACE_DEBUG("BTM_BLE_MULTI_ADV_ENB status = %d", status);

            /* Mark as not in use here, if instance cannot be enabled */
            if (HCI_SUCCESS != status && BTM_BLE_MULTI_ADV_ENB_EVT == cb_evt)
                btm_multi_adv_cb.p_adv_inst[inst_id-1].in_use = FALSE;
            break;
        }

        case BTM_BLE_MULTI_ADV_SET_PARAM:
        {
            BTM_TRACE_DEBUG("BTM_BLE_MULTI_ADV_SET_PARAM status = %d", status);
            break;
        }

        case BTM_BLE_MULTI_ADV_WRITE_ADV_DATA:
        {
            BTM_TRACE_DEBUG("BTM_BLE_MULTI_ADV_WRITE_ADV_DATA status = %d", status);
            break;
        }

        case BTM_BLE_MULTI_ADV_WRITE_SCAN_RSP_DATA:
        {
            BTM_TRACE_DEBUG("BTM_BLE_MULTI_ADV_WRITE_SCAN_RSP_DATA status = %d", status);
            break;
        }

        case BTM_BLE_MULTI_ADV_SET_RANDOM_ADDR:
        {
            BTM_TRACE_DEBUG("BTM_BLE_MULTI_ADV_SET_RANDOM_ADDR status = %d", status);
            break;
        }

        default:
            break;
    }

    if (cb_evt != 0 && p_inst->p_cback != NULL)
    {
        (p_inst->p_cback)(cb_evt, inst_id, p_inst->p_ref, status);
    }

error:
    pthread_mutex_unlock(&btm_multi_adv_lock);
    return;
}

/*******************************************************************************
**
** Function         btm_ble_enable_multi_adv
**
** Description      This function enable the customer specific feature in controller
**
** Parameters       enable: enable or disable
**                  inst_id:    adv instance ID, can not be 0
**
** Returns          status
**
*******************************************************************************/
tBTM_STATUS btm_ble_enable_multi_adv (BOOLEAN enable, UINT8 inst_id, UINT8 cb_evt)
{
    UINT8           param[BTM_BLE_MULTI_ADV_ENB_LEN], *pp;
    UINT8           enb = enable ? 1: 0;
    tBTM_STATUS     rt;

    pp = param;
    memset(param, 0, BTM_BLE_MULTI_ADV_ENB_LEN);

    UINT8_TO_STREAM (pp, BTM_BLE_MULTI_ADV_ENB);
    UINT8_TO_STREAM (pp, enb);
    UINT8_TO_STREAM (pp, inst_id);

    BTM_TRACE_EVENT (" btm_ble_enable_multi_adv: enb %d, Inst ID %d",enb,inst_id);

    if ((rt = BTM_VendorSpecificCommand (HCI_BLE_MULTI_ADV_OCF,
                                    BTM_BLE_MULTI_ADV_ENB_LEN,
                                    param,
                                    btm_ble_multi_adv_vsc_cmpl_cback))
                                     == BTM_CMD_STARTED)
    {
        btm_ble_multi_adv_enq_op_q(BTM_BLE_MULTI_ADV_ENB, inst_id, cb_evt);
    }
    return rt;
}
/*******************************************************************************
**
** Function         btm_ble_map_adv_tx_power
**
** Description      return the actual power in dBm based on the mapping in config file
**
** Parameters       advertise parameters used for this instance.
**
** Returns          tx power in dBm
**
*******************************************************************************/
int btm_ble_tx_power[BTM_BLE_ADV_TX_POWER_MAX + 1] = BTM_BLE_ADV_TX_POWER;
char btm_ble_map_adv_tx_power(int tx_power_index)
{
    if(0 <= tx_power_index && tx_power_index < BTM_BLE_ADV_TX_POWER_MAX)
        return (char)btm_ble_tx_power[tx_power_index];
    return 0;
}
/*******************************************************************************
**
** Function         btm_ble_multi_adv_set_params
**
** Description      This function enable the customer specific feature in controller
**
** Parameters       advertise parameters used for this instance.
**
** Returns          status
**
*******************************************************************************/
tBTM_STATUS btm_ble_multi_adv_set_params (tBTM_BLE_MULTI_ADV_INST *p_inst,
                                          tBTM_BLE_ADV_PARAMS *p_params,
                                          UINT8 cb_evt)
{
    UINT8           param[BTM_BLE_MULTI_ADV_SET_PARAM_LEN], *pp;
    tBTM_STATUS     rt;
    BD_ADDR         dummy ={0,0,0,0,0,0};

    pp = param;
    memset(param, 0, BTM_BLE_MULTI_ADV_SET_PARAM_LEN);

    UINT8_TO_STREAM(pp, BTM_BLE_MULTI_ADV_SET_PARAM);

    UINT16_TO_STREAM (pp, p_params->adv_int_min);
    UINT16_TO_STREAM (pp, p_params->adv_int_max);
    UINT8_TO_STREAM  (pp, p_params->adv_type);

#if (defined BLE_PRIVACY_SPT && BLE_PRIVACY_SPT == TRUE)
    if (btm_cb.ble_ctr_cb.privacy_mode != BTM_PRIVACY_NONE)
    {
        UINT8_TO_STREAM  (pp, BLE_ADDR_RANDOM);
        BDADDR_TO_STREAM (pp, p_inst->rpa);
    }
    else
#endif
    {
        UINT8_TO_STREAM  (pp, BLE_ADDR_PUBLIC);
        BDADDR_TO_STREAM (pp, controller_get_interface()->get_address()->address);
    }

    BTM_TRACE_EVENT (" btm_ble_multi_adv_set_params,Min %d, Max %d,adv_type %d",
        p_params->adv_int_min,p_params->adv_int_max,p_params->adv_type);

    UINT8_TO_STREAM  (pp, 0);
    BDADDR_TO_STREAM (pp, dummy);

    if (p_params->channel_map == 0 || p_params->channel_map > BTM_BLE_DEFAULT_ADV_CHNL_MAP)
        p_params->channel_map = BTM_BLE_DEFAULT_ADV_CHNL_MAP;
    UINT8_TO_STREAM (pp, p_params->channel_map);

    if (p_params->adv_filter_policy >= AP_SCAN_CONN_POLICY_MAX)
        p_params->adv_filter_policy = AP_SCAN_CONN_ALL;
    UINT8_TO_STREAM (pp, p_params->adv_filter_policy);

    UINT8_TO_STREAM (pp, p_inst->inst_id);

    if (p_params->tx_power > BTM_BLE_ADV_TX_POWER_MAX)
        p_params->tx_power = BTM_BLE_ADV_TX_POWER_MAX;
    UINT8_TO_STREAM (pp, btm_ble_map_adv_tx_power(p_params->tx_power));

    BTM_TRACE_EVENT("set_params:Chnl Map %d,adv_fltr policy %d,ID:%d, TX Power%d",
        p_params->channel_map,p_params->adv_filter_policy,p_inst->inst_id,p_params->tx_power);

    if ((rt = BTM_VendorSpecificCommand (HCI_BLE_MULTI_ADV_OCF,
                                    BTM_BLE_MULTI_ADV_SET_PARAM_LEN,
                                    param,
                                    btm_ble_multi_adv_vsc_cmpl_cback))
           == BTM_CMD_STARTED)
    {
        p_inst->adv_evt = p_params->adv_type;

#if (defined BLE_PRIVACY_SPT && BLE_PRIVACY_SPT == TRUE)
        if (btm_cb.ble_ctr_cb.privacy_mode != BTM_PRIVACY_NONE) {
            alarm_set_on_queue(p_inst->adv_raddr_timer,
                               BTM_BLE_PRIVATE_ADDR_INT_MS,
                               btm_ble_adv_raddr_timer_timeout, p_inst,
                               btu_general_alarm_queue);
        }
#endif
        btm_ble_multi_adv_enq_op_q(BTM_BLE_MULTI_ADV_SET_PARAM, p_inst->inst_id, cb_evt);
    }
    return rt;
}

/*******************************************************************************
**
** Function         btm_ble_multi_adv_write_rpa
**
** Description      This function write the random address for the adv instance into
**                  controller
**
** Parameters
**
** Returns          status
**
*******************************************************************************/
tBTM_STATUS btm_ble_multi_adv_write_rpa (tBTM_BLE_MULTI_ADV_INST *p_inst, BD_ADDR random_addr)
{
    UINT8           param[BTM_BLE_MULTI_ADV_SET_RANDOM_ADDR_LEN], *pp = param;
    tBTM_STATUS     rt = BTM_NO_RESOURCES;

    BTM_TRACE_EVENT ("%s-BD_ADDR:%02x-%02x-%02x-%02x-%02x-%02x,inst_id:%d",
                      __FUNCTION__, random_addr[5], random_addr[4], random_addr[3], random_addr[2],
                      random_addr[1], random_addr[0], p_inst->inst_id);

#if (defined BLE_EXTENDED_ADV_SUPPORT && (BLE_EXTENDED_ADV_SUPPORT == TRUE))
    if (controller_get_interface()->supports_ble_extended_advertisements())
    {
        rt = btm_ble_add_multi_adv_rpa(random_addr, BLE_ADDR_RANDOM);
        return rt;
    }
    else
#endif
    {
        memset(param, 0, BTM_BLE_MULTI_ADV_SET_RANDOM_ADDR_LEN);

        UINT8_TO_STREAM (pp, BTM_BLE_MULTI_ADV_SET_RANDOM_ADDR);
        BDADDR_TO_STREAM(pp, random_addr);
        UINT8_TO_STREAM(pp,  p_inst->inst_id);
        rt = BTM_VendorSpecificCommand (HCI_BLE_MULTI_ADV_OCF,
                                    BTM_BLE_MULTI_ADV_SET_RANDOM_ADDR_LEN,
                                    param,
                                    btm_ble_multi_adv_vsc_cmpl_cback);
    }


    if (rt == BTM_CMD_STARTED)
    {
        /* start a periodical timer to refresh random addr */
        /* TODO: is the above comment correct - is the timer periodical? */
        alarm_set_on_queue(p_inst->adv_raddr_timer,
                           BTM_BLE_PRIVATE_ADDR_INT_MS,
                           btm_ble_adv_raddr_timer_timeout, p_inst,
                           btu_general_alarm_queue);
        btm_ble_multi_adv_enq_op_q(BTM_BLE_MULTI_ADV_SET_RANDOM_ADDR,
                                   p_inst->inst_id, 0);
    }
    return rt;
}

/*******************************************************************************
**
** Function         btm_ble_multi_adv_gen_rpa_cmpl
**
** Description      RPA generation completion callback for each adv instance. Will
**                  continue write the new RPA into controller.
**
** Returns          none.
**
*******************************************************************************/
void btm_ble_multi_adv_gen_rpa_cmpl(tBTM_RAND_ENC *p)
{
#if (SMP_INCLUDED == TRUE)
    tSMP_ENC    output;
    UINT8 index = 0;
    tBTM_BLE_MULTI_ADV_INST *p_inst = NULL;

     /* Retrieve the index of adv instance from stored Q */
    if (btm_multi_adv_idx_q.front == -1)
    {
        BTM_TRACE_ERROR(" %s can't locate advertise instance", __FUNCTION__);
        return;
    }
    else
    {
        index = btm_multi_adv_idx_q.inst_index_queue[btm_multi_adv_idx_q.front];
        if (btm_multi_adv_idx_q.front == btm_multi_adv_idx_q.rear)
        {
            btm_multi_adv_idx_q.front = -1;
            btm_multi_adv_idx_q.rear = -1;
        }
        else
        {
            btm_multi_adv_idx_q.front = (btm_multi_adv_idx_q.front + 1) % BTM_BLE_MULTI_ADV_MAX;
        }
    }

    pthread_mutex_lock(&btm_multi_adv_lock);
    if (!is_btm_multi_adv_cb_valid())
        goto error;
    p_inst = &(btm_multi_adv_cb.p_adv_inst[index]);

    BTM_TRACE_EVENT ("btm_ble_multi_adv_gen_rpa_cmpl inst_id = %d", p_inst->inst_id);
    if (p)
    {
        p->param_buf[2] &= (~BLE_RESOLVE_ADDR_MASK);
        p->param_buf[2] |= BLE_RESOLVE_ADDR_MSB;

        p_inst->rpa[2] = p->param_buf[0];
        p_inst->rpa[1] = p->param_buf[1];
        p_inst->rpa[0] = p->param_buf[2];

        if (!SMP_Encrypt(btm_cb.devcb.id_keys.irk, BT_OCTET16_LEN, p->param_buf, 3, &output))
        {
            BTM_TRACE_DEBUG("generate random address failed");
        }
        else
        {
            /* set hash to be LSB of rpAddress */
            p_inst->rpa[5] = output.param_buf[0];
            p_inst->rpa[4] = output.param_buf[1];
            p_inst->rpa[3] = output.param_buf[2];
        }

        if (p_inst->inst_id != BTM_BLE_MULTI_ADV_DEFAULT_STD &&
            p_inst->inst_id < BTM_BleMaxMultiAdvInstanceCount())
        {
            /* set it to controller */
            btm_ble_multi_adv_write_rpa(p_inst, p_inst->rpa);
        }
    }
error:
    pthread_mutex_unlock(&btm_multi_adv_lock);
#endif
}

/*******************************************************************************
**
** Function         btm_ble_multi_adv_configure_rpa
**
** Description      This function set the random address for the adv instance
**
** Parameters       advertise parameters used for this instance.
**
** Returns          none
**
*******************************************************************************/
void btm_ble_multi_adv_configure_rpa (tBTM_BLE_MULTI_ADV_INST *p_inst)
{
    if (btm_multi_adv_idx_q.front == (btm_multi_adv_idx_q.rear + 1) % BTM_BLE_MULTI_ADV_MAX)
    {
        BTM_TRACE_ERROR("outstanding rand generation exceeded max allowed ");
        return;
    }
    else
    {
        if (btm_multi_adv_idx_q.front == -1)
        {
            btm_multi_adv_idx_q.front = 0;
            btm_multi_adv_idx_q.rear = 0;
        }
        else
        {
            btm_multi_adv_idx_q.rear = (btm_multi_adv_idx_q.rear + 1) % BTM_BLE_MULTI_ADV_MAX;
        }
        btm_multi_adv_idx_q.inst_index_queue[btm_multi_adv_idx_q.rear] = p_inst->index;
    }
    btm_gen_resolvable_private_addr((void *)btm_ble_multi_adv_gen_rpa_cmpl);
}

/*******************************************************************************
**
** Function         btm_ble_update_multi_adv_inst_data_length
**
** Description      This function set the random address for the adv instance
**
** Parameters       advertise parameters used for this instance.
**
** Returns          none
**
*******************************************************************************/
void btm_ble_update_multi_adv_inst_data_length (UINT16 inst_len)
{
    UINT8 index;
    tBTM_BLE_MULTI_ADV_INST *p_inst = &btm_multi_adv_cb.p_adv_inst[0];
    BTM_TRACE_ERROR("btm_ble_update_multi_adv_inst_data_length inst_len:%d",inst_len);

    for (index = 0; index <  BTM_BleMaxMultiAdvInstanceCount() - 1; index++, p_inst++)
    {
        p_inst->len = inst_len;
    }
}

/*******************************************************************************
**
** Function         btm_ble_multi_adv_reenable
**
** Description      This function re-enable adv instance upon a connection establishment.
**
** Parameters       advertise parameters used for this instance.
**
** Returns          none.
**
*******************************************************************************/
void btm_ble_multi_adv_reenable(UINT8 inst_id)
{
    tBTM_BLE_MULTI_ADV_INST *p_inst = &btm_multi_adv_cb.p_adv_inst[inst_id - 1];

    if (TRUE == p_inst->in_use)
    {
        if (p_inst->adv_evt != BTM_BLE_CONNECT_DIR_EVT)
            btm_ble_enable_multi_adv (TRUE, p_inst->inst_id, 0);
        else
          /* mark directed adv as disabled if adv has been stopped */
        {
            (p_inst->p_cback)(BTM_BLE_MULTI_ADV_DISABLE_EVT,p_inst->inst_id,p_inst->p_ref,0);
             p_inst->in_use = FALSE;
        }
     }
}

/*******************************************************************************
**
** Function         btm_ble_multi_adv_enb_privacy
**
** Description      This function enable/disable privacy setting in multi adv
**
** Parameters       enable: enable or disable the adv instance.
**
** Returns          none.
**
*******************************************************************************/
void btm_ble_multi_adv_enb_privacy(BOOLEAN enable)
{
    UINT8 i;
    tBTM_BLE_MULTI_ADV_INST *p_inst = &btm_multi_adv_cb.p_adv_inst[0];

    for (i = 0; i <  BTM_BleMaxMultiAdvInstanceCount() - 1; i ++, p_inst++)
    {
        p_inst->in_use = FALSE;
        if (enable)
            btm_ble_multi_adv_configure_rpa(p_inst);
        else
            alarm_cancel(p_inst->adv_raddr_timer);
    }
}

#if (defined BLE_EXTENDED_ADV_SUPPORT && (BLE_EXTENDED_ADV_SUPPORT == TRUE))
/*******************************************************************************
**
** Function         btm_ble_extended_configure_inst_size
**
** Description      This function gathers max length of advertiement sets
**
** Parameters       none.
**
** Returns          none.
**
*******************************************************************************/
void btm_ble_extended_configure_inst_size()
{
    BTM_TRACE_DEBUG ("%s", __func__);
    btsnd_hcic_ble_read_extended_max_adv_len();
}
#endif

/*******************************************************************************
**
** Function         BTM_BleEnableAdvInstance
**
** Description      This function enable a Multi-ADV instance with the specified
**                  adv parameters
**
** Parameters       p_params: pointer to the adv parameter structure, set as default
**                            adv parameter when the instance is enabled.
**                  p_cback: callback function for the adv instance.
**                  p_ref:  reference data attach to the adv instance to be enabled.
**
** Returns          status
**
*******************************************************************************/
tBTM_STATUS BTM_BleEnableAdvInstance (tBTM_BLE_ADV_PARAMS *p_params,
                                      tBTM_BLE_MULTI_ADV_CBACK *p_cback,void *p_ref)
{
    UINT8 i;
    tBTM_STATUS rt = BTM_NO_RESOURCES;
    tBTM_BLE_MULTI_ADV_INST *p_inst = &btm_multi_adv_cb.p_adv_inst[0];

    BTM_TRACE_EVENT("BTM_BleEnableAdvInstance called");

    if (0 == BTM_BleMaxMultiAdvInstanceCount())
    {
        BTM_TRACE_ERROR("Controller does not support Multi ADV");
        return BTM_ERR_PROCESSING;
    }

    if (NULL == p_inst)
    {
        BTM_TRACE_ERROR("Invalid instance in BTM_BleEnableAdvInstance");
        return BTM_ERR_PROCESSING;
    }

    for (i = 0; i <  BTM_BleMaxMultiAdvInstanceCount() - 1; i ++, p_inst++)
    {
        if (FALSE == p_inst->in_use)
        {
            p_inst->in_use = TRUE;
            /* configure adv parameter */
            if (p_params) {
#if (defined BLE_EXTENDED_ADV_SUPPORT && (BLE_EXTENDED_ADV_SUPPORT == TRUE))
                if (controller_get_interface()->supports_ble_extended_advertisements())
                {
                    rt = btm_ble_extended_adv_set_params(p_inst, p_params, 0);
                }
                else
#endif
                {
                    rt = btm_ble_multi_adv_set_params(p_inst, p_params, 0);
                }
            }
            else {
                BTM_TRACE_ERROR("Invalid params value in BTM_BleEnableAdvInstance");
                return BTM_ILLEGAL_VALUE;
            }

            /* enable adv */
            BTM_TRACE_EVENT("btm_ble_enable_multi_adv being called with inst_id:%d",
                p_inst->inst_id);

            if (BTM_CMD_STARTED == rt)
            {
#if (defined BLE_EXTENDED_ADV_SUPPORT && (BLE_EXTENDED_ADV_SUPPORT == TRUE))
                if (controller_get_interface()->supports_ble_extended_advertisements()) {
                    p_inst->in_use = FALSE;
                    btm_ble_enable_resolving_list (BTM_BLE_RL_EXT_ADV);
                    p_inst->in_use = TRUE;
                    rt = btm_ble_enable_extended_adv (TRUE, p_inst->inst_id,
                                                      p_inst->duration, p_params->max_ext_adv_evts, BTM_BLE_EXTENDED_ADV_ENB_EVT);
                }
                else
#endif
                {
                    rt = btm_ble_enable_multi_adv (TRUE, p_inst->inst_id,
                                                    BTM_BLE_MULTI_ADV_ENB_EVT);
                }

                if (rt == BTM_CMD_STARTED)
                {
                    p_inst->p_cback = p_cback;
                    p_inst->p_ref   = p_ref;
                }
            }

            if (BTM_CMD_STARTED != rt)
            {
                p_inst->in_use = FALSE;
                BTM_TRACE_ERROR("BTM_BleEnableAdvInstance failed");
            }
            break;
        }
    }
    return rt;
}

/*******************************************************************************
**
** Function         BTM_BleUpdateAdvInstParam
**
** Description      This function update a Multi-ADV instance with the specified
**                  adv parameters.
**
** Parameters       inst_id: adv instance ID
**                  p_params: pointer to the adv parameter structure.
**
** Returns          status
**
*******************************************************************************/
tBTM_STATUS BTM_BleUpdateAdvInstParam (UINT8 inst_id, tBTM_BLE_ADV_PARAMS *p_params)
{
    tBTM_STATUS rt = BTM_ILLEGAL_VALUE;
    tBTM_BLE_MULTI_ADV_INST *p_inst = &btm_multi_adv_cb.p_adv_inst[inst_id - 1];
    UINT8 cb_evt = BTM_BLE_MULTI_ADV_PARAM_EVT;

    BTM_TRACE_EVENT("BTM_BleUpdateAdvInstParam called with inst_id:%d", inst_id);

    if (0 == BTM_BleMaxMultiAdvInstanceCount())
    {
        BTM_TRACE_ERROR("Controller does not support Multi ADV");
        return BTM_ERR_PROCESSING;
    }

    if (inst_id <  BTM_BleMaxMultiAdvInstanceCount() &&
        inst_id != BTM_BLE_MULTI_ADV_DEFAULT_STD &&
        p_params != NULL)
    {
        if (FALSE == p_inst->in_use)
        {
            BTM_TRACE_DEBUG("adv instance %d is not active", inst_id);
            return BTM_WRONG_MODE;
        }
        else {
#if (defined BLE_EXTENDED_ADV_SUPPORT && (BLE_EXTENDED_ADV_SUPPORT == TRUE))
            if (controller_get_interface()->supports_ble_extended_advertisements())
                btm_ble_enable_extended_adv(FALSE, inst_id, 0, 0/*p_params->max_ext_adv_evts*/, 0);
            else
#endif
            {
                btm_ble_enable_multi_adv(FALSE, inst_id, 0);
            }
        }

#if (defined BLE_EXTENDED_ADV_SUPPORT && (BLE_EXTENDED_ADV_SUPPORT == TRUE))
        if (controller_get_interface()->supports_ble_extended_advertisements())
        {
            rt = btm_ble_extended_adv_set_params(p_inst, p_params, 0);
            cb_evt = BTM_BLE_EXTENDED_ADV_PARAM_EVT;
        }
        else
#endif
        {
            rt = btm_ble_multi_adv_set_params(p_inst, p_params, 0);
        }

        if (BTM_CMD_STARTED == rt) {
#if (defined BLE_EXTENDED_ADV_SUPPORT && (BLE_EXTENDED_ADV_SUPPORT == TRUE))
            if (controller_get_interface()->supports_ble_extended_advertisements())
                rt = btm_ble_enable_extended_adv(TRUE, inst_id, p_inst->duration, p_params->max_ext_adv_evts, cb_evt);
            else
#endif
            {
                rt = btm_ble_enable_multi_adv(TRUE, inst_id, cb_evt);
            }
        }
    }
    return rt;
}

/*******************************************************************************
**
** Function         BTM_BleCfgAdvInstData
**
** Description      This function configure a Multi-ADV instance with the specified
**                  adv data or scan response data.
**
** Parameters       inst_id: adv instance ID
**                  is_scan_rsp: is this scan response. if no, set as adv data.
**                  data_mask: adv data mask.
**                  p_data: pointer to the adv data structure.
**
** Returns          status
**
*******************************************************************************/
tBTM_STATUS BTM_BleCfgAdvInstData (UINT8 inst_id, BOOLEAN is_scan_rsp,
                                    tBTM_BLE_AD_MASK data_mask, UINT8 frag_pref,
                                    tBTM_BLE_ADV_DATA *p_data)
{
    UINT8       param[BTM_BLE_MULTI_ADV_WRITE_DATA_LEN], *pp = param;
    UINT8       sub_code = (is_scan_rsp) ?
                           BTM_BLE_MULTI_ADV_WRITE_SCAN_RSP_DATA : BTM_BLE_MULTI_ADV_WRITE_ADV_DATA;
    UINT8       *p_len;
    tBTM_STATUS rt;
    UINT8 *pp_temp = (UINT8*)(param + BTM_BLE_MULTI_ADV_WRITE_DATA_LEN -1);
    tBTM_BLE_VSC_CB cmn_ble_vsc_cb;

#if (defined BLE_EXTENDED_ADV_SUPPORT && (BLE_EXTENDED_ADV_SUPPORT == TRUE))
    if (controller_get_interface()->supports_ble_extended_advertisements())
    {
        return BTM_BleWriteExtendedAdvData (inst_id, is_scan_rsp, data_mask,
                                            BTM_BLE_EXT_ADV_COMPLETE ,frag_pref, p_data);
    }
#endif

    BTM_BleGetVendorCapabilities(&cmn_ble_vsc_cb);
    if (0 == BTM_BleMaxMultiAdvInstanceCount())
    {
        BTM_TRACE_ERROR("Controller does not support Multi ADV");
        return BTM_ERR_PROCESSING;
    }

    btm_ble_update_dmt_flag_bits(&p_data->flag, btm_cb.btm_inq_vars.connectable_mode,
                                        btm_cb.btm_inq_vars.discoverable_mode);

    BTM_TRACE_EVENT("BTM_BleCfgAdvInstData called with inst_id:%d", inst_id);
    if (inst_id > BTM_BLE_MULTI_ADV_MAX || inst_id == BTM_BLE_MULTI_ADV_DEFAULT_STD)
        return BTM_ILLEGAL_VALUE;

    memset(param, 0, BTM_BLE_MULTI_ADV_WRITE_DATA_LEN);

    UINT8_TO_STREAM(pp, sub_code);
    p_len = pp ++;
    btm_ble_build_adv_data(&data_mask, &pp, p_data, BTM_BLE_AD_DATA_LEN);
    *p_len = (UINT8)(pp - param - 2);
    UINT8_TO_STREAM(pp_temp, inst_id);
#ifdef WIPOWER_SUPPORTED
    if (param[7] == WIPOWER_16_UUID_LSB && param[8] == WIPOWER_16_UUID_MSB)
    {
        is_wipower_adv = true;
        wipower_inst_id = inst_id;
    }
#endif

    if ((rt = BTM_VendorSpecificCommand (HCI_BLE_MULTI_ADV_OCF,
                                    (UINT8)BTM_BLE_MULTI_ADV_WRITE_DATA_LEN,
                                    param,
                                    btm_ble_multi_adv_vsc_cmpl_cback))
                                     == BTM_CMD_STARTED)
    {
        btm_ble_multi_adv_enq_op_q(sub_code, inst_id, BTM_BLE_MULTI_ADV_DATA_EVT);
    }
    return rt;
}

/*******************************************************************************
**
** Function         BTM_BleDisableAdvInstance
**
** Description      This function disables a Multi-ADV instance.
**
** Parameters       inst_id: adv instance ID
**
** Returns          status
**
*******************************************************************************/
tBTM_STATUS BTM_BleDisableAdvInstance (UINT8 inst_id)
{
    tBTM_STATUS rt = BTM_ILLEGAL_VALUE;
    tBTM_BLE_VSC_CB cmn_ble_vsc_cb;

    BTM_TRACE_EVENT("BTM_BleDisableAdvInstance with inst_id:%d", inst_id);

    BTM_BleGetVendorCapabilities(&cmn_ble_vsc_cb);

    if (0 == BTM_BleMaxMultiAdvInstanceCount())
    {
        BTM_TRACE_ERROR("Controller does not support Multi ADV");
        return BTM_ERR_PROCESSING;
    }

    if (inst_id < BTM_BleMaxMultiAdvInstanceCount() &&
        inst_id != BTM_BLE_MULTI_ADV_DEFAULT_STD)
    {
#if (defined BLE_EXTENDED_ADV_SUPPORT && (BLE_EXTENDED_ADV_SUPPORT == TRUE))
        tBTM_BLE_MULTI_ADV_INST *p_inst = &btm_multi_adv_cb.p_adv_inst[inst_id - 1];
        if (controller_get_interface()->supports_ble_extended_advertisements()) {
            rt = btm_ble_enable_extended_adv (FALSE, inst_id,
                                                     p_inst->duration, 0/*p_params->max_ext_adv_evts*/,
                                                     BTM_BLE_MULTI_ADV_DISABLE_EVT);
        }
        else
#endif
        {
            rt = btm_ble_enable_multi_adv(FALSE, inst_id, BTM_BLE_MULTI_ADV_DISABLE_EVT);
        }

        if (rt == BTM_CMD_STARTED
#if (defined BLE_EXTENDED_ADV_SUPPORT && (BLE_EXTENDED_ADV_SUPPORT == TRUE))
            && !controller_get_interface()->supports_ble_extended_advertisements()
#endif
           )
        {
            btm_ble_multi_adv_configure_rpa(&btm_multi_adv_cb.p_adv_inst[inst_id - 1]);
            alarm_cancel(btm_multi_adv_cb.p_adv_inst[inst_id - 1].adv_raddr_timer);
        }
        btm_multi_adv_cb.p_adv_inst[inst_id - 1].in_use = FALSE;
     }
    return rt;
}
/*******************************************************************************
**
** Function         btm_ble_multi_adv_vse_cback
**
** Description      VSE callback for multi adv events.
**
** Returns
**
*******************************************************************************/
void btm_ble_multi_adv_vse_cback(UINT8 len, UINT8 *p)
{
    UINT8   sub_event;
    UINT8   adv_inst, idx;
    UINT16  conn_handle;

    /* Check if this is a BLE RSSI vendor specific event */
    STREAM_TO_UINT8(sub_event, p);
    len--;

    BTM_TRACE_EVENT("btm_ble_multi_adv_vse_cback called with event:%d", sub_event);
    if ((sub_event == HCI_VSE_SUBCODE_BLE_MULTI_ADV_ST_CHG) && (len >= 4))
    {
        STREAM_TO_UINT8(adv_inst, p);
        ++p;
        STREAM_TO_UINT16(conn_handle, p);

        if ((idx = btm_handle_to_acl_index(conn_handle)) != MAX_L2CAP_LINKS)
        {
#if (defined BLE_PRIVACY_SPT && BLE_PRIVACY_SPT == TRUE)
            if (btm_cb.ble_ctr_cb.privacy_mode != BTM_PRIVACY_NONE &&
                adv_inst <= BTM_BLE_MULTI_ADV_MAX && adv_inst !=  BTM_BLE_MULTI_ADV_DEFAULT_STD)
            {
                memcpy(btm_cb.acl_db[idx].conn_addr, btm_multi_adv_cb.p_adv_inst[adv_inst - 1].rpa,
                                BD_ADDR_LEN);
            }
#endif
        }

        if (adv_inst < BTM_BleMaxMultiAdvInstanceCount() &&
            adv_inst !=  BTM_BLE_MULTI_ADV_DEFAULT_STD)
        {
            BTM_TRACE_EVENT("btm_ble_multi_adv_reenable called");
#ifdef WIPOWER_SUPPORTED
            if (!(is_wipower_adv && (adv_inst == wipower_inst_id))) {
                btm_ble_multi_adv_reenable(adv_inst);
            }
#else
                btm_ble_multi_adv_reenable(adv_inst);
#endif
        }
        /* re-enable connectibility */
        else if (adv_inst == BTM_BLE_MULTI_ADV_DEFAULT_STD)
        {
            if (btm_cb.ble_ctr_cb.inq_var.connectable_mode == BTM_BLE_CONNECTABLE)
            {
                btm_ble_set_connectability ( btm_cb.ble_ctr_cb.inq_var.connectable_mode );
            }
        }

    }

}
/*******************************************************************************
**
** Function         btm_ble_multi_adv_init
**
** Description      This function initialize the multi adv control block.
**
** Parameters       UINT8 max_adv_inst
**
** Returns          void
**
*******************************************************************************/
void btm_ble_multi_adv_init(UINT8 max_adv_inst)
{
    BTM_TRACE_ERROR("%s: max adv instances: %d", __func__, max_adv_inst);
    UINT8 i = 0;
    memset(&btm_multi_adv_cb, 0, sizeof(tBTM_BLE_MULTI_ADV_CB));
    memset (&btm_multi_adv_idx_q,0, sizeof (tBTM_BLE_MULTI_ADV_INST_IDX_Q));

    btm_multi_adv_idx_q.front = -1;
    btm_multi_adv_idx_q.rear = -1;

    if (max_adv_inst > 0)
    {
        btm_multi_adv_cb.p_adv_inst = osi_calloc(sizeof(tBTM_BLE_MULTI_ADV_INST) *
                                                 (max_adv_inst));

        btm_multi_adv_cb.op_q.p_sub_code = osi_calloc(sizeof(UINT8) *
                                                      (max_adv_inst));

        btm_multi_adv_cb.op_q.p_inst_id = osi_calloc(sizeof(UINT8) *
                                                     (max_adv_inst));

        btm_ble_ext_enable_cb.set_ids = osi_calloc(sizeof(UINT8) * max_adv_inst);

        btm_ble_ext_enable_cb.durations = osi_calloc(sizeof(UINT16) * max_adv_inst);

        btm_ble_ext_enable_cb.max_adv_events = osi_calloc(sizeof(UINT8) * max_adv_inst);
    }

    /* Initialize adv instance indices and IDs. */
    for (i = 0; i < max_adv_inst; i++) {
        btm_multi_adv_cb.p_adv_inst[i].index = i;
        btm_multi_adv_cb.p_adv_inst[i].inst_id = i + 1;
        btm_multi_adv_cb.p_adv_inst[i].adv_raddr_timer =
            alarm_new("btm_ble.adv_raddr_timer");
    }
    controller_get_interface()->set_ble_adv_ext_size(max_adv_inst);

#if (defined BLE_EXTENDED_ADV_SUPPORT && (BLE_EXTENDED_ADV_SUPPORT == TRUE))
    if(!controller_get_interface()->supports_ble_extended_advertisements())
#endif
    {
        BTM_RegisterForVSEvents(btm_ble_multi_adv_vse_cback, TRUE);
    }
}

/*******************************************************************************
**
** Function         btm_ble_multi_adv_cleanup
**
** Description      This function cleans up multi adv control block.
**
** Parameters
** Returns          void
**
*******************************************************************************/
void btm_ble_multi_adv_cleanup(void)
{
#ifdef WIPOWER_SUPPORTED
    is_wipower_adv = false;
    wipower_inst_id = BTM_BLE_MULTI_ADV_DEFAULT_STD;
#endif

    pthread_mutex_lock(&btm_multi_adv_lock);
    if (btm_multi_adv_cb.p_adv_inst) {
        for (size_t i = 0; i < btm_cb.cmn_ble_vsc_cb.adv_inst_max; i++) {
            alarm_free(btm_multi_adv_cb.p_adv_inst[i].adv_raddr_timer);
        }
        osi_free_and_reset((void **)&btm_multi_adv_cb.p_adv_inst);
    }

    osi_free_and_reset((void **)&btm_multi_adv_cb.op_q.p_sub_code);
    osi_free_and_reset((void **)&btm_multi_adv_cb.op_q.p_inst_id);
    osi_free_and_reset((void **)&btm_multi_adv_cb.op_q.p_inst_id);
    osi_free_and_reset((void **)&btm_ble_ext_enable_cb.set_ids);
    osi_free_and_reset((void **)&btm_ble_ext_enable_cb.durations);
    osi_free_and_reset((void **)&btm_ble_ext_enable_cb.max_adv_events);
    pthread_mutex_unlock(&btm_multi_adv_lock);
}

/*******************************************************************************
**
** Function         btm_ble_multi_adv_get_ref
**
** Description      This function obtains the reference pointer for the instance ID provided
**
** Parameters       inst_id - Instance ID
**
** Returns          void*
**
*******************************************************************************/
void* btm_ble_multi_adv_get_ref(UINT8 inst_id)
{
    tBTM_BLE_MULTI_ADV_INST *p_inst = NULL;

    if (inst_id < BTM_BleMaxMultiAdvInstanceCount())
    {
        p_inst = &btm_multi_adv_cb.p_adv_inst[inst_id - 1];
        if (NULL != p_inst)
            return p_inst->p_ref;
    }

    return NULL;
}

#if (defined BLE_EXTENDED_ADV_SUPPORT && (BLE_EXTENDED_ADV_SUPPORT == TRUE))

/*******************************************************************************
**
** Function         btm_ble_enable_extended_adv
**
** Description      This function enables the extended adv
**
** Parameters       enable: enable or disable
**                  inst_id:  adv instance ID, can not be 0
**                  duration: duration of the adv
**
** Returns          status
**
*******************************************************************************/
tBTM_STATUS btm_ble_enable_extended_adv (BOOLEAN enable, UINT8 inst_id, UINT16 duration, UINT8 max_ext_adv_evts, UINT8 cb_evt)
{
    UINT8           enb = enable ? 1: 0;
    tBTM_STATUS     rt;

    BTM_TRACE_EVENT ("%s: enb %d, Inst ID %d, dur = %d(s)",__func__, enb,inst_id, duration);
    inst_id = inst_id - 1;
    duration = duration * 100; //duration is interpreted as t*10msec

    if ((rt = btsnd_hcic_ble_set_extended_adv_enable (enb,
                                    1, //Num of sets
                                    &inst_id,
                                    &duration,
                                    &max_ext_adv_evts))
                                    == BTM_CMD_STARTED)
    {
        btm_ble_multi_adv_enq_op_q(BTM_BLE_MULTI_ADV_ENB, inst_id + 1, cb_evt);
    }
    return rt;
}

/*******************************************************************************
**
** Function         btm_ble_save_extended_adv_params
**
** Description      This function sets the extended adv params
**
** Parameters       p_inst: pointer to instance variable
**                  p_params: adv parameters
**                  cb_evt: callback event
**
** Returns          status
**
*******************************************************************************/
void btm_ble_save_extended_adv_params (tBTM_BLE_MULTI_ADV_INST *p_inst,UINT32 adv_int_min, UINT32 adv_int_max,
        UINT8 own_addr_type, UINT8 pri_phy, UINT8 adv_sid ,UINT8 sec_adv_max_skip, UINT8 sec_adv_phy,
        UINT8 scan_req_notf_enb, UINT8 channel_map, UINT8 adv_filter_policy, UINT8 tx_power)
{
    if(p_inst != NULL)
    {
        p_inst->adv_int_min = adv_int_min;
        p_inst->adv_int_max = adv_int_max;
        p_inst->own_addr_type = own_addr_type;
        p_inst->pri_phy = pri_phy;
        p_inst->adv_sid = adv_sid;
        p_inst->sec_adv_max_skip = sec_adv_max_skip;
        p_inst->sec_adv_phy = sec_adv_phy;
        p_inst->scan_req_notf_enb = scan_req_notf_enb;
        p_inst->channel_map = channel_map;
        p_inst->adv_filter_policy = adv_filter_policy;
        p_inst->tx_power = tx_power;
    }
}


/*******************************************************************************
**
** Function         btm_ble_extended_adv_set_params
**
** Description      This function sets the extended adv params
**
** Parameters       p_inst: pointer to instance variable
**                  p_params: adv parameters
**                  cb_evt: callback event
**
** Returns          status
**
*******************************************************************************/
tBTM_STATUS btm_ble_extended_adv_set_params (tBTM_BLE_MULTI_ADV_INST *p_inst,
                                             tBTM_BLE_ADV_PARAMS *p_params,
                                             UINT8 cb_evt)
{
    UINT8             set_id;
    tBTM_BLE_EXT_EVT  evt_prop = BTM_BLE_EXT_CONNECT_EVT;
    tBTM_STATUS       rt;
    tBLE_ADDR_TYPE    own_addr_type;
    tBLE_ADDR_TYPE    dir_addr_type = BLE_ADDR_RANDOM;
    UINT32            adv_int_min = 0, adv_int_max =0;
    UINT8             adv_filter_policy = 0, tx_power =0, channel_map = 0;
    UINT8             pri_phy = 0,sec_adv_max_skip=0, sec_adv_phy=0, adv_sid=1, scan_req_notf_enb=0;

    if (!p_inst)
    {
        BTM_TRACE_ERROR ("%s: instance variable is null", __func__);
        return BTM_ERR_PROCESSING;
    }

    if (0 == BTM_BleMaxMultiAdvInstanceCount())
    {
        BTM_TRACE_ERROR ("%s: Controller does not support extended Multi ADV", __func__);
        return BTM_ERR_PROCESSING;
    }

    set_id = p_inst->inst_id;
#if (defined BLE_PRIVACY_SPT && BLE_PRIVACY_SPT == TRUE)
        if (btm_cb.ble_ctr_cb.privacy_mode != BTM_PRIVACY_NONE)
        {
            own_addr_type = BLE_ADDR_RANDOM_ID;
        }
        else
#endif
        {
            own_addr_type = BLE_ADDR_PUBLIC;
        }

    if(p_params != NULL)
    {
        p_inst->duration = p_params->duration;
        p_inst->adv_evt = p_params->adv_type;

        adv_int_min = p_params->adv_int_min;
        adv_int_max = p_params->adv_int_max;
        //To identify that event type(adv type) parameter was read from bt_stack.conf and not sent from fwks
        if(p_params->sec_adv_phy > 0)
        {
            evt_prop = p_params->adv_type;
            pri_phy = p_params->pri_phy;
            adv_sid = p_params->adv_sid;
            scan_req_notf_enb =p_params->scan_req_notf_enb;
            sec_adv_phy = p_params->sec_adv_phy;
            sec_adv_max_skip = p_params->sec_adv_max_skip;
        }
        else
        {
            switch (p_params->adv_type)
            {
                case BTM_BLE_CONNECT_EVT:
                    evt_prop = BTM_BLE_EXT_CONNECT_EVT;
                    break;
                case BTM_BLE_CONNECT_DIR_EVT:
                    evt_prop = BTM_BLE_EXT_CONNECT_DIR_EVT;
                    break;
                case BTM_BLE_DISCOVER_EVT:
                    evt_prop = BTM_BLE_EXT_DISCOVER_EVT;
                    break;
                case BTM_BLE_NON_CONNECT_EVT:
                    evt_prop = BTM_BLE_EXT_NON_CONNECT_EVT;
                    break;
                case BTM_BLE_CONNECT_LO_DUTY_DIR_EVT:
                    evt_prop = BTM_BLE_EXT_CONNECT_LO_DUTY_DIR_EVT;
                    break;
            }
            pri_phy = BTM_DATA_RATE_ONE;
            adv_sid = 0x01;

        }
        p_inst->evt_prop = evt_prop;

        if (p_params->channel_map == 0 || p_params->channel_map > BTM_BLE_DEFAULT_ADV_CHNL_MAP)
            channel_map = BTM_BLE_DEFAULT_ADV_CHNL_MAP;
        else
            channel_map = p_params->channel_map;

        if (p_params->adv_filter_policy >= AP_SCAN_CONN_POLICY_MAX)
            adv_filter_policy = AP_SCAN_CONN_ALL;
        else
            adv_filter_policy = p_params->adv_filter_policy;

        if (p_params->tx_power > BTM_BLE_ADV_TX_POWER_MAX)
            tx_power = BTM_BLE_ADV_TX_POWER_MAX;
        else
            tx_power = p_params->tx_power;

        //save ext adv params in p_inst for reenabling adv for chained ext advs
        btm_ble_save_extended_adv_params(p_inst, adv_int_min, adv_int_max, own_addr_type, pri_phy, adv_sid ,sec_adv_max_skip,
                                         sec_adv_phy, scan_req_notf_enb, channel_map, adv_filter_policy,tx_power);
    }
    //else case is reached when adv is disabled and re enabled again for chained advs
    else
    {
        evt_prop = p_inst->evt_prop;
        adv_int_min = p_inst->adv_int_min;
        adv_int_max =p_inst->adv_int_max;
        channel_map =p_inst->channel_map;
        adv_filter_policy =p_inst->adv_filter_policy;
        tx_power=p_inst->tx_power;
        pri_phy=p_inst->pri_phy;
        sec_adv_max_skip=p_inst->sec_adv_max_skip;
        sec_adv_phy =p_inst->sec_adv_phy;
        adv_sid =p_inst->adv_sid;
        scan_req_notf_enb=p_inst->scan_req_notf_enb;
    }

    BTM_TRACE_ERROR ("%s: evt_prop::%d, primary_phy=%d,p_params->sec_adv_max_skip=%d, p_params->sec_adv_phy=%d, p_params->adv_sid=%d, p_params->scan_req_notf_enb=%d", __func__,
                    evt_prop, pri_phy, sec_adv_max_skip, sec_adv_phy, adv_sid, scan_req_notf_enb);

    rt = btsnd_hcic_ble_set_extended_adv_params (set_id - 1, evt_prop,
                                                 adv_int_min, adv_int_max,
                                                 channel_map, own_addr_type,
                                                 dir_addr_type, p_inst->rpa,
                                                 adv_filter_policy, btm_ble_map_adv_tx_power(tx_power),
                                                 pri_phy, sec_adv_max_skip,
                                                 sec_adv_phy, adv_sid, scan_req_notf_enb);

    if (rt == BTM_CMD_STARTED)
    {
        btm_ble_multi_adv_enq_op_q(BTM_BLE_MULTI_ADV_SET_PARAM, set_id, cb_evt);
    }

    return rt;
}

/*******************************************************************************
**
** Function         btm_ble_send_ext_adv_data
**
** Description      This function configure a Multi-ADV instance with the specified
**                  adv data or scan response data.
**
** Parameters       inst_id: adv instance ID
**                  is_scan_rsp: is this scan response. if no, set as adv data.
**                  data_mask: adv data mask.
**                  p_data: pointer to the adv data structure.
**                  operation:
**                  0x00: Intermediate fragment
**                  0x01: first fragment
**                  0x02: Last fragment
**                  0x03: complete data, ctrlr fragmentation permitted
**                  0x04: complete data, ctrlr fragmentation not permitted
**
** Returns          status
**
*******************************************************************************/
tBTM_STATUS btm_ble_send_ext_adv_data (BOOLEAN is_scan_rsp, UINT8 inst_id, UINT8 operation, UINT8 frag_pref, UINT8 data_len, UINT8 *param)
{
    tBTM_STATUS rt;
    UINT8 sub_code;
    if (!is_scan_rsp)
    {
        rt = btsnd_hcic_ble_set_extended_adv_data(inst_id - 1,
                                    operation,
                                    frag_pref,
                                    data_len,
                                    param);
        sub_code = BTM_BLE_MULTI_ADV_WRITE_ADV_DATA;
    }
    else
    {
        rt = btsnd_hcic_ble_set_extended_scan_rsp_data(inst_id - 1,
                                    operation,
                                    frag_pref,
                                    data_len,
                                    param);
        sub_code = BTM_BLE_MULTI_ADV_WRITE_SCAN_RSP_DATA;
    }

    if (rt == BTM_CMD_STARTED)
    {
        btm_ble_multi_adv_enq_op_q(sub_code, inst_id, BTM_BLE_MULTI_ADV_DATA_EVT);
    }
    return rt;
}


/*******************************************************************************
**
** Function         btm_ble_compute_frag_lens
**
** Description      This function configure a Multi-ADV instance with the specified
**                  adv data or scan response data.
**
** Parameters       inst_id: adv instance ID
**                  is_scan_rsp: is this scan response. if no, set as adv data.
**                  data_mask: adv data mask.
**                  p_data: pointer to the adv data structure.
**                  operation:
**                  0x00: Intermediate fragment
**                  0x01: first fragment
**                  0x02: Last fragment
**                  0x03: complete data, ctrlr fragmentation permitted
**                  0x04: complete data, ctrlr fragmentation not permitted
**
** Returns          status
**
*******************************************************************************/
void btm_ble_compute_frag_lens (UINT16 data_len, UINT8* num_hci_cmds, UINT8 *pp_data_len)
{
    UINT8 index = 0;
    *num_hci_cmds = (data_len/(HCI_COMMAND_SIZE-4));
    for(index=0; index < *num_hci_cmds; index++)
        pp_data_len[index] = (HCI_COMMAND_SIZE -4);

    if(data_len % (HCI_COMMAND_SIZE-4))
    {
        pp_data_len[index] = (data_len % (HCI_COMMAND_SIZE-4));
        (*num_hci_cmds)++;
    }
    BTM_TRACE_EVENT("btm_ble_compute_frag_lens::Final num_hci_cmds=%d",*num_hci_cmds);
}

/*******************************************************************************
**
** Function         btm_ble_get_operation
**
** Description      This function configure a Multi-ADV instance with the specified
**                  adv data or scan response data.
**
** Parameters       inst_id: adv instance ID
**                  is_scan_rsp: is this scan response. if no, set as adv data.
**                  data_mask: adv data mask.
**                  p_data: pointer to the adv data structure.
**                  operation:
**                  0x00: Intermediate fragment
**                  0x01: first fragment
**                  0x02: Last fragment
**                  0x03: complete data, ctrlr fragmentation permitted
**                  0x04: complete data, ctrlr fragmentation not permitted
**
** Returns          status
**
*******************************************************************************/
UINT8 btm_ble_get_operation (UINT8 num_hci_cmds_copy, UINT8 num_hci_cmds)
{
    UINT8 op = 0;
    if(num_hci_cmds_copy == num_hci_cmds)
        op = BTM_BLE_EXT_ADV_FIRST_FRAG;
    else if(num_hci_cmds != 1)
        op = BTM_BLE_EXT_ADV_INT_FRAG;
    else
        op = BTM_BLE_EXT_ADV_LAST_FRAG;
    return op;
}


/*******************************************************************************
**
** Function         BTM_BleWriteExtendedAdvData
**
** Description      This function configure a Multi-ADV instance with the specified
**                  adv data or scan response data.
**
** Parameters       inst_id: adv instance ID
**                  is_scan_rsp: is this scan response. if no, set as adv data.
**                  data_mask: adv data mask.
**                  p_data: pointer to the adv data structure.
**                  operation:
**                  0x00: Intermediate fragment
**                  0x01: first fragment
**                  0x02: Last fragment
**                  0x03: complete data, ctrlr fragmentation permitted
**                  0x04: complete data, ctrlr fragmentation not permitted
**
** Returns          status
**
*******************************************************************************/
tBTM_STATUS BTM_BleWriteExtendedAdvData (UINT8 inst_id, BOOLEAN is_scan_rsp,
                                    tBTM_BLE_AD_MASK data_mask,
                                    UINT8 operation, UINT8 frag_pref,
                                    tBTM_BLE_ADV_DATA *p_data)
{
    UINT8       param[BTM_BLE_EXTENDED_AD_DATA_LEN], *pp = param, frag_data[HCI_COMMAND_SIZE-4];
    UINT8       frag_data_len[BTM_BLE_EXT_ADV_MAX_FRAG_NUM], index=0;
    UINT16      data_len, offset =0, j=0;
    tBTM_STATUS rt;
    UINT8       num_hci_cmds = 0, num_hci_cmds_copy = 0;
    tBTM_BLE_MULTI_ADV_INST *p_inst;
    tBTM_BLE_LOCAL_ADV_DATA *p_adv_data = &btm_cb.ble_ctr_cb.inq_var.adv_data;

    if (0 == BTM_BleMaxMultiAdvInstanceCount())
    {
        BTM_TRACE_ERROR ("%s: Controller does not support extended Multi ADV", __func__);
        return BTM_ERR_PROCESSING;
    }
    BTM_TRACE_EVENT("BTM_BleWriteExtendedAdvData");
    if (stack_config_get_interface()->get_pts_le_nonconn_adv_enabled())
    {
        if (p_adv_data->p_flags != NULL)
        {
            p_data->flag = *(p_adv_data->p_flags);
        }
    }

    btm_ble_update_dmt_flag_bits(&p_data->flag, btm_cb.btm_inq_vars.connectable_mode,
                                        btm_cb.btm_inq_vars.discoverable_mode);

    BTM_TRACE_EVENT("%s called with inst_id:%d", __func__, inst_id);
    // inst_id will range from 1 to 16 max, so the below should ideally fail
    if (inst_id > BTM_BLE_MULTI_ADV_MAX || inst_id == BTM_BLE_MULTI_ADV_DEFAULT_STD)
        return BTM_ILLEGAL_VALUE;

    p_inst = &btm_multi_adv_cb.p_adv_inst[inst_id - 1];

    /* check if adv is not enabled for operations 0 to 3 */
    if (p_inst->in_use && (operation < BTM_BLE_EXT_ADV_COMPLETE))
    {
        BTM_TRACE_ERROR ("%s: Illegal operation: %d while adv is enabled", __func__, operation);
        return BTM_ILLEGAL_VALUE;
    }

    memset(param, 0, BTM_BLE_EXTENDED_AD_DATA_LEN);

    if (p_inst->len == 0 || (p_inst->evt_prop & BTM_BLE_EXTENDED_LEGACY_MASK)) {
        BTM_TRACE_ERROR("%s: using default length 31 bytes", __func__);
        p_inst->len = BTM_BLE_AD_DATA_LEN;
    }

    btm_ble_build_adv_data(&data_mask, &pp, p_data, p_inst->len);
    data_len = (UINT16) (pp - param);
    BTM_TRACE_ERROR("%s: data len is %d", __func__, data_len);

    if(data_len > HCI_COMMAND_SIZE)
        btm_ble_compute_frag_lens(data_len, &num_hci_cmds, frag_data_len);

    num_hci_cmds_copy = num_hci_cmds;

    if(num_hci_cmds > 0)
    {
        //Disable advertisement
        rt = btm_ble_enable_extended_adv (FALSE, inst_id,
                                         p_inst->duration, 0/*p_params->max_ext_adv_evts*/,
                                         0/*instead of BTM_BLE_MULTI_ADV_DISABLE_EVT*/);
        p_inst->in_use = FALSE;

        //set ext adv params
        if (rt == BTM_CMD_STARTED)
        {
            p_inst->in_use = TRUE;
            rt = btm_ble_extended_adv_set_params(p_inst, NULL, 0);
        }

        //set ext adv data
        if (rt == BTM_CMD_STARTED)
        {
            while(num_hci_cmds)
            {
                memset(frag_data, 0, frag_data_len[index]);
                BTM_TRACE_ERROR("offset =%d , index =%d ,frag_data_len[index] ::%d",offset, index, frag_data_len[index]);
                for(j=0; j< frag_data_len[index]; j++)
                {
                    BTM_TRACE_ERROR("Adv data ::%02x",param[offset+j]);
                }
                memcpy(frag_data, (param+offset), frag_data_len[index]);
                operation = btm_ble_get_operation(num_hci_cmds_copy, num_hci_cmds);
                offset += frag_data_len[index];
                rt = btm_ble_send_ext_adv_data(is_scan_rsp, inst_id, operation, frag_pref, frag_data_len[index++], frag_data);
                num_hci_cmds--;
            }
        }
        //enable ext adv
        if (rt == BTM_CMD_STARTED)
        {
            rt = btm_ble_enable_extended_adv (TRUE, p_inst->inst_id, p_inst->duration, 0, 0/*instead of BTM_BLE_EXTENDED_ADV_ENB_EVT*/);
        }
    }
    else
        rt = btm_ble_send_ext_adv_data(is_scan_rsp, inst_id, operation, frag_pref, data_len, param);

    return rt;
}

/*******************************************************************************
**
** Function         btm_ble_ext_adv_reenable
**
** Description      This function re-enable adv instance upon a connection establishment.
**
** Parameters       instance id
**
** Returns          none.
**
*******************************************************************************/
void btm_ble_ext_adv_reenable(UINT8 inst_id)
{
    BTM_TRACE_DEBUG("%s, instance id: %d", __func__, inst_id);
    tBTM_BLE_MULTI_ADV_INST *p_inst = &btm_multi_adv_cb.p_adv_inst[inst_id - 1];
    //TODO: Use expired ticks instead. GKI_get_os_tick_count
    // provides time in msec. Diff between current and start time
    // to figure out remaining duration
    UINT16 duration = p_inst->duration;

    if (TRUE == p_inst->in_use)
    {
        // Verify the evt prop directed adv bit is 0
        if (!(p_inst->evt_prop & 0x04))
            btm_ble_enable_extended_adv (TRUE, p_inst->inst_id, duration, 0, 0);
        else
        {
            //mark directed adv as disabled if adv has been stopped
            (p_inst->p_cback)(BTM_BLE_MULTI_ADV_DISABLE_EVT,p_inst->inst_id,p_inst->p_ref,0);
             p_inst->in_use = FALSE;
        }
     }
}

/*******************************************************************************
**
** Function         btm_ble_multi_adv_enable_all
**
** Description      This function enables/disables all adv instances at once
**
** Parameters       enable/disable
**
** Returns          none.
**
*******************************************************************************/
void btm_ble_multi_adv_enable_all(UINT8 enable)
{
    BTM_TRACE_DEBUG("%s, enable = %d", __func__, enable);
    UINT8 i;
    tBTM_BLE_MULTI_ADV_INST *p_inst = &btm_multi_adv_cb.p_adv_inst[0];
    UINT8 enb = enable? 1:0;
    UINT8 num_instances = 0;

    for (i = 0; i <  BTM_BleMaxMultiAdvInstanceCount() - 1; i++, p_inst++)
    {
        if (p_inst->in_use) {
            btm_ble_ext_enable_cb.set_ids[num_instances] = p_inst->inst_id - 1;
            btm_ble_ext_enable_cb.durations[num_instances] = p_inst->duration * 100;
            btm_ble_ext_enable_cb.max_adv_events[num_instances] = 0x00;
            num_instances++;
        }
    }

    if (num_instances == 0) return;

    if (!enable)
    {
       if (btsnd_hcic_ble_set_extended_adv_enable(enb, 0, NULL, NULL, NULL)
                                                   == BTM_CMD_STARTED)
       {
           btm_ble_multi_adv_enq_op_q(BTM_BLE_MULTI_ADV_ENB, 1, 0);
       }
    }
    else
    {
        if ((btsnd_hcic_ble_set_extended_adv_enable (enb,
                                    num_instances,
                                    btm_ble_ext_enable_cb.set_ids,
                                    btm_ble_ext_enable_cb.durations,
                                    btm_ble_ext_enable_cb.max_adv_events))
                                    == BTM_CMD_STARTED)
        {
            btm_ble_multi_adv_enq_op_q(BTM_BLE_MULTI_ADV_ENB, 1, 0);
        }
    }
}

/*******************************************************************************
**
** Function         btm_ble_adv_set_terminated_evt
**
** Description      This function is a callback event when an adv set is
**                  terminiated due to connection complete or due to duration
**                  timeout
**
** Returns          void
**
*******************************************************************************/

void btm_ble_adv_set_terminated_evt (UINT8* p)
{
    BTM_TRACE_EVENT("%s", __func__);
    UINT8 status;
    UINT8 inst_id;
    UINT16 handle;
    tBTM_BLE_MULTI_ADV_INST *p_inst;

    STREAM_TO_UINT8(status,p);
    STREAM_TO_UINT8(inst_id, p);

    //Adjust inst_id to base 1
    inst_id++;

    STREAM_TO_UINT16(handle, p);
    BTM_TRACE_DEBUG("%s, status = %d, inst_id = %d, handle = %x",
                     __func__, status, inst_id, handle);

    if (inst_id > BTM_BleMaxMultiAdvInstanceCount() ||
        inst_id == BTM_BLE_MULTI_ADV_DEFAULT_STD)
    {
        BTM_TRACE_ERROR("%s:Invalid instance received", __func__);
        return;
    }

    p_inst = &btm_multi_adv_cb.p_adv_inst[inst_id - 1];

    if (status == BTM_BLE_EXTENDED_ADV_TIMEOUT)
    {
        p_inst->in_use = FALSE;
        if (p_inst->p_cback)
            (p_inst->p_cback)(BTM_BLE_MULTI_ADV_DISABLE_EVT,p_inst->inst_id, p_inst->p_ref, 0);
    }

    else if (status == HCI_SUCCESS)
    {
        btm_ble_ext_adv_reenable(inst_id);
    }
}


/*******************************************************************************
**
** Function         btm_ble_adv_extension_operation_complete
**
** Description      This function is a callback event of adv extension operation
**                  including write rpa, set adv data/scan rsp data
**
** Returns          void
**
*******************************************************************************/

void btm_ble_adv_extension_operation_complete(UINT8* p, UINT16 hcicmd)
{
    UINT8 status;
    UINT8 cb_evt = 0, opcode;
    UINT8 subcode = 0;
    UINT8 inst_id;
    tBTM_BLE_MULTI_ADV_INST *p_inst ;
    STREAM_TO_UINT8 (status, p);

    BTM_TRACE_EVENT ("%s, status: %d, opcode = %d", __func__, status, hcicmd);

    if (status != HCI_SUCCESS)
    {
        BTM_TRACE_ERROR ("%s, HCI command failure", __func__);
        return;
    }

    btm_ble_multi_adv_deq_op_q(&opcode, &inst_id, &cb_evt);

    switch (hcicmd)
    {
        case HCI_BLE_WRITE_EXTENDED_ADV_RPA:
            subcode = BTM_BLE_MULTI_ADV_SET_RANDOM_ADDR;
            break;
        case HCI_BLE_WRITE_EXTENDED_ADV_DATA:
            subcode = BTM_BLE_MULTI_ADV_WRITE_ADV_DATA;
            break;
        case HCI_BLE_WRITE_EXTENDED_SCAN_RSP_DATA:
            subcode = BTM_BLE_MULTI_ADV_WRITE_SCAN_RSP_DATA;
            break;
        case HCI_BLE_WRITE_EXTENDED_ADV_PARAMS:
            subcode = BTM_BLE_MULTI_ADV_SET_PARAM;
            break;
        case HCI_BLE_WRITE_EXTENDED_ADV_ENABLE:
            subcode = BTM_BLE_MULTI_ADV_ENB;
            break;
    }

    if (opcode != subcode)
    {
        BTM_TRACE_ERROR("got unexpected Event, expected: %d got: %d", opcode, subcode);
        return;
    }
    p_inst = &btm_multi_adv_cb.p_adv_inst[inst_id - 1];

    if (cb_evt != 0 && p_inst->p_cback != NULL)
    {
        (p_inst->p_cback)(cb_evt, inst_id, p_inst->p_ref, status);
    }
    return;
}

UINT8 BTM_BleGetAvailableMAInstance ()
{
    UINT8 index = 0;
    tBTM_BLE_MULTI_ADV_INST *p_inst = &btm_multi_adv_cb.p_adv_inst[0];

    BTM_TRACE_EVENT("BTM_BleGetAvailableMAInstance");

    for (index = 0; index <  BTM_BleMaxMultiAdvInstanceCount() - 1; index++, p_inst++)
    {
        if (FALSE == p_inst->in_use)
            break;
    }
    return index;
}

#endif
#endif

