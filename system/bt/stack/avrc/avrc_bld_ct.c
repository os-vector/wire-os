/******************************************************************************
 *
 *  Copyright (C) 2006-2013 Broadcom Corporation
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

#include "bt_common.h"
#include "avrc_api.h"
#include "avrc_defs.h"
#include "avrc_int.h"

/*****************************************************************************
**  Global data
*****************************************************************************/


#if (AVRC_METADATA_INCLUDED == TRUE)
/*******************************************************************************
**
** Function         avrc_bld_next_cmd
**
** Description      This function builds the Request Continue or Abort command.
**
** Returns          AVRC_STS_NO_ERROR, if the command is built successfully
**                  Otherwise, the error code.
**
*******************************************************************************/
static tAVRC_STS avrc_bld_next_cmd (tAVRC_NEXT_CMD *p_cmd, BT_HDR *p_pkt)
{
    UINT8   *p_data, *p_start;

    AVRC_TRACE_API("avrc_bld_next_cmd");

    /* get the existing length, if any, and also the num attributes */
    p_start = (UINT8 *)(p_pkt + 1) + p_pkt->offset;
    p_data = p_start + 2; /* pdu + rsvd */

    /* add fixed lenth 1 - pdu_id (1) */
    UINT16_TO_BE_STREAM(p_data, 1);
    UINT8_TO_BE_STREAM(p_data, p_cmd->target_pdu);
    p_pkt->len = (p_data - p_start);

    return AVRC_STS_NO_ERROR;
}

/*****************************************************************************
**  the following commands are introduced in AVRCP 1.4
*****************************************************************************/

#if (AVRC_ADV_CTRL_INCLUDED == TRUE)
/*******************************************************************************
**
** Function         avrc_bld_set_abs_volume_cmd
**
** Description      This function builds the Set Absolute Volume command.
**
** Returns          AVRC_STS_NO_ERROR, if the command is built successfully
**                  Otherwise, the error code.
**
*******************************************************************************/
static tAVRC_STS avrc_bld_set_abs_volume_cmd (tAVRC_SET_VOLUME_CMD *p_cmd, BT_HDR *p_pkt)
{
    UINT8   *p_data, *p_start;

    AVRC_TRACE_API("avrc_bld_set_abs_volume_cmd");
    /* get the existing length, if any, and also the num attributes */
    p_start = (UINT8 *)(p_pkt + 1) + p_pkt->offset;
    p_data = p_start + 2; /* pdu + rsvd */
    /* add fixed lenth 1 - volume (1) */
    UINT16_TO_BE_STREAM(p_data, 1);
    UINT8_TO_BE_STREAM(p_data, (AVRC_MAX_VOLUME & p_cmd->volume));
    p_pkt->len = (p_data - p_start);
    return AVRC_STS_NO_ERROR;
}

/*******************************************************************************
**
** Function         avrc_bld_register_notifn
**
** Description      This function builds the register notification.
**
** Returns          AVRC_STS_NO_ERROR, if the command is built successfully
**                  Otherwise, the error code.
**
*******************************************************************************/
static tAVRC_STS avrc_bld_register_notifn(BT_HDR * p_pkt, UINT8 event_id, UINT32 event_param)
{
    UINT8   *p_data, *p_start;

    AVRC_TRACE_API("avrc_bld_register_notifn");
    /* get the existing length, if any, and also the num attributes */
    // Set the notify value
    p_start = (UINT8 *)(p_pkt + 1) + p_pkt->offset;
    p_data = p_start + 2; /* pdu + rsvd */
    /* add fixed length 5 -*/
    UINT16_TO_BE_STREAM(p_data, 5);
    UINT8_TO_BE_STREAM(p_data,event_id);
    UINT32_TO_BE_STREAM(p_data, event_param);
    p_pkt->len = (p_data - p_start);
    return AVRC_STS_NO_ERROR;
}
#endif
#if (AVRC_CTLR_INCLUDED == TRUE)
/*******************************************************************************
**
** Function         avrc_bld_get_capability_cmd
**
** Description      This function builds the get capability command.
**
** Returns          AVRC_STS_NO_ERROR, if the command is built successfully
**                  Otherwise, the error code.
**
*******************************************************************************/
static tAVRC_STS avrc_bld_get_capability_cmd(BT_HDR * p_pkt, UINT8 cap_id)
{
    AVRC_TRACE_API("avrc_bld_get_capability_cmd");
    UINT8 *p_start = (UINT8 *)(p_pkt + 1) + p_pkt->offset;
    UINT8 *p_data = p_start + 2; /* pdu + rsvd */
    /* add fixed length 1 -*/
    UINT16_TO_BE_STREAM(p_data, 1);
    UINT8_TO_BE_STREAM(p_data,cap_id);
    p_pkt->len = (p_data - p_start);
    return AVRC_STS_NO_ERROR;
}

