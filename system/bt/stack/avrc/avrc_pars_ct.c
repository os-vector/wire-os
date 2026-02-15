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
#include "bt_utils.h"

/*****************************************************************************
**  Global data
*****************************************************************************/

#if (AVRC_METADATA_INCLUDED == TRUE)

/*******************************************************************************
**
** Function         avrc_pars_vendor_rsp
**
** Description      This function parses the vendor specific commands defined by
**                  Bluetooth SIG
**
** Returns          AVRC_STS_NO_ERROR, if the message in p_data is parsed successfully.
**                  Otherwise, the error code defined by AVRCP 1.4
**
*******************************************************************************/
static tAVRC_STS avrc_pars_vendor_rsp(tAVRC_MSG_VENDOR *p_msg, tAVRC_RESPONSE *p_result)
{
    tAVRC_STS  status = AVRC_STS_NO_ERROR;
    UINT8   *p;
    UINT16  len;
#if (AVRC_ADV_CTRL_INCLUDED == TRUE)
    UINT8 eventid=0;
#endif

    /* Check the vendor data */
    if (p_msg->vendor_len == 0)
        return AVRC_STS_NO_ERROR;
    if (p_msg->p_vendor_data == NULL)
        return AVRC_STS_INTERNAL_ERR;

    p = p_msg->p_vendor_data;
    BE_STREAM_TO_UINT8 (p_result->pdu, p);
    p++; /* skip the reserved/packe_type byte */
    BE_STREAM_TO_UINT16 (len, p);
    AVRC_TRACE_DEBUG("%s ctype:0x%x pdu:0x%x, len:%d/0x%x",
                     __func__, p_msg->hdr.ctype, p_result->pdu, len, len);
    if (p_msg->hdr.ctype == AVRC_RSP_REJ)
    {
        p_result->rsp.status = *p;
        return p_result->rsp.status;
    }

    switch (p_result->pdu)
    {
    /* case AVRC_PDU_REQUEST_CONTINUATION_RSP: 0x40 */
    /* case AVRC_PDU_ABORT_CONTINUATION_RSP:   0x41 */

#if (AVRC_ADV_CTRL_INCLUDED == TRUE)
    case AVRC_PDU_SET_ABSOLUTE_VOLUME:      /* 0x50 */
        if (len != 1)
            status = AVRC_STS_INTERNAL_ERR;
        else
        {
            BE_STREAM_TO_UINT8 (p_result->volume.volume, p);
        }
        break;
#endif /* (AVRC_ADV_CTRL_INCLUDED == TRUE) */

    case AVRC_PDU_REGISTER_NOTIFICATION:    /* 0x31 */
#if (AVRC_ADV_CTRL_INCLUDED == TRUE)
        BE_STREAM_TO_UINT8 (eventid, p);
        if(AVRC_EVT_VOLUME_CHANGE==eventid
            && (AVRC_RSP_CHANGED==p_msg->hdr.ctype || AVRC_RSP_INTERIM==p_msg->hdr.ctype
            || AVRC_RSP_REJ==p_msg->hdr.ctype || AVRC_RSP_NOT_IMPL==p_msg->hdr.ctype))
        {
            p_result->reg_notif.status=p_msg->hdr.ctype;
            p_result->reg_notif.event_id=eventid;
            BE_STREAM_TO_UINT8 (p_result->reg_notif.param.volume, p);
        }
        AVRC_TRACE_DEBUG("%s PDU reg notif response:event %x, volume %x",
                         __func__, eventid, p_result->reg_notif.param.volume);
#endif /* (AVRC_ADV_CTRL_INCLUDED == TRUE) */
        break;
    default:
        status = AVRC_STS_BAD_CMD;
        break;
    }

    return status;
}

