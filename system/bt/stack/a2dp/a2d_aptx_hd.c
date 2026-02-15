/******************************************************************************

    Copyright (c) 2016, The Linux Foundation. All rights reserved.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions are
    met:
        * Redistributions of source code must retain the above copyright
          notice, this list of conditions and the following disclaimer.
        * Redistributions in binary form must reproduce the above
          copyright notice, this list of conditions and the following
          disclaimer in the documentation and/or other materials provided
          with the distribution.
        * Neither the name of The Linux Foundation nor the names of its
          contributors may be used to endorse or promote products derived
          from this software without specific prior written permission.

    THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
    WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
    MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
    ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
    BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
    CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
    SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
    BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
    WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
    OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
    IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

 ******************************************************************************/
/******************************************************************************

    Utility functions to help build and parse the aptX HD Codec Information
    Element and Media Payload.

******************************************************************************/

#include "bt_target.h"

#include <string.h>
#include <dlfcn.h>
#include "osi/include/thread.h"
#include "bt_utils.h"
#include "a2d_api.h"
#include "a2d_int.h"
#include "a2d_aptx.h"
#include "a2d_aptx_hd.h"
#include <utils/Log.h>


BOOLEAN isA2dAptXHdEnabled = FALSE;

int (*A2D_aptx_hd_encoder_init)(void);
void (*A2D_aptx_hd_encoder_deinit)(void);

/******************************************************************************
**
** Function         A2D_BldAptx_hdInfo
**
******************************************************************************/
UINT8 A2D_BldAptx_hdInfo(UINT8 media_type, tA2D_APTX_HD_CIE *p_ie, UINT8 *p_result)
{
    A2D_TRACE_API("%s: - MediaType:%d", __func__, media_type);

    UINT8 status = 0;
    status = A2D_SUCCESS;
    *p_result++ = A2D_APTX_HD_CODEC_LEN;
    *p_result++ = media_type;
    *p_result++ = A2D_NON_A2DP_MEDIA_CT;
    *p_result++ = (UINT8)(p_ie->vendorId & 0x000000FF);
    *p_result++ = (UINT8)(p_ie->vendorId & 0x0000FF00)>> 8;
    *p_result++ = (UINT8)(p_ie->vendorId & 0x00FF0000)>> 16;
    *p_result++ = (UINT8)(p_ie->vendorId & 0xFF000000)>> 24;
    *p_result++ = (UINT8)(p_ie->codecId & 0x00FF);
    *p_result++ = (UINT8)(p_ie->codecId & 0xFF00) >> 8;
    *p_result++ = p_ie->sampleRate | p_ie->channelMode;
    *p_result++ = p_ie->acl_sprint_reserved0;
    *p_result++ = p_ie->acl_sprint_reserved1;
    *p_result++ = p_ie->acl_sprint_reserved2;
    *p_result++ = p_ie->acl_sprint_reserved3;
    return status;
}

/******************************************************************************
**
** Function         A2D_ParsAptx_hdInfo
**
******************************************************************************/
tA2D_STATUS A2D_ParsAptx_hdInfo(tA2D_APTX_HD_CIE *p_ie, UINT8 *p_info, BOOLEAN for_caps)
{
    tA2D_STATUS status;
    UINT8 losc;
    UINT8 mt;

    A2D_TRACE_API("%s: - MediaType:%d", __func__, for_caps);

    if (p_ie == NULL || p_info == NULL)
    {
        A2D_TRACE_ERROR("A2D_ParsAptx_hdInfo - Invalid Params");
        status = A2D_INVALID_PARAMS;
    }
    else
    {
        losc    = *p_info++;
        mt      = *p_info++;
        A2D_TRACE_DEBUG("%s: losc %d, mt %02x", __func__, losc, mt);

        /* If the function is called for the wrong Media Type or Media Codec Type */
        if (losc != A2D_APTX_HD_CODEC_LEN || *p_info != A2D_NON_A2DP_MEDIA_CT) {
            A2D_TRACE_ERROR("%s: wrong media type %02x", __func__, *p_info);
            status = A2D_WRONG_CODEC;
        }
        else
        {
            p_info++;
            p_ie->vendorId = (*p_info & 0x000000FF) |
                             (*(p_info+1) << 8   & 0x0000FF00) |
                             (*(p_info+2) << 16  & 0x00FF0000) |
                             (*(p_info+3) << 24  & 0xFF000000);
            p_info = p_info+4;
            p_ie->codecId = (*p_info & 0x00FF) | (*(p_info+1) << 8 & 0xFF00);
            p_info = p_info+2;
            p_ie->channelMode= *p_info & 0x0F;
            p_ie->sampleRate = *p_info & 0xF0;
            p_info = p_info+1;
            p_ie->acl_sprint_reserved0 = *(p_info ++);
            p_ie->acl_sprint_reserved1 = *(p_info ++);
            p_ie->acl_sprint_reserved2 = *(p_info ++);
            p_ie->acl_sprint_reserved3 = *(p_info ++);

            status = A2D_SUCCESS;

            if (for_caps == FALSE)
            {
                if (A2D_BitsSet(p_ie->sampleRate) != A2D_SET_ONE_BIT)
                    status = A2D_BAD_SAMP_FREQ;
                if (A2D_BitsSet(p_ie->channelMode) != A2D_SET_ONE_BIT)
                    status = A2D_BAD_CH_MODE;
            }
        }
    }
    return status;
}