/*******************************************************************************
**
** Function         avrc_bld_list_player_app_attr_cmd
**
** Description      This function builds the list player app attrib command.
**
** Returns          AVRC_STS_NO_ERROR, if the command is built successfully
**                  Otherwise, the error code.
**
*******************************************************************************/
static tAVRC_STS avrc_bld_list_player_app_attr_cmd(BT_HDR * p_pkt)
{
    AVRC_TRACE_API("avrc_bld_list_player_app_attr_cmd");
    UINT8 *p_start = (UINT8 *)(p_pkt + 1) + p_pkt->offset;
    UINT8 *p_data = p_start + 2; /* pdu + rsvd */
    /* add fixed length 1 -*/
    UINT16_TO_BE_STREAM(p_data, 0);
    p_pkt->len = (p_data - p_start);
    return AVRC_STS_NO_ERROR;
}

/*******************************************************************************
**
** Function         avrc_bld_list_player_app_values_cmd
**
** Description      This function builds the list player app values command.
**
** Returns          AVRC_STS_NO_ERROR, if the command is built successfully
**                  Otherwise, the error code.
**
*******************************************************************************/
static tAVRC_STS avrc_bld_list_player_app_values_cmd(BT_HDR * p_pkt, UINT8 attrib_id)
{
    AVRC_TRACE_API("avrc_bld_list_player_app_values_cmd");
    UINT8 *p_start = (UINT8 *)(p_pkt + 1) + p_pkt->offset;
    UINT8 *p_data = p_start + 2; /* pdu + rsvd */
    /* add fixed length 1 -*/
    UINT16_TO_BE_STREAM(p_data, 1);
    UINT8_TO_BE_STREAM(p_data,attrib_id);
    p_pkt->len = (p_data - p_start);
    return AVRC_STS_NO_ERROR;
}

/*******************************************************************************
**
** Function         avrc_bld_get_current_player_app_values_cmd
**
** Description      This function builds the get current player app setting values command.
**
** Returns          AVRC_STS_NO_ERROR, if the command is built successfully
**                  Otherwise, the error code.
**
*******************************************************************************/
static tAVRC_STS avrc_bld_get_current_player_app_values_cmd(
    BT_HDR * p_pkt, UINT8 num_attrib_id, UINT8* attrib_ids)
{
    AVRC_TRACE_API("avrc_bld_get_current_player_app_values_cmd");
    UINT8 *p_start = (UINT8 *)(p_pkt + 1) + p_pkt->offset;
    UINT8 *p_data = p_start + 2; /* pdu + rsvd */
    UINT8 param_len = num_attrib_id + 1; // 1 additional to hold num attributes feild
    /* add length -*/
    UINT16_TO_BE_STREAM(p_data, param_len);
    UINT8_TO_BE_STREAM(p_data,num_attrib_id);
    for(int count = 0; count < num_attrib_id && count < AVRC_MAX_APP_ATTR_SIZE; count ++)
    {
        UINT8_TO_BE_STREAM(p_data,attrib_ids[count]);
    }
    p_pkt->len = (p_data - p_start);
    return AVRC_STS_NO_ERROR;
}

/*******************************************************************************
**
** Function         avrc_bld_set_current_player_app_values_cmd
**
** Description      This function builds the set current player app setting values command.
**
** Returns          AVRC_STS_NO_ERROR, if the command is built successfully
**                  Otherwise, the error code.
**
*******************************************************************************/
static tAVRC_STS avrc_bld_set_current_player_app_values_cmd(BT_HDR * p_pkt, UINT8 num_attrib_id, tAVRC_APP_SETTING* p_val)
{
    AVRC_TRACE_API("avrc_bld_set_current_player_app_values_cmd");
    UINT8 *p_start = (UINT8 *)(p_pkt + 1) + p_pkt->offset;
    UINT8 *p_data = p_start + 2; /* pdu + rsvd */
    /* we have to store attrib- value pair
     * 1 additional to store num elements
     */
    UINT8 param_len = (2*num_attrib_id) + 1;
    /* add length */
    UINT16_TO_BE_STREAM(p_data, param_len);
    UINT8_TO_BE_STREAM(p_data,num_attrib_id);
    for(int count = 0; count < num_attrib_id; count ++)
    {
        UINT8_TO_BE_STREAM(p_data,p_val[count].attr_id);
        UINT8_TO_BE_STREAM(p_data,p_val[count].attr_val);
    }
    p_pkt->len = (p_data - p_start);
    return AVRC_STS_NO_ERROR;
}