void avrc_parse_notification_rsp (UINT8 *p_stream, tAVRC_REG_NOTIF_RSP *p_rsp)
{
    BE_STREAM_TO_UINT8(p_rsp->event_id, p_stream);
    switch (p_rsp->event_id)
    {
        case AVRC_EVT_PLAY_STATUS_CHANGE:
            BE_STREAM_TO_UINT8(p_rsp->param.play_status, p_stream);
            break;

        case AVRC_EVT_TRACK_CHANGE:
            BE_STREAM_TO_ARRAY(p_stream, p_rsp->param.track, 8);
            break;

        case AVRC_EVT_APP_SETTING_CHANGE:
            BE_STREAM_TO_UINT8(p_rsp->param.player_setting.num_attr, p_stream);
            for (int index = 0; index < p_rsp->param.player_setting.num_attr; index++)
            {
                BE_STREAM_TO_UINT8(p_rsp->param.player_setting.attr_id[index], p_stream);
                BE_STREAM_TO_UINT8(p_rsp->param.player_setting.attr_value[index], p_stream);
            }
            break;

        case AVRC_EVT_PLAY_POS_CHANGED:
            BE_STREAM_TO_UINT32(p_rsp->param.play_pos, p_stream);
            break;

        case AVRC_EVT_VOLUME_CHANGE:
            BE_STREAM_TO_UINT8(p_rsp->param.volume, p_stream);
            break;

        case AVRC_EVT_NOW_PLAYING_CHANGE:
            break;

        case AVRC_EVT_AVAL_PLAYERS_CHANGE:
            break;

        case AVRC_EVT_ADDR_PLAYER_CHANGE:
            BE_STREAM_TO_UINT16(p_rsp->param.addr_player.player_id, p_stream);
            BE_STREAM_TO_UINT16(p_rsp->param.addr_player.uid_counter, p_stream);
            break;

        case AVRC_EVT_UIDS_CHANGE:
            break;

        case AVRC_EVT_TRACK_REACHED_END:
        case AVRC_EVT_TRACK_REACHED_START:
        case AVRC_EVT_BATTERY_STATUS_CHANGE:
        case AVRC_EVT_SYSTEM_STATUS_CHANGE:
        default:
            break;
    }
}

