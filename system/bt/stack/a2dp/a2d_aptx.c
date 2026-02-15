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
 *
 *  Utility functions to help build and parse the aptX Codec Information
 *  Element and Media Payload.
 *
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

const char* A2D_APTX_SCHED_LIB_NAME = "libaptXScheduler.so";
void *A2dAptXSchedLibHandle = NULL;
thread_t *A2d_aptx_thread = NULL;
pthread_mutex_t aptx_thread_lock = PTHREAD_MUTEX_INITIALIZER;
BOOLEAN isA2dAptXEnabled = FALSE;

int (*A2D_aptx_encoder_init)(void);
A2D_AptXThreadFn (*A2D_aptx_sched_start)(void *encoder,
                        A2D_AptXCodecType aptX_codec_type,
                        BOOLEAN use_SCMS_T, BOOLEAN is_24bit_audio,
                        UINT16 sample_rate, UINT8 format_bits,
                        UINT8 channel, UINT16 MTU, A2D_AptXReadFn read_fn,
                        A2D_AptXBufferSendFn send_fn,
                        A2D_AptXSetPriorityFn set_priority_fn,
                        BOOLEAN test, BOOLEAN trace);
BOOLEAN (*A2D_aptx_sched_stop)(void);
void (*A2D_aptx_encoder_deinit)(void);
A2D_AptXThreadFn A2d_aptx_thread_fn;

/******************************************************************************
**
** Function         A2D_BldAptxInfo
**
******************************************************************************/
UINT8 A2D_BldAptxInfo(UINT8 media_type, tA2D_APTX_CIE *p_ie, UINT8 *p_result)
{
    A2D_TRACE_API("%s: - MediaType:%d", __func__, media_type);

    UINT8 status = 0;
    status = A2D_SUCCESS;
    *p_result++ = A2D_APTX_CODEC_LEN;
    *p_result++ = media_type;
    *p_result++ = A2D_NON_A2DP_MEDIA_CT;
    *p_result++ = (UINT8)(p_ie->vendorId & 0x000000FF);
    *p_result++ = (UINT8)(p_ie->vendorId & 0x0000FF00)>> 8;
    *p_result++ = (UINT8)(p_ie->vendorId & 0x00FF0000)>> 16;
    *p_result++ = (UINT8)(p_ie->vendorId & 0xFF000000)>> 24;
    *p_result++ = (UINT8)(p_ie->codecId & 0x00FF);
    *p_result++ = (UINT8)(p_ie->codecId & 0xFF00) >> 8;
    *p_result++ = p_ie->sampleRate | p_ie->channelMode;

    return status;
}