/*******************************************************************************
**
** Function         avrc_bld_get_player_app_setting_attr_text_cmd
**
** Description      This function builds the get player app setting attribute text command.
**
** Returns          AVRC_STS_NO_ERROR, if the command is built successfully
**                  Otherwise, the error code.
**
*******************************************************************************/
static tAVRC_STS avrc_bld_get_player_app_setting_attr_text_cmd (BT_HDR * p_pkt, tAVRC_GET_APP_ATTR_TXT_CMD *p_cmd)
{
    AVRC_TRACE_API("%s", __FUNCTION__);

    UINT8 *p_start = (UINT8 *)(p_pkt + 1) + p_pkt->offset;
    UINT8 *p_data = p_start + 2; /* pdu + rsvd */

    UINT8 param_len = p_cmd->num_attr + 1;
    /* add length */
    UINT16_TO_BE_STREAM(p_data, param_len);
    UINT8_TO_BE_STREAM(p_data, p_cmd->num_attr);
    for(int count = 0; count < p_cmd->num_attr; count++)
    {
        UINT8_TO_BE_STREAM(p_data, p_cmd->attrs[count]);
    }
    p_pkt->len = (p_data - p_start);
    return AVRC_STS_NO_ERROR;
}

/*******************************************************************************
**
** Function         avrc_bld_get_player_app_setting_value_text_cmd
**
** Description      This function builds the get player app setting value text command.
**
** Returns          AVRC_STS_NO_ERROR, if the command is built successfully
**                  Otherwise, the error code.
**
*******************************************************************************/
static tAVRC_STS avrc_bld_get_player_app_setting_value_text_cmd (BT_HDR * p_pkt, tAVRC_GET_APP_VAL_TXT_CMD *p_cmd)
{
    AVRC_TRACE_API("%s", __FUNCTION__);

    UINT8 *p_start = (UINT8 *)(p_pkt + 1) + p_pkt->offset;
    UINT8 *p_data = p_start + 2; /* pdu + rsvd */

    UINT8 param_len = p_cmd->num_val + 1;
    /* add length */
    UINT16_TO_BE_STREAM(p_data, param_len);
    UINT8_TO_BE_STREAM(p_data, p_cmd->num_val);
    for(int count = 0; count < p_cmd->num_val; count++)
    {
        UINT8_TO_BE_STREAM(p_data, p_cmd->vals[count]);
    }
    p_pkt->len = (p_data - p_start);
    return AVRC_STS_NO_ERROR;
}

/*******************************************************************************
**
** Function         avrc_bld_get_element_attr_cmd
**
** Description      This function builds the get element attribute command.
**
** Returns          AVRC_STS_NO_ERROR, if the command is built successfully
**                  Otherwise, the error code.
**
*******************************************************************************/
static tAVRC_STS avrc_bld_get_element_attr_cmd(BT_HDR * p_pkt, UINT8 num_attrib, UINT32* attrib_ids)
{
    AVRC_TRACE_API("avrc_bld_get_element_attr_cmd");
    UINT8 *p_start = (UINT8 *)(p_pkt + 1) + p_pkt->offset;
    UINT8 *p_data = p_start + 2; /* pdu + rsvd */
    /* we have to store attrib- value pair
     * 1 additional to store num elements
     */
    UINT8 param_len = (4*num_attrib) + 9;
    /* add length */
    UINT16_TO_BE_STREAM(p_data, param_len);
    /* 8 bytes of identifier as 0 (playing)*/
    UINT32_TO_BE_STREAM(p_data,0);
    UINT32_TO_BE_STREAM(p_data,0);
    UINT8_TO_BE_STREAM(p_data,num_attrib);
    for(int count = 0; count < num_attrib && count < AVRC_MAX_ELEM_ATTR_SIZE; count ++)
    {
        UINT32_TO_BE_STREAM(p_data,attrib_ids[count]);
    }
    p_pkt->len = (p_data - p_start);
    return AVRC_STS_NO_ERROR;
}