#if (AVRC_CTLR_INCLUDED == TRUE)
static tAVRC_STS avrc_pars_browse_rsp(tAVRC_MSG_BROWSE* p_msg,
                                      tAVRC_RESPONSE* p_rsp) {
  tAVRC_STS status = AVRC_STS_NO_ERROR;
  uint8_t pdu;

  if (p_msg->browse_len == 0) {
    AVRC_TRACE_ERROR("%s length ", p_msg->browse_len);
    return AVRC_STS_BAD_PARAM;
  }

  uint8_t* p = p_msg->p_browse_data;

  /* read the pdu */
  BE_STREAM_TO_UINT8(pdu, p);
  uint16_t pkt_len;
  /* read the entire packet len */
  BE_STREAM_TO_UINT16(pkt_len, p);

  AVRC_TRACE_DEBUG("%s pdu %d", __func__, pdu);

  /* used to track how much we have read, if we cannot read anymore but the
   * packet says so then we have a malformed packet. Also vice versa. */
  uint16_t pkt_len_read = 0;

  switch (pdu) {
    case AVRC_PDU_GET_FOLDER_ITEMS: {
      tAVRC_GET_ITEMS_RSP* get_item_rsp = &(p_rsp->get_items);
      /* Copy back the PDU */
      get_item_rsp->pdu = pdu;
      /* read the status */
      BE_STREAM_TO_UINT8(get_item_rsp->status, p);
      /* read the UID counter */
      BE_STREAM_TO_UINT16(get_item_rsp->uid_counter, p);
      /* read the number of items */
      BE_STREAM_TO_UINT16(get_item_rsp->item_count, p);
      pkt_len_read += 5;

      AVRC_TRACE_DEBUG(
          "%s pdu %d status %d pkt_len %d uid counter %d item count %d",
          __func__, get_item_rsp->pdu, get_item_rsp->status, pkt_len,
          get_item_rsp->uid_counter, get_item_rsp->item_count);

      if (get_item_rsp->status != AVRC_STS_NO_ERROR) {
        AVRC_TRACE_WARNING("%s returning error %d", __func__,
                           get_item_rsp->status);
        return get_item_rsp->status;
      }

      /* get each of the items */
      get_item_rsp->p_item_list = (tAVRC_ITEM*)osi_malloc(
          get_item_rsp->item_count * (sizeof(tAVRC_ITEM)));
      tAVRC_ITEM* curr_item = get_item_rsp->p_item_list;
      for (int i = 0; i < get_item_rsp->item_count; i++) {
        BE_STREAM_TO_UINT8(curr_item->item_type, p);
        pkt_len_read += 1;
        AVRC_TRACE_DEBUG("%s item type %d", __func__, curr_item->item_type);
        switch (curr_item->item_type) {
          case AVRC_ITEM_PLAYER: {
            /* Handle player */
            tAVRC_ITEM_PLAYER* player = &(curr_item->u.player);
            uint8_t player_len;
            BE_STREAM_TO_UINT16(player_len, p);
            BE_STREAM_TO_UINT16(player->player_id, p);
            BE_STREAM_TO_UINT8(player->major_type, p);
            BE_STREAM_TO_UINT32(player->sub_type, p);
            BE_STREAM_TO_UINT8(player->play_status, p);
            BE_STREAM_TO_ARRAY(p, player->features, AVRC_FEATURE_MASK_SIZE);
            pkt_len_read += (10 + AVRC_FEATURE_MASK_SIZE);

            /* read str */
            BE_STREAM_TO_UINT16(player->name.charset_id, p);
            BE_STREAM_TO_UINT16(player->name.str_len, p);
            player->name.p_str = (uint8_t*)osi_malloc(
                (player->name.str_len + 1) * sizeof(uint8_t));
            BE_STREAM_TO_ARRAY(p, player->name.p_str, player->name.str_len);
            pkt_len_read += (4 + player->name.str_len);
            AVRC_TRACE_DEBUG(
                "%s type %d id %d mtype %d stype %d ps %d cs %d name len %d",
                __func__, curr_item->item_type, player->player_id,
                player->major_type, player->sub_type, player->play_status,
                player->name.charset_id, player->name.str_len);
          } break;

          case AVRC_ITEM_FOLDER: {
            tAVRC_ITEM_FOLDER* folder = &(curr_item->u.folder);
            uint16_t folder_len;
            BE_STREAM_TO_UINT16(folder_len, p);

            BE_STREAM_TO_ARRAY(p, folder->uid, AVRC_UID_SIZE);
            BE_STREAM_TO_UINT8(folder->type, p);
            BE_STREAM_TO_UINT8(folder->playable, p);
            pkt_len_read += (4 + AVRC_UID_SIZE);

            /* read str, encoding to be handled by upper layers */
            BE_STREAM_TO_UINT16(folder->name.charset_id, p);
            BE_STREAM_TO_UINT16(folder->name.str_len, p);
            folder->name.p_str = (uint8_t*)osi_malloc(
                (folder->name.str_len + 1) * sizeof(uint8_t));
            BE_STREAM_TO_ARRAY(p, folder->name.p_str, folder->name.str_len);
            pkt_len_read += (4 + folder->name.str_len);
            AVRC_TRACE_DEBUG("%s type %d playable %d cs %d name len %d",
                             __func__, folder->type, folder->playable,
                             folder->name.charset_id, folder->name.str_len);
          } break;

          case AVRC_ITEM_MEDIA: {
            tAVRC_ITEM_MEDIA* media = &(curr_item->u.media);
            uint8_t media_len;
            BE_STREAM_TO_UINT16(media_len, p);
            BE_STREAM_TO_ARRAY(p, media->uid, AVRC_UID_SIZE);
            BE_STREAM_TO_UINT8(media->type, p);
            pkt_len_read += (3 + AVRC_UID_SIZE);

            /* read str, encoding to be handled by upper layers */
            BE_STREAM_TO_UINT16(media->name.charset_id, p);
            BE_STREAM_TO_UINT16(media->name.str_len, p);
            media->name.p_str =
                (uint8_t*)osi_malloc((media->name.str_len) * sizeof(uint8_t));
            BE_STREAM_TO_ARRAY(p, media->name.p_str, media->name.str_len);

            BE_STREAM_TO_UINT8(media->attr_count, p);
            AVRC_TRACE_DEBUG("%s media type %d charset id %d len %d attr ct %d",
                             __func__, media->type, media->name.charset_id,
                             media->name.str_len, media->attr_count);
            pkt_len_read += (5 + media->name.str_len);

            media->p_attr_list = (tAVRC_ATTR_ENTRY*)osi_malloc(
                media->attr_count * sizeof(tAVRC_ATTR_ENTRY));
            for (int jk = 0; jk < media->attr_count; jk++) {
              tAVRC_ATTR_ENTRY* attr_entry = &(media->p_attr_list[jk]);
              BE_STREAM_TO_UINT32(attr_entry->attr_id, p);

              /* Parse the name now */
              BE_STREAM_TO_UINT16(attr_entry->name.charset_id, p);
              BE_STREAM_TO_UINT16(attr_entry->name.str_len, p);
              attr_entry->name.p_str = (uint8_t*)osi_malloc(
                  attr_entry->name.str_len * sizeof(uint8_t));
              BE_STREAM_TO_ARRAY(p, attr_entry->name.p_str,
                                 attr_entry->name.str_len);
              pkt_len_read += (8 + attr_entry->name.str_len);
              AVRC_TRACE_DEBUG("%s media attr id %d cs %d name len %d",
                               __func__, attr_entry->attr_id,
                               attr_entry->name.charset_id,
                               attr_entry->name.str_len);
            }
          } break;

          default:
            AVRC_TRACE_ERROR("%s item type not handled %d", __func__,
                             curr_item->item_type);
            return AVRC_STS_INTERNAL_ERR;
        }

        /* we check if we have overrun */
        if (pkt_len_read > pkt_len) {
          AVRC_TRACE_ERROR("%s overflow in read pkt_len %d pkt_len_read %d",
                           __func__, pkt_len, pkt_len_read);
          return AVRC_STS_BAD_CMD;
        }
        AVRC_TRACE_DEBUG("%s pkt_len %d pkt_len_read %d", __func__, pkt_len,
                         pkt_len_read);

        /* advance to populate the next item */
        curr_item++;
      }
      break;
    }

    case AVRC_PDU_CHANGE_PATH: {
      tAVRC_CHG_PATH_RSP* change_path_rsp = &(p_rsp->chg_path);
      /* Copyback the PDU */
      change_path_rsp->pdu = pdu;
      /* Read the status */
      BE_STREAM_TO_UINT8(change_path_rsp->status, p);
      if (change_path_rsp->status != AVRC_STS_NO_ERROR) {
        AVRC_TRACE_ERROR(
            "%s Stopping further parsing because status is error %d",
            __func__, change_path_rsp->status);
        return change_path_rsp->status;
      }
      /* Read the number of items in folder */
      BE_STREAM_TO_UINT32(change_path_rsp->num_items, p);
      pkt_len_read += 5;

      AVRC_TRACE_DEBUG("%s pdu %d status %d item count %d", __func__,
                       change_path_rsp->pdu, change_path_rsp->status,
                       change_path_rsp->num_items);
      break;
    }

    case AVRC_PDU_SEARCH: {
      tAVRC_SEARCH_RSP* search_rsp = &(p_rsp->search);
      /* Copyback the PDU */
      search_rsp->pdu = pdu;
      /* Read the status */
      BE_STREAM_TO_UINT8(search_rsp->status, p);
      if (search_rsp->status != AVRC_STS_NO_ERROR) {
        AVRC_TRACE_ERROR(
            "%s Stopping further parsing because status is error %d",
            __func__, search_rsp->status);
        return search_rsp->status;
      }
      BE_STREAM_TO_UINT16(search_rsp->uid_counter, p);
      BE_STREAM_TO_UINT32(search_rsp->num_items, p);
      pkt_len_read += 7;

      AVRC_TRACE_DEBUG("%s pdu %d status %d item count %d", __func__,
                       search_rsp->pdu, search_rsp->status,
                       search_rsp->num_items);
      break;
    }

    case AVRC_PDU_GET_ITEM_ATTRIBUTES: {
        tAVRC_GET_ATTRS_RSP* get_attrs_rsp = &(p_rsp->get_attrs);
        /* Copyback the PDU */
        get_attrs_rsp->pdu = pdu;
        /* Read the status */
        BE_STREAM_TO_UINT8(get_attrs_rsp->status, p);

      if (get_attrs_rsp->status != AVRC_STS_NO_ERROR) {
        AVRC_TRACE_ERROR(
            "%s Stopping further parsing because status is error %d",
            __func__, get_attrs_rsp->status);
        return get_attrs_rsp->status;
      }

        /* Read the number of items in folder */
        BE_STREAM_TO_UINT8(get_attrs_rsp->attr_count, p);
        pkt_len_read += 2;

        AVRC_TRACE_DEBUG(
          "%s AVRC_PDU_GET_ITEM_ATTRIBUTES status %d attr_count %d ",
          __func__, get_attrs_rsp->status, get_attrs_rsp->attr_count);

        if(get_attrs_rsp->attr_count > 0)
        {
            get_attrs_rsp->p_attr_list = (tAVRC_ATTR_ENTRY*)osi_malloc(
                get_attrs_rsp->attr_count * sizeof(tAVRC_ATTR_ENTRY));

            /* Read each of the attr */
            for (uint32_t i = 0; i < get_attrs_rsp->attr_count; i++) {
              tAVRC_ATTR_ENTRY* pAttr = &(get_attrs_rsp->p_attr_list[i]);
              BE_STREAM_TO_UINT32(pAttr->attr_id, p);
              BE_STREAM_TO_UINT16(pAttr->name.charset_id, p);
              BE_STREAM_TO_UINT16(pAttr->name.str_len, p);

              AVRC_TRACE_DEBUG("%s AVRC_PDU_GET_ITEM_ATTRIBUTES attr: %d attr_id: %d charset_id: %d str_len: %d",
                               __func__, i, pAttr->attr_id, pAttr->name.charset_id, pAttr->name.str_len);
              pAttr->name.p_str =
                  (uint8_t*)osi_malloc((pAttr->name.str_len + 1) * sizeof(uint8_t));
              BE_STREAM_TO_ARRAY(p, pAttr->name.p_str, pAttr->name.str_len);
              pkt_len_read += (8 + pAttr->name.str_len);
             }
        }
      else
           get_attrs_rsp->p_attr_list = NULL;
      break;
    }

    case AVRC_PDU_SET_BROWSED_PLAYER: {
      tAVRC_SET_BR_PLAYER_RSP* set_br_pl_rsp = &(p_rsp->br_player);
      /* Copyback the PDU */
      set_br_pl_rsp->pdu = pdu;

      /* Read the status */
      BE_STREAM_TO_UINT8(set_br_pl_rsp->status, p);

      if (set_br_pl_rsp->status != AVRC_STS_NO_ERROR) {
        AVRC_TRACE_ERROR(
            "%s Stopping further parsing because player not browsable sts %d",
            __func__, set_br_pl_rsp->status);
        return set_br_pl_rsp->status;
      }
      BE_STREAM_TO_UINT16(set_br_pl_rsp->uid_counter, p);
      BE_STREAM_TO_UINT32(set_br_pl_rsp->num_items, p);
      BE_STREAM_TO_UINT16(set_br_pl_rsp->charset_id, p);
      BE_STREAM_TO_UINT8(set_br_pl_rsp->folder_depth, p);
      AVRC_TRACE_DEBUG(
          "%s AVRC_PDU_SET_BROWSED_PLAYER status %d items %d cs %d depth %d",
          __func__, set_br_pl_rsp->status, set_br_pl_rsp->num_items,
          set_br_pl_rsp->charset_id, set_br_pl_rsp->folder_depth);
      pkt_len_read += 10;

      if(set_br_pl_rsp->folder_depth > 0)
      {
          set_br_pl_rsp->p_folders = (tAVRC_NAME*)osi_malloc(
              set_br_pl_rsp->folder_depth * sizeof(tAVRC_NAME));

          /* Read each of the folder in the depth */
          for (uint32_t i = 0; i < set_br_pl_rsp->folder_depth; i++) {
            tAVRC_NAME* folder_name = &(set_br_pl_rsp->p_folders[i]);
            BE_STREAM_TO_UINT16(folder_name->str_len, p);
            AVRC_TRACE_DEBUG("%s AVRC_PDU_SET_BROWSED_PLAYER item: %d len: %d",
                             __func__, i, folder_name->str_len);
            folder_name->p_str =
                (uint8_t*)osi_malloc((folder_name->str_len + 1) * sizeof(uint8_t));
            BE_STREAM_TO_ARRAY(p, folder_name->p_str, folder_name->str_len);
            pkt_len_read += (2 + folder_name->str_len);
          }
      }
      else
           set_br_pl_rsp->p_folders = NULL;
      break;
    }

    default:
      AVRC_TRACE_ERROR("%s pdu %d not handled", __func__, pdu);
  }

  if (pkt_len != pkt_len_read) {
    AVRC_TRACE_ERROR("%s finished pkt_len %d pkt_len_read %d", __func__,
                     pkt_len, pkt_len_read);
    return AVRC_STS_BAD_CMD;
  }
  return status;
}

