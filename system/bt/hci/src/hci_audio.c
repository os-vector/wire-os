/******************************************************************************
 *
 *  Copyright (C) 2015 Google, Inc.
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

#define LOG_TAG "bt_hci_audio"

#include "hci/include/hci_audio.h"

#include "hci/include/bt_vendor_lib.h"
#include "hci/include/vendor.h"
#include "osi/include/log.h"

void set_audio_state(uint16_t handle, sco_codec_t codec, sco_state_t state)
{
    LOG_INFO(LOG_TAG, "%s handle:%d codec:0x%x state:%d", __func__, handle, codec, state);

    bt_vendor_op_audio_state_t audio_state;

    audio_state.handle = handle;
    audio_state.peer_codec = codec;
    audio_state.state = state;

    vendor_get_interface()->send_command(VENDOR_SET_AUDIO_STATE, &audio_state);
}
