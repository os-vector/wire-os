/******************************************************************************
 *
 *  Copyright (c) 2017, The Linux Foundation. All rights reserved.
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

#ifndef A2D_MP3_H
#define A2D_MP3_H

/*****************************************************************************
**  Constants
*****************************************************************************/
/* MP3 media codec capabilitiy len*/
#define A2D_MP3_INFO_LEN            6

/* MP3 Codec Specific */
#define A2D_MP3_IE_LAYER_MSK                   0xE0    /* b7-b5 LAYER */
#define A2D_MP3_IE_LAYER_1                     0x80    /* b7:MP1 */
#define A2D_MP3_IE_LAYER_2                     0x40    /* b6:MP2 */
#define A2D_MP3_IE_LAYER_3                     0x20    /* b5:MP3 */

#define A2D_MP3_IE_CRC_MSK                     0x10    /* b4 CRC */
#define A2D_MP3_IE_CRC                         0x10    /* b4:SUPPORTED */

#define A2D_MP3_IE_CHANNELS_MSK                0x0F    /* b0-b3 channels supported */
#define A2D_MP3_IE_CHANNEL_MONO                0x08    /* Channel MONO */
#define A2D_MP3_IE_CHANNEL_DUAL                0x04    /* Channel DUAL */
#define A2D_MP3_IE_CHANNEL_STEREO              0x02    /* Channel STEREO */
#define A2D_MP3_IE_CHANNEL_JOINT_STEREO        0x01    /* Channel JOINT_STEREO */

#define A2D_MP3_IE_MPF_MSK                     0x40    /* b6 MPF */
#define A2D_MP3_IE_MPF_2                       0x40    /* b6:MPF2 supported, otwerwise 0 */

#define A2D_MP3_IE_SAMP_FREQ_MSK               0x3F    /* b5-b0 sampling frequency */
#define A2D_MP3_IE_SAMP_FREQ_16000             0x20    /* b5: 16000 */
#define A2D_MP3_IE_SAMP_FREQ_22050             0x10    /* b4: 22050 */
#define A2D_MP3_IE_SAMP_FREQ_24000             0x08    /* b3: 24000 */
#define A2D_MP3_IE_SAMP_FREQ_32000             0x04    /* b2: 32000 */
#define A2D_MP3_IE_SAMP_FREQ_44100             0x02    /* b1: 441000 */
#define A2D_MP3_IE_SAMP_FREQ_48000             0x01    /* b0: 48000 */

#define A2D_MP3_IE_VBR_MSK                     0x80    /* b7 variable bit rate */
#define A2D_MP3_IE_VBR                         0x80    /* supported */

#define A2D_MP3_IE_BIT_RATE_MSK                0x07FFF  /* bit rate */
#define A2D_MP3_IE_BIT_RATE                    0x07FFF

/*****************************************************************************
**  Type Definitions
*****************************************************************************/

/* MP3 Codec Information data type */
typedef struct
{
    UINT8   layer;          /* Layer */
    UINT8   crc;            /* CRC */
    UINT8   channels;       /* Channels */
    UINT8   mpf;            /* MPF */
    UINT8   samp_freq;      /* Sampling Frequency */
    UINT8   vbr;            /* variable bit rate */
    UINT16  bit_rate;       /* bit_rate */
} tA2D_MP3_CIE;


/*****************************************************************************
**  External Function Declarations
*****************************************************************************/
#ifdef __cplusplus
extern "C"
{
#endif
/******************************************************************************
**
** Function         A2D_BldMp3Info
**
** Description      This function builds byte sequence for
**                  MP3 Codec Capabilities.
** Input :           media_type:  Audio or MultiMedia.
**                  p_ie: MP3 Codec Information Element
**
** Output :          p_result: codec info.
**
** Returns          A2D_SUCCESS if successful.
**                  Error otherwise.
******************************************************************************/
extern tA2D_STATUS A2D_BldMp3Info(UINT8 media_type, tA2D_MP3_CIE *p_ie, UINT8 *p_result);

/******************************************************************************
**
** Function         A2D_ParsMp3Info
**
** Description      This function parse byte sequence for
**                  Mp3 Codec Capabilities.
** Input :          p_info:  input byte sequence.
**                  for_caps: True for getcap, false otherwise
**
** Output :          p_ie: Mp3 codec information.
**
** Returns          A2D_SUCCESS if successful.
**                  Error otherwise.
******************************************************************************/
extern tA2D_STATUS A2D_ParsMp3Info(tA2D_MP3_CIE *p_ie, UINT8 *p_info, BOOLEAN for_caps);

#ifdef __cplusplus
}
#endif

#endif /* A2D_MP3_H */