/*******************************************************************************
**
** Function         avrc_bld_get_play_status_cmd
**
** Description      This function builds the get play status command.
**
** Returns          AVRC_STS_NO_ERROR, if the command is built successfully
**                  Otherwise, the error code.
**
*******************************************************************************/
static tAVRC_STS avrc_bld_get_play_status_cmd(BT_HDR * p_pkt)
{
    AVRC_TRACE_API("avrc_bld_list_player_app_attr_cmd");
    UINT8 *p_start = (UINT8 *)(p_pkt + 1) + p_pkt->offset;
    UINT8 *p_data = p_start + 2; /* pdu + rsvd */
    /* add fixed length 1 -*/
    UINT16_TO_BE_STREAM(p_data, 0);
    p_pkt->len = (p_data - p_start);
    return AVRC_STS_NO_ERROR;
}

/*******************************************************************************
 *
 * Function         avrc_bld_set_addressed_player_cmd
 *
 * Description      This function builds the set addressed player cmd.
 *
 * Returns          AVRC_STS_NO_ERROR, if the command is built successfully
 *                  Otherwise, the error code.
 *
 ******************************************************************************/
static tAVRC_STS avrc_bld_set_addressed_player_cmd(
    BT_HDR* p_pkt, const tAVRC_SET_ADDR_PLAYER_CMD* cmd) {
  AVRC_TRACE_API("%s", __func__);
  /* get the existing length, if any, and also the num attributes */
  uint8_t* p_start = (uint8_t*)(p_pkt + 1) + p_pkt->offset;
  uint8_t* p_data = p_start + 2; /* pdu + rsvd */

  /* To change addressed player the following is the total length:
   * Player ID (2)
   */
  UINT16_TO_BE_STREAM(p_data, 2); /* fixed length */
  UINT16_TO_BE_STREAM(p_data, cmd->player_id);
  p_pkt->len = (p_data - p_start);
  return AVRC_STS_NO_ERROR;
}

/*******************************************************************************
 *
 * Function         avrc_bld_set_browsed_player_cmd
 *
 * Description      This function builds the set browsed player cmd.
 *
 * Returns          AVRC_STS_NO_ERROR, if the command is built successfully
 *                  Otherwise, the error code.
 *
 ******************************************************************************/
static tAVRC_STS avrc_bld_set_browsed_player_cmd(
    BT_HDR* p_pkt, const tAVRC_SET_BR_PLAYER_CMD* cmd) {
    AVRC_TRACE_API("%s", __func__);
    uint8_t* p_start = (uint8_t*)(p_pkt + 1) + p_pkt->offset;
    /* This is where the PDU specific for AVRC starts
     * AVRCP Spec 1.4 section 22.19 */
    uint8_t* p_data = p_start + 1; /* pdu */

    /* To change browsed player the following is the total length:
     * Player ID (2)
     */
    UINT16_TO_BE_STREAM(p_data, 2); /* fixed length */
    UINT16_TO_BE_STREAM(p_data, cmd->player_id);
    p_pkt->len = (p_data - p_start);
     AVRC_TRACE_API("%s p_pkt->len = %d", __func__, p_pkt->len);
    return AVRC_STS_NO_ERROR;

}

/*******************************************************************************
 *
 * Function         avrc_bld_get_folder_items_cmd
 *
 * Description      This function builds the get folder items cmd.
 *
 * Returns          AVRC_STS_NO_ERROR, if the command is built successfully
 *                  Otherwise, the error code.
 *
 ******************************************************************************/