/*******************************************************************************
**
** Function         avrc_ctrl_pars_vendor_rsp
**
** Description      This function parses the vendor specific commands defined by
**                  Bluetooth SIG
**
** Returns          AVRC_STS_NO_ERROR, if the message in p_data is parsed successfully.
**                  Otherwise, the error code defined by AVRCP 1.4
**
*******************************************************************************/
static tAVRC_STS avrc_ctrl_pars_vendor_rsp(
    tAVRC_MSG_VENDOR *p_msg, tAVRC_RESPONSE *p_result, UINT8* p_buf, UINT16* buf_len)
{
    UINT8   *p = p_msg->p_vendor_data;
    BE_STREAM_TO_UINT8 (p_result->pdu, p);
    p++; /* skip the reserved/packe_type byte */

    UINT16  len;
    BE_STREAM_TO_UINT16 (len, p);
    AVRC_TRACE_DEBUG("%s ctype:0x%x pdu:0x%x, len:%d",
                     __func__, p_msg->hdr.ctype, p_result->pdu, len);
    /* Todo: Issue in handling reject, check */
    if (p_msg->hdr.ctype == AVRC_RSP_REJ)
    {
        p_result->rsp.status = *p;
        return p_result->rsp.status;
    }

    /* TODO: Break the big switch into functions. */
    switch (p_result->pdu)
    {
    /* case AVRC_PDU_REQUEST_CONTINUATION_RSP: 0x40 */
    /* case AVRC_PDU_ABORT_CONTINUATION_RSP:   0x41 */

    case AVRC_PDU_REGISTER_NOTIFICATION:
        avrc_parse_notification_rsp(p, &p_result->reg_notif);
        break;

    case AVRC_PDU_GET_CAPABILITIES:
        if (len == 0)
        {
            p_result->get_caps.count = 0;
            p_result->get_caps.capability_id = 0;
            break;
        }
        BE_STREAM_TO_UINT8(p_result->get_caps.capability_id, p);
        BE_STREAM_TO_UINT8(p_result->get_caps.count, p);
        AVRC_TRACE_DEBUG("%s cap id = %d, cap_count = %d ",
                         __func__, p_result->get_caps.capability_id, p_result->get_caps.count);
        if (p_result->get_caps.capability_id == AVRC_CAP_COMPANY_ID)
        {
            for(int xx = 0; ((xx < p_result->get_caps.count) && (xx < AVRC_CAP_MAX_NUM_COMP_ID));
                xx++)
            {
                BE_STREAM_TO_UINT24(p_result->get_caps.param.company_id[xx], p);
            }
        }
        else if (p_result->get_caps.capability_id == AVRC_CAP_EVENTS_SUPPORTED)
        {
            for(int xx = 0; ((xx < p_result->get_caps.count) && (xx < AVRC_CAP_MAX_NUM_EVT_ID));
                xx++)
            {
                BE_STREAM_TO_UINT8(p_result->get_caps.param.event_id[xx], p);
            }
        }
        break;

    case AVRC_PDU_LIST_PLAYER_APP_ATTR:
        if (len == 0)
        {
            p_result->list_app_attr.num_attr = 0;
            break;
        }
        BE_STREAM_TO_UINT8(p_result->list_app_attr.num_attr, p);
        AVRC_TRACE_DEBUG("%s attr count = %d ", __func__, p_result->list_app_attr.num_attr);
        for(int xx = 0; xx < p_result->list_app_attr.num_attr; xx++)
        {
            BE_STREAM_TO_UINT8(p_result->list_app_attr.attrs[xx], p);
        }
        break;

    case AVRC_PDU_LIST_PLAYER_APP_VALUES:
        if (len == 0)
        {
            p_result->list_app_values.num_val = 0;
            break;
        }
        BE_STREAM_TO_UINT8(p_result->list_app_values.num_val, p);
        AVRC_TRACE_DEBUG("%s value count = %d ", __func__, p_result->list_app_values.num_val);
        for(int xx = 0; xx < p_result->list_app_values.num_val; xx++)
        {
            BE_STREAM_TO_UINT8(p_result->list_app_values.vals[xx], p);
        }
        break;

    case AVRC_PDU_GET_CUR_PLAYER_APP_VALUE:
    {
        if (len == 0)
        {
            p_result->get_cur_app_val.num_val = 0;
            break;
        }
        BE_STREAM_TO_UINT8(p_result->get_cur_app_val.num_val, p);
        tAVRC_APP_SETTING *app_sett =
            (tAVRC_APP_SETTING*)osi_malloc(p_result->get_cur_app_val.num_val*sizeof(tAVRC_APP_SETTING));
        AVRC_TRACE_DEBUG("%s attr count = %d ", __func__, p_result->get_cur_app_val.num_val);
        for (int xx = 0; xx < p_result->get_cur_app_val.num_val; xx++)
        {
            BE_STREAM_TO_UINT8(app_sett[xx].attr_id, p);
            BE_STREAM_TO_UINT8(app_sett[xx].attr_val, p);
        }
        p_result->get_cur_app_val.p_vals = app_sett;
    }
        break;

    case AVRC_PDU_GET_PLAYER_APP_ATTR_TEXT:
    {
        tAVRC_APP_SETTING_TEXT   *p_setting_text;
        UINT8                    num_attrs;

        if (len == 0)
        {
            p_result->get_app_attr_txt.num_attr = 0;
            break;
        }
        BE_STREAM_TO_UINT8(num_attrs, p);
        AVRC_TRACE_DEBUG("%s attr count = %d ", __func__, p_result->get_app_attr_txt.num_attr);
        p_result->get_app_attr_txt.num_attr = num_attrs;
        p_setting_text = (tAVRC_APP_SETTING_TEXT*)osi_malloc(num_attrs * sizeof(tAVRC_APP_SETTING_TEXT));
        for (int xx = 0; xx < num_attrs; xx++)
        {
            BE_STREAM_TO_UINT8(p_result->get_app_attr_txt.p_attrs[xx].attr_id, p);
            BE_STREAM_TO_UINT16(p_result->get_app_attr_txt.p_attrs[xx].charset_id, p);
            BE_STREAM_TO_UINT8(p_result->get_app_attr_txt.p_attrs[xx].str_len, p);
            if (p_result->get_app_attr_txt.p_attrs[xx].str_len != 0)
            {
                UINT8 *p_str = (UINT8 *)osi_malloc(p_result->get_app_attr_txt.p_attrs[xx].str_len);
                BE_STREAM_TO_ARRAY(p, p_str, p_result->get_app_attr_txt.p_attrs[xx].str_len);
                p_result->get_app_attr_txt.p_attrs[xx].p_str = p_str;
            } else {
                p_result->get_app_attr_txt.p_attrs[xx].p_str = NULL;
            }
        }
    }
        break;

    case AVRC_PDU_GET_PLAYER_APP_VALUE_TEXT:
    {
        tAVRC_APP_SETTING_TEXT   *p_setting_text;
        UINT8                    num_vals;

        if (len == 0)
        {
            p_result->get_app_val_txt.num_attr = 0;
            break;
        }
        BE_STREAM_TO_UINT8(num_vals, p);
        p_result->get_app_val_txt.num_attr = num_vals;
        AVRC_TRACE_DEBUG("%s value count = %d ", __func__, p_result->get_app_val_txt.num_attr);

        p_setting_text = (tAVRC_APP_SETTING_TEXT *)osi_malloc(num_vals * sizeof(tAVRC_APP_SETTING_TEXT));
        for (int i = 0; i < num_vals; i++) {
            BE_STREAM_TO_UINT8(p_result->get_app_val_txt.p_attrs[i].attr_id, p);
            BE_STREAM_TO_UINT16(p_result->get_app_val_txt.p_attrs[i].charset_id, p);
            BE_STREAM_TO_UINT8(p_result->get_app_val_txt.p_attrs[i].str_len, p);
            if (p_result->get_app_val_txt.p_attrs[i].str_len != 0) {
                UINT8 *p_str = (UINT8 *)osi_malloc(p_result->get_app_val_txt.p_attrs[i].str_len);
                BE_STREAM_TO_ARRAY(p, p_str, p_result->get_app_val_txt.p_attrs[i].str_len);
                p_result->get_app_val_txt.p_attrs[i].p_str = p_str;
            } else {
                p_result->get_app_val_txt.p_attrs[i].p_str = NULL;
            }
        }
    }
        break;

    case AVRC_PDU_SET_PLAYER_APP_VALUE:
        /* nothing comes as part of this rsp */
        break;

    case AVRC_PDU_GET_ELEMENT_ATTR:
    {
        UINT8               num_attrs;

        if (len <= 0)
        {
            p_result->get_elem_attrs.num_attr = 0;
            break;
        }
        BE_STREAM_TO_UINT8(num_attrs, p);
        p_result->get_elem_attrs.num_attr = num_attrs;
        if (num_attrs)
        {
            tAVRC_ATTR_ENTRY *p_attrs =
                (tAVRC_ATTR_ENTRY*)osi_malloc(num_attrs * sizeof(tAVRC_ATTR_ENTRY));
            for (int i = 0; i < num_attrs; i++) {
                BE_STREAM_TO_UINT32(p_attrs[i].attr_id, p);
                BE_STREAM_TO_UINT16(p_attrs[i].name.charset_id, p);
                BE_STREAM_TO_UINT16(p_attrs[i].name.str_len, p);
                if (p_attrs[i].name.str_len > 0) {
                    p_attrs[i].name.p_str = (UINT8 *)osi_malloc(p_attrs[i].name.str_len);
                    BE_STREAM_TO_ARRAY(p, p_attrs[i].name.p_str, p_attrs[i].name.str_len);
                }
            }
            p_result->get_elem_attrs.p_attrs = p_attrs;
        }
    }
        break;

    case AVRC_PDU_GET_PLAY_STATUS:
        if (len == 0)
            break;
        BE_STREAM_TO_UINT32(p_result->get_play_status.song_len, p);
        BE_STREAM_TO_UINT32(p_result->get_play_status.song_pos, p);
        BE_STREAM_TO_UINT8(p_result->get_play_status.play_status, p);
        break;
    case AVRC_PDU_SET_ADDRESSED_PLAYER:
         if (len == 0)
            break;
        BE_STREAM_TO_UINT8(p_result->addr_player.status, p);
       break;
   case AVRC_PDU_PLAY_ITEM:
        if (len == 0)
           break;
       BE_STREAM_TO_UINT8(p_result->play_item.status, p);
      break;
    case AVRC_PDU_ADD_TO_NOW_PLAYING:
        if (len == 0)
          break;
       BE_STREAM_TO_UINT8(p_result->add_to_play.status, p);
      break;

   case AVRC_PDU_SET_BROWSED_PLAYER:
        if (len == 0)
           break;
       UINT8 folder_depth;
       BE_STREAM_TO_UINT8(p_result->br_player.status, p);
       BE_STREAM_TO_UINT16(p_result->br_player.uid_counter, p);
       BE_STREAM_TO_UINT32(p_result->br_player.num_items, p);
       BE_STREAM_TO_UINT16(p_result->br_player.charset_id, p);
       BE_STREAM_TO_UINT8(folder_depth, p);
       p_result->br_player.folder_depth = folder_depth;
       if(folder_depth)
        {
            tAVRC_NAME *pFolderName =
                (tAVRC_NAME*)osi_malloc(folder_depth * sizeof(tAVRC_NAME));
            for (int i = 0; i < folder_depth; i++) {
                BE_STREAM_TO_UINT16(pFolderName[i].str_len, p);
                if (pFolderName[i].str_len > 0) {
                    pFolderName[i].p_str = (UINT8 *)osi_malloc(pFolderName[i].str_len);
                    BE_STREAM_TO_ARRAY(p, pFolderName[i].p_str, pFolderName[i].str_len);
                }
            }
            p_result->br_player.p_folders = pFolderName;
        }
      break;

    default:
        return AVRC_STS_BAD_CMD;
    }
    return AVRC_STS_NO_ERROR;
}