/*******************************************************************************
**
** Function         a2d_av_aptx_hd_cfg_in_cap
**
** Description      This function checks whether an aptX HD codec configuration
**                  is allowable for the given codec capabilities.
**
** Returns          0 if ok, nonzero if error.
**
*******************************************************************************/
UINT8 a2d_av_aptx_hd_cfg_in_cap(UINT8 *p_cfg, tA2D_APTX_HD_CIE *p_cap)
{
    UINT8 status = 0;
    tA2D_APTX_HD_CIE cfg_cie;

    A2D_TRACE_API("%s", __func__);

    /* parse configuration */
    if ((status = A2D_ParsAptx_hdInfo(&cfg_cie, p_cfg, FALSE)) != 0)
    {
        A2D_TRACE_ERROR("%s: aptX HD parse failed", __func__);
        return status;
    }

    /* verify that each parameter is in range */

    /* sampling frequency */
    if ((cfg_cie.sampleRate & p_cap->sampleRate) == 0)
        status = A2D_NS_SAMP_FREQ;
    /* channel mode */
    else if ((cfg_cie.channelMode & p_cap->channelMode) == 0)
        status = A2D_NS_CH_MODE;

    return status;
}

/*******************************************************************************
**
** Function         A2D_check_and_init_aptX_HD
**
** Description      This function checks if all the libraries required for
**                  aptX HD are present and needed function pointers are resolved
**
** Returns          returns true if aptX HD codec initialization succeeds
**
*******************************************************************************/
BOOLEAN A2D_check_and_init_aptX_HD(void)
{
    A2D_TRACE_DEBUG("%s", __func__);

    if (isA2dAptXEnabled && A2dAptXSchedLibHandle)
    {
        A2D_aptx_hd_encoder_init = (int (*)(void))dlsym(A2dAptXSchedLibHandle,
                                                   "aptx_hd_encoder_init");
        if (!A2D_aptx_hd_encoder_init)
        {
            A2D_TRACE_ERROR("%s: aptX HD encoder init missing", __func__);
            goto error_exit;
        }

        A2D_aptx_hd_encoder_deinit = (void (*)(void))dlsym(A2dAptXSchedLibHandle,
                                                      "aptx_hd_encoder_deinit");
        if (!A2D_aptx_hd_encoder_deinit)
        {
            A2D_TRACE_ERROR("%s: aptX HD encoder deinit missing", __func__);
            goto error_exit;
        }

        if (A2D_aptx_hd_encoder_init())
        {
            A2D_TRACE_ERROR("%s: aptX HD encoder init failed - %s", __func__, dlerror());
            goto error_exit;
        }
    } else {
        A2D_TRACE_ERROR("%s: isA2dAptXEnabled = false", __func__);
        goto error_exit;
    }
    isA2dAptXHdEnabled = true;
    return isA2dAptXHdEnabled;

 error_exit:;
    isA2dAptXHdEnabled = false;
    return isA2dAptXHdEnabled;

}

/*******************************************************************************
**
** Function         A2D_deinit_aptX_HD
**
** Description      This function de-initialized aptX HD
**
** Returns          Nothing
**
*******************************************************************************/
void A2D_deinit_aptX_HD(void)
{
    A2D_TRACE_DEBUG("%s", __func__);

    if (isA2dAptXHdEnabled && isA2dAptXEnabled && A2dAptXSchedLibHandle)
    {
       A2D_aptx_hd_encoder_deinit();
       isA2dAptXHdEnabled = false;
    }

    return;
}