static tAVRC_STS avrc_bld_get_folder_items_cmd(BT_HDR* p_pkt,
                                               const tAVRC_GET_ITEMS_CMD* cmd) {
    AVRC_TRACE_API(
      "avrc_bld_get_folder_items_cmd scope %d, start_item %d, end_item %d",
      cmd->scope, cmd->start_item, cmd->end_item);
    uint8_t* p_start = (uint8_t*)(p_pkt + 1) + p_pkt->offset;
    /* This is where the PDU specific for AVRC starts
    * AVRCP Spec 1.4 section 22.19 */
    uint8_t* p_data = p_start + 1; /* pdu */

    /* To get the list of all media players we simply need to use the predefined
    * PDU mentioned in above spec. */
    /* scope (1) + st item (4) + end item (4) + attr (1) */
    int nCount = cmd->attr_count;
    UINT16_TO_BE_STREAM(p_data, 10 + nCount*4);
    UINT8_TO_BE_STREAM(p_data, cmd->scope);       /* scope (1bytes) */
    UINT32_TO_BE_STREAM(p_data, cmd->start_item); /* start item (4bytes) */
    UINT32_TO_BE_STREAM(p_data, cmd->end_item);   /* end item (4bytes) */
    UINT8_TO_BE_STREAM(p_data, cmd->attr_count); /* attribute count (1bytes) */
    for(int i=0;i < nCount; i++)
        UINT32_TO_BE_STREAM(p_data, cmd->attrs[i]);
    p_pkt->len = (p_data - p_start);
    return AVRC_STS_NO_ERROR;
}

/*******************************************************************************
 *
 * Function         avrc_bld_get_folder_items_cmd
 *
 * Description      This function builds the get folder items cmd.
 *
 * Returns          AVRC_STS_NO_ERROR, if the command is built successfully
 *                  Otherwise, the error code.
 *
 ******************************************************************************/
static tAVRC_STS avrc_bld_get_item_attributes_cmd(BT_HDR* p_pkt,
                                               const tAVRC_GET_ATTRS_CMD* cmd) {
    AVRC_TRACE_API(
      "avrc_bld_get_item_attributes_cmd scope %d, uid %lld, uid_counter %x attr_count %d",
      cmd->scope, cmd->uid, cmd->uid_counter,cmd->attr_count);
    uint8_t* p_start = (uint8_t*)(p_pkt + 1) + p_pkt->offset;
    /* This is where the PDU specific for AVRC starts
    * AVRCP Spec 1.4 section 22.19 */
    uint8_t* p_data = p_start + 1; /* pdu */

    /* To get the list of all media players we simply need to use the predefined
    * PDU mentioned in above spec. */
    /* scope (1) + UID (8) + UID Counte (2) + Number of Attributes (1) + Attribute IDs(4N)*/
    int nCount = cmd->attr_count;
    UINT16_TO_BE_STREAM(p_data, 12 + nCount*4);
    UINT8_TO_BE_STREAM(p_data, cmd->scope);       /* scope (1bytes) */
    UINT64_TO_BE_STREAM(p_data, cmd->uid);  /* uid (8bytes) */
    UINT16_TO_BE_STREAM(p_data, cmd->uid_counter);   /* uid counter (2bytes) */
    UINT8_TO_BE_STREAM(p_data, cmd->attr_count); /* attribute count (1bytes) */
    for(int i=0;i < nCount; i++)
        UINT32_TO_BE_STREAM(p_data, cmd->attrs[i]);
    p_pkt->len = (p_data - p_start);
    return AVRC_STS_NO_ERROR;
}

/*******************************************************************************
 *
 * Function         avrc_bld_play_item__cmd
 *
 * Description      This function builds the play items cmd.
 *
 * Returns          AVRC_STS_NO_ERROR, if the command is built successfully
 *                  Otherwise, the error code.
 *
 ******************************************************************************/
static tAVRC_STS avrc_bld_play_item_cmd(BT_HDR* p_pkt,
                                               const tAVRC_PLAY_ITEM_CMD* cmd) {
    AVRC_TRACE_API(
      "avrc_bld_play_item__cmd scope %d, uid %lld, uid_counter %x",
      cmd->scope, cmd->uid, cmd->uid_counter);
    uint8_t* p_start = (uint8_t*)(p_pkt + 1) + p_pkt->offset;
    uint8_t* p_data = p_start + 2; /* pdu + rsvd */
    /* add fixed length 11 */
    UINT16_TO_BE_STREAM(p_data, 0xb);
    /* Add scope */
    UINT8_TO_BE_STREAM(p_data, cmd->scope);
    /* Add UID */
    UINT64_TO_BE_STREAM(p_data, cmd->uid);
    /* Add UID Counter */
    UINT16_TO_BE_STREAM(p_data, cmd->uid_counter);
    p_pkt->len = (p_data - p_start);
    return AVRC_STS_NO_ERROR;

}