/*******************************************************************************
**
** Function         AVRC_Ctrl_ParsResponse
**
** Description      This function is a parse response for AVRCP Controller.
**
** Returns          AVRC_STS_NO_ERROR, if the message in p_data is parsed successfully.
**                  Otherwise, the error code defined by AVRCP 1.4
**
*******************************************************************************/
tAVRC_STS AVRC_Ctrl_ParsResponse (tAVRC_MSG *p_msg, tAVRC_RESPONSE *p_result, UINT8 *p_buf, UINT16* buf_len)
{
    tAVRC_STS  status = AVRC_STS_INTERNAL_ERR;
    if (p_msg && p_result)
    {
        switch (p_msg->hdr.opcode)
        {
        case AVRC_OP_VENDOR:     /*  0x00    Vendor-dependent commands */
            status = avrc_ctrl_pars_vendor_rsp(&p_msg->vendor, p_result, p_buf,buf_len);
            break;

        case AVRC_OP_BROWSE: /* 0xff Browse commands */
          status = avrc_pars_browse_rsp(&p_msg->browse, p_result);
          break;

        default:
            AVRC_TRACE_ERROR("%s unknown opcode:0x%x", __func__, p_msg->hdr.opcode);
            break;
        }
        p_result->rsp.opcode = p_msg->hdr.opcode;
        p_result->rsp.status = status;
    }
    return status;
}
#endif /* (AVRC_CTRL_INCLUDED) == TRUE) */
/*******************************************************************************
**
** Function         AVRC_ParsResponse
**
** Description      This function is a superset of AVRC_ParsMetadata to parse the response.
**
** Returns          AVRC_STS_NO_ERROR, if the message in p_data is parsed successfully.
**                  Otherwise, the error code defined by AVRCP 1.4
**
*******************************************************************************/
tAVRC_STS AVRC_ParsResponse (tAVRC_MSG *p_msg, tAVRC_RESPONSE *p_result, UINT8 *p_buf, UINT16 buf_len)
{
    tAVRC_STS  status = AVRC_STS_INTERNAL_ERR;
    UINT16  id;
    UNUSED(p_buf);
    UNUSED(buf_len);

    if (p_msg && p_result)
    {
        switch (p_msg->hdr.opcode)
        {
        case AVRC_OP_VENDOR:     /*  0x00    Vendor-dependent commands */
            status = avrc_pars_vendor_rsp(&p_msg->vendor, p_result);
            break;

        case AVRC_OP_PASS_THRU:  /*  0x7C    panel subunit opcode */
            status = avrc_pars_pass_thru(&p_msg->pass, &id);
            if (status == AVRC_STS_NO_ERROR)
            {
                p_result->pdu = (UINT8)id;
            }
            break;

        default:
            AVRC_TRACE_ERROR("%s unknown opcode:0x%x", __func__, p_msg->hdr.opcode);
            break;
        }
        p_result->rsp.opcode = p_msg->hdr.opcode;
        p_result->rsp.status = status;
    }
    return status;
}
#endif /* (AVRC_METADATA_INCLUDED == TRUE) */
