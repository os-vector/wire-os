/******************************************************************************
 *
 *  Copyright (c) 2016, The Linux Foundation. All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are
 *   met:
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above
 *     copyright notice, this list of conditions and the following
 *     disclaimer in the documentation and/or other materials provided
 *     with the distribution.
 *   * Neither the name of The Linux Foundation nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
  ******************************************************************************/

#include "a2d_api.h"
#include "a2d_aac.h"
#include "bta_av_aac.h"
#include "utl.h"

/*******************************************************************************
**
** Function         bta_av_aac_cfg_in_cap
**
** Description      Checks if AAC codec config is allowed for given caps.
**                  codec config bits are also checked for sanity
**
** Returns          0 if ok, nonzero if error.
**
*******************************************************************************/
UINT8 bta_av_aac_cfg_in_cap(UINT8 *p_cfg, tA2D_AAC_CIE *p_cap)
{
    UINT8           status = 0;
    tA2D_AAC_CIE    cfg_cie;

    /* parse configuration */
    if ((status = A2D_ParsAacInfo(&cfg_cie, p_cfg, FALSE)) != 0)
    {
        APPL_TRACE_DEBUG("bta_av_aac_cfg_in_cap can't parse  %d", status);
        return status;
    }


    /* object type */
    if ((cfg_cie.object_type & p_cap->object_type) == 0)
    {
        status = A2D_NS_OBJ_TYPE;
    }
    /* frequency */
    else if ((cfg_cie.samp_freq & p_cap->samp_freq) == 0)
    {
        status = A2D_NS_SAMP_FREQ;
    }
    /* channels */
    else if ((cfg_cie.channels & p_cap->channels) == 0)
    {
        status = A2D_NS_CHANNEL;
    }
    /* block length */
    else if ((cfg_cie.bit_rate & p_cap->bit_rate) == 0)
    {
        status = A2D_NS_BIT_RATE;
    }
    APPL_TRACE_DEBUG("bta_av_aac_cfg_in_cap return %d", status);
    return status;
}