/*******************************************************************************
 *
 * Function         avrc_bld_addto_nowplaying_cmd
 *
 * Description      This function builds the addto now playing cmd.
 *
 * Returns          AVRC_STS_NO_ERROR, if the command is built successfully
 *                  Otherwise, the error code.
 *
 ******************************************************************************/
static tAVRC_STS avrc_bld_addto_nowplaying_cmd(BT_HDR* p_pkt,
                                               const tAVRC_ADD_TO_PLAY_CMD* cmd) {
    AVRC_TRACE_API(
      "avrc_bld_addto_nowplaying_cmd scope %d, uid %lld, uid_counter %x",
      cmd->scope, cmd->uid, cmd->uid_counter);
    uint8_t* p_start = (uint8_t*)(p_pkt + 1) + p_pkt->offset;
    uint8_t* p_data = p_start + 2; /* pdu + rsvd */
    /* add fixed length 11 */
    UINT16_TO_BE_STREAM(p_data, 0xb);
    /* Add scope */
    UINT8_TO_BE_STREAM(p_data, cmd->scope);
    /* Add UID */
    UINT64_TO_BE_STREAM(p_data, cmd->uid);
    /* Add UID Counter */
    UINT16_TO_BE_STREAM(p_data, cmd->uid_counter);
    p_pkt->len = (p_data - p_start);
    return AVRC_STS_NO_ERROR;

}

/*******************************************************************************
 *
 * Function         avrc_bld_search_cmd
 *
 * Description      This function builds the search command
 *
 * Returns          AVRC_STS_NO_ERROR, if the command is built successfully
 *                  Otherwise, the error code.
 *
 ******************************************************************************/
static tAVRC_STS avrc_bld_search_cmd(BT_HDR* p_pkt,
                                            const tAVRC_SEARCH_CMD* cmd) {
    AVRC_TRACE_API("avrc_bld_search_cmd");
    uint8_t* p_start = (uint8_t*)(p_pkt + 1) + p_pkt->offset;
    /* This is where the PDU specific for AVRC starts
    * AVRCP Spec 1.4 section 22.19 */
    uint8_t* p_data = p_start + 1; /* pdu */

    /* To change folder we need to provide the following:
    * Character set (2) + Length (2) + string (Length)
    */
    int nLen = cmd->string.str_len;
    UINT16_TO_BE_STREAM(p_data, 4 + nLen);
    UINT16_TO_BE_STREAM(p_data, cmd->string.charset_id);
    UINT16_TO_BE_STREAM(p_data, cmd->string.str_len);
    ARRAY_TO_BE_STREAM(p_data, cmd->string.p_str, nLen);
    p_pkt->len = (p_data - p_start);
    return AVRC_STS_NO_ERROR;
}



/*******************************************************************************
 *
 * Function         avrc_bld_change_path_cmd
 *
 * Description      This function builds the change path command
 *
 * Returns          AVRC_STS_NO_ERROR, if the command is built successfully
 *                  Otherwise, the error code.
 *
 ******************************************************************************/
static tAVRC_STS avrc_bld_change_path_cmd(BT_HDR* p_pkt,
                                            const tAVRC_CHG_PATH_CMD* cmd) {
    AVRC_TRACE_API("avrc_bld_change_folder_cmd");
    uint8_t* p_start = (uint8_t*)(p_pkt + 1) + p_pkt->offset;
    /* This is where the PDU specific for AVRC starts
    * AVRCP Spec 1.4 section 22.19 */
    uint8_t* p_data = p_start + 1; /* pdu */

    /* To change folder we need to provide the following:
    * UID Counter (2) + Direction (1) + UID (8) = 11bytes
    */
    UINT16_TO_BE_STREAM(p_data, 11);
    UINT16_TO_BE_STREAM(p_data, cmd->uid_counter);
    UINT8_TO_BE_STREAM(p_data, cmd->direction);
    UINT64_TO_BE_STREAM(p_data, cmd->folder_uid);
    p_pkt->len = (p_data - p_start);
    return AVRC_STS_NO_ERROR;
}

#endif