/******************************************************************************
**
** Function         A2D_ParsAptxInfo
**
******************************************************************************/
tA2D_STATUS A2D_ParsAptxInfo(tA2D_APTX_CIE *p_ie, UINT8 *p_info, BOOLEAN for_caps)
{
    tA2D_STATUS status;
    UINT8   losc;
    UINT8   mt;

    A2D_TRACE_API("%s: - MediaType:%d", __func__, for_caps);

    if (p_ie == NULL || p_info == NULL)
    {
        A2D_TRACE_ERROR("A2D_ParsAptxInfo - Invalid Params");
        status = A2D_INVALID_PARAMS;
    }
    else
    {
        losc    = *p_info++;
        mt      = *p_info++;
        A2D_TRACE_DEBUG("%s: losc %d, mt %02x", __func__, losc, mt);

        /* If the function is called for the wrong Media Type or Media Codec Type */
        if (losc != A2D_APTX_CODEC_LEN || *p_info != A2D_NON_A2DP_MEDIA_CT) {
            A2D_TRACE_ERROR("%s: wrong media type %02x", __func__, *p_info);
            status = A2D_WRONG_CODEC;
        }
        else
        {
            p_info++;
            p_ie->vendorId = (*p_info & 0x000000FF) |
                             (*(p_info+1) << 8    & 0x0000FF00) |
                             (*(p_info+2) << 16  & 0x00FF0000) |
                             (*(p_info+3) << 24  & 0xFF000000);
            p_info = p_info+4;
            p_ie->codecId = (*p_info & 0x00FF) |(*(p_info+1) << 8 & 0xFF00);
            p_info = p_info+2;
            p_ie->channelMode= *p_info & 0x0F;
            p_ie->sampleRate = *p_info & 0xF0;

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
** Function         a2d_av_aptx_cfg_in_cap
**
** Description      This function checks whether an aptX codec configuration
**                  is allowable for the given codec capabilities.
**
** Returns          0 if ok, nonzero if error.
**
*******************************************************************************/
UINT8 a2d_av_aptx_cfg_in_cap(UINT8 *p_cfg, tA2D_APTX_CIE *p_cap)
{
    UINT8           status = 0;
    tA2D_APTX_CIE   cfg_cie;

    A2D_TRACE_API("%s", __func__);

    /* parse configuration */
    if ((status = A2D_ParsAptxInfo(&cfg_cie, p_cfg, FALSE)) != 0)
    {
        A2D_TRACE_ERROR("%s:, aptx parse failed", __func__);
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
** Function         A2D_check_and_init_aptX
**
** Description      This function checks if all the libraries required for
**                  aptX are present and needed function pointers are resolved
**
** Returns          returns true if aptX codec initialization succeeds
**
*******************************************************************************/
BOOLEAN A2D_check_and_init_aptX(void)
{
    A2D_TRACE_DEBUG("%s", __func__);

    if (A2dAptXSchedLibHandle == NULL)
    {
        A2dAptXSchedLibHandle = dlopen(A2D_APTX_SCHED_LIB_NAME, RTLD_NOW);

        if (!A2dAptXSchedLibHandle)
        {
            A2D_TRACE_ERROR("%s: aptX scheduler library missing", __func__);
            goto error_exit;
        }

        A2D_aptx_encoder_init = (int (*)(void))dlsym(A2dAptXSchedLibHandle,
                                                   "aptx_encoder_init");
        if (!A2D_aptx_encoder_init)
        {
            A2D_TRACE_ERROR("%s: aptX encoder init missing", __func__);
            goto error_exit;
        }

        A2D_aptx_sched_start = (A2D_AptXThreadFn (*)(void*, A2D_AptXCodecType, BOOLEAN,
                                        BOOLEAN, UINT16, UINT8, UINT8, UINT16, A2D_AptXReadFn,
                                        A2D_AptXBufferSendFn,
                                        A2D_AptXSetPriorityFn, BOOLEAN,
                                        BOOLEAN))dlsym(A2dAptXSchedLibHandle,
                                        "aptx_scheduler_start");

        if (!A2D_aptx_sched_start)
        {
            A2D_TRACE_ERROR("%s: aptX scheduler start missing", __func__);
            goto error_exit;
        }

        A2D_aptx_sched_stop = (BOOLEAN (*)(void))dlsym(A2dAptXSchedLibHandle,
                                                       "aptx_scheduler_stop");
        if (!A2D_aptx_sched_stop)
        {
            A2D_TRACE_ERROR("%s: aptX scheduler stop missing", __func__);
            goto error_exit;
        }

        A2D_aptx_encoder_deinit = (void (*)(void))dlsym(A2dAptXSchedLibHandle,
                                                      "aptx_encoder_deinit");
        if (!A2D_aptx_encoder_deinit)
        {
            A2D_TRACE_ERROR("%s: aptX encoder deinit missing ", __func__);
            goto error_exit;
        }

        if (A2D_aptx_encoder_init())
        {
            A2D_TRACE_ERROR("%s: aptX encoder init failed - %s", __func__, dlerror());
            goto error_exit;
        }
    }
    isA2dAptXEnabled = true;
    return isA2dAptXEnabled;

 error_exit:;
    if (A2dAptXSchedLibHandle)
    {
       dlclose(A2dAptXSchedLibHandle);
       A2dAptXSchedLibHandle = NULL;
    }
    isA2dAptXEnabled = false;
    return isA2dAptXEnabled;

}

/*******************************************************************************
**
** Function         A2D_deinit_aptX
**
** Description      This function de-initialized aptX
**
** Returns          Nothing
**
*******************************************************************************/
void A2D_deinit_aptX(void)
{
    A2D_TRACE_DEBUG("%s", __func__);

    if (isA2dAptXEnabled && A2dAptXSchedLibHandle)
    {
        A2D_aptx_encoder_deinit();
        isA2dAptXEnabled = false;
    }

    return;
}

/*******************************************************************************
**
** Function         A2D_stop_aptX
**
** Description      This function remove aptX thread
**
** Returns          Nothing
**
*******************************************************************************/
void A2D_stop_aptX(void)
{
    A2D_TRACE_DEBUG("%s", __func__);
    pthread_mutex_lock(&aptx_thread_lock);
    if (A2dAptXSchedLibHandle)
    {
        // remove aptX thread
        if (A2d_aptx_thread)
        {
            A2D_aptx_sched_stop();
            thread_free(A2d_aptx_thread);
            A2d_aptx_thread = NULL;
        }
    }
    pthread_mutex_unlock(&aptx_thread_lock);
    return;
}

/*******************************************************************************
**
** Function         A2D_close_aptX
**
** Description      This function close aptX
**
** Returns          Nothing
**
*******************************************************************************/
void A2D_close_aptX(void)
{
    A2D_TRACE_DEBUG("%s", __func__);

    pthread_mutex_lock(&aptx_thread_lock);
    if (A2dAptXSchedLibHandle)
    {
        // remove aptX thread
        if (A2d_aptx_thread)
        {
            A2D_aptx_sched_stop();
            thread_free(A2d_aptx_thread);
            A2d_aptx_thread = NULL;
        }
    }
    pthread_mutex_unlock(&aptx_thread_lock);

    // de-initialize aptX HD
    A2D_deinit_aptX_HD();

    // de-initialize aptX
    A2D_deinit_aptX();

    if (A2dAptXSchedLibHandle)
    {
       dlclose(A2dAptXSchedLibHandle);
       A2dAptXSchedLibHandle = NULL;
    }

    return;
}