/*******************************************************************************
**
** Function         avrc_bld_init_cmd_buffer
**
** Description      This function initializes the command buffer based on PDU
**
** Returns          NULL, if no GKI buffer or failure to build the message.
**                  Otherwise, the GKI buffer that contains the initialized message.
**
*******************************************************************************/
static BT_HDR *avrc_bld_init_cmd_buffer(tAVRC_COMMAND *p_cmd)
{
    uint16_t chnl = AVCT_DATA_CTRL;
    uint8_t opcode = avrc_opcode_from_pdu(p_cmd->pdu);
    AVRC_TRACE_API("avrc_bld_init_cmd_buffer: pdu=%x, opcode=%x", p_cmd->pdu,
                   opcode);

    uint16_t offset = 0;
    switch (opcode) {
      case AVRC_OP_BROWSE:
        chnl = AVCT_DATA_BROWSE;
        offset = AVCT_BROWSE_OFFSET;
        break;

      case AVRC_OP_PASS_THRU:
        offset = AVRC_MSG_PASS_THRU_OFFSET;
        break;

      case AVRC_OP_VENDOR:
        offset = AVRC_MSG_VENDOR_OFFSET;
        break;
    }

    /* allocate and initialize the buffer */
    BT_HDR* p_pkt = (BT_HDR*)osi_malloc(AVRC_META_CMD_BUF_SIZE);
    uint8_t *p_data, *p_start;

    p_pkt->layer_specific = chnl;
    p_pkt->event = opcode;
    p_pkt->offset = offset;
    p_data = (uint8_t*)(p_pkt + 1) + p_pkt->offset;
    p_start = p_data;

    /* pass thru - group navigation - has a two byte op_id, so dont do it here */
    if (opcode != AVRC_OP_PASS_THRU) *p_data++ = p_cmd->pdu;

    switch (opcode) {
      case AVRC_OP_VENDOR:
        /* reserved 0, packet_type 0 */
        UINT8_TO_BE_STREAM(p_data, 0);
        /* continue to the next "case to add length */
        /* add fixed lenth - 0 */
        UINT16_TO_BE_STREAM(p_data, 0);
        break;
    }

    p_pkt->len = (p_data - p_start);
    p_cmd->cmd.opcode = opcode;

    return p_pkt;

}

/*******************************************************************************
**
** Function         AVRC_BldCommand
**
** Description      This function builds the given AVRCP command to the given
**                  GKI buffer
**
** Returns          AVRC_STS_NO_ERROR, if the command is built successfully
**                  Otherwise, the error code.
**
*******************************************************************************/
tAVRC_STS AVRC_BldCommand( tAVRC_COMMAND *p_cmd, BT_HDR **pp_pkt)
{
    tAVRC_STS status = AVRC_STS_BAD_PARAM;
    BOOLEAN alloc = FALSE;
    AVRC_TRACE_API("AVRC_BldCommand: pdu=%x status=%x", p_cmd->cmd.pdu, p_cmd->cmd.status);
    if (!p_cmd || !pp_pkt)
    {
        AVRC_TRACE_API("AVRC_BldCommand. Invalid parameters passed. p_cmd=%p, pp_pkt=%p",
            p_cmd, pp_pkt);
        return AVRC_STS_BAD_PARAM;
    }

    if (*pp_pkt == NULL)
    {
        if ((*pp_pkt = avrc_bld_init_cmd_buffer(p_cmd)) == NULL)
        {
            AVRC_TRACE_API("AVRC_BldCommand: Failed to initialize command buffer");
            return AVRC_STS_INTERNAL_ERR;
        }
        alloc = TRUE;
    }
    status = AVRC_STS_NO_ERROR;
    BT_HDR* p_pkt = *pp_pkt;

    switch (p_cmd->pdu)
    {
    case AVRC_PDU_REQUEST_CONTINUATION_RSP:     /*        0x40 */
        status = avrc_bld_next_cmd(&p_cmd->continu, p_pkt);
        break;

    case AVRC_PDU_ABORT_CONTINUATION_RSP:       /*          0x41 */
        status = avrc_bld_next_cmd(&p_cmd->abort, p_pkt);
        break;
#if (AVRC_ADV_CTRL_INCLUDED == TRUE)
    case AVRC_PDU_SET_ABSOLUTE_VOLUME:         /* 0x50 */
        status = avrc_bld_set_abs_volume_cmd(&p_cmd->volume, p_pkt);
        break;
#endif
    case AVRC_PDU_REGISTER_NOTIFICATION:      /* 0x31 */
#if (AVRC_ADV_CTRL_INCLUDED == TRUE)
        status=avrc_bld_register_notifn(p_pkt,p_cmd->reg_notif.event_id,p_cmd->reg_notif.param);
#endif
        break;
#if (AVRC_CTLR_INCLUDED == TRUE)
    case AVRC_PDU_GET_CAPABILITIES:
        status = avrc_bld_get_capability_cmd(p_pkt, p_cmd->get_caps.capability_id);
        break;
    case AVRC_PDU_LIST_PLAYER_APP_ATTR:
        status = avrc_bld_list_player_app_attr_cmd(p_pkt);
        break;
    case AVRC_PDU_LIST_PLAYER_APP_VALUES:
        status = avrc_bld_list_player_app_values_cmd(p_pkt,p_cmd->list_app_values.attr_id);
        break;
    case AVRC_PDU_GET_CUR_PLAYER_APP_VALUE:
        /* Fix for below Klockwork Issue,taken care in the function
         * defination avrc_bld_get_current_player_app_values_cmd()
         * Array 'p_cmd->get_cur_app_val.attrs' of
         * size 8 may use index value(s) 16..254*/
        status = avrc_bld_get_current_player_app_values_cmd(p_pkt,
             p_cmd->get_cur_app_val.num_attr,p_cmd->get_cur_app_val.attrs);
        break;
    case AVRC_PDU_SET_PLAYER_APP_VALUE:
        status = avrc_bld_set_current_player_app_values_cmd(p_pkt,
                     p_cmd->set_app_val.num_val,p_cmd->set_app_val.p_vals);
        break;
    case AVRC_PDU_GET_PLAYER_APP_ATTR_TEXT:
        avrc_bld_get_player_app_setting_attr_text_cmd(p_pkt, &p_cmd->get_app_attr_txt);
        break;
    case AVRC_PDU_GET_PLAYER_APP_VALUE_TEXT:
        avrc_bld_get_player_app_setting_value_text_cmd(p_pkt, &p_cmd->get_app_val_txt);
        break;
    case AVRC_PDU_GET_ELEMENT_ATTR:
        /* Fix for below Klockwork Issue,taken care in the function
         * defination avrc_bld_get_element_attr_cmd()
         * Array 'p_cmd->get_elem_attrs.attrs' of
         * size 8 may use index value(s) 8..254*/
        status = avrc_bld_get_element_attr_cmd(p_pkt,
              p_cmd->get_elem_attrs.num_attr,p_cmd->get_elem_attrs.attrs);
        break;
    case AVRC_PDU_GET_PLAY_STATUS:
        status = avrc_bld_get_play_status_cmd(p_pkt);
        break;
    case AVRC_PDU_SET_ADDRESSED_PLAYER:
      status = avrc_bld_set_addressed_player_cmd(p_pkt, &(p_cmd->addr_player));
      break;
    case AVRC_PDU_SET_BROWSED_PLAYER:
      status = avrc_bld_set_browsed_player_cmd(p_pkt, &(p_cmd->br_player));
      break;
    case AVRC_PDU_CHANGE_PATH:
      status = avrc_bld_change_path_cmd(p_pkt, &(p_cmd->chg_path));
      break;
    case AVRC_PDU_GET_FOLDER_ITEMS:
      status = avrc_bld_get_folder_items_cmd(p_pkt, &(p_cmd->get_items));
      break;
    case AVRC_PDU_GET_ITEM_ATTRIBUTES:
      status = avrc_bld_get_item_attributes_cmd(p_pkt, &(p_cmd->get_attrs));
      break;
    case AVRC_PDU_PLAY_ITEM:
      status = avrc_bld_play_item_cmd(p_pkt, &(p_cmd->play_item));
      break;
    case AVRC_PDU_ADD_TO_NOW_PLAYING:
      status = avrc_bld_addto_nowplaying_cmd(p_pkt, &(p_cmd->add_to_play));
      break;
    case AVRC_PDU_SEARCH:
      status = avrc_bld_search_cmd(p_pkt, &(p_cmd->search));
      break;


#endif
    }

    if (alloc && (status != AVRC_STS_NO_ERROR) )
    {
        osi_free(p_pkt);
        *pp_pkt = NULL;
    }
    AVRC_TRACE_API("AVRC_BldCommand: returning %d", status);
    return status;
}
#endif /* (AVRC_METADATA_INCLUDED == TRUE) */
