/******************************************************************************
 *
 *  Copyright (c) 2015,2017 The Linux Foundation. All rights reserved.
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

#ifndef A2D_AAC_H
#define A2D_AAC_H

/*****************************************************************************
**  Constants
*****************************************************************************/

#define A2D_AAC_MPL_HDR_LEN         1

/* AAC media codec capabilitiy len*/
#define A2D_AAC_INFO_LEN            8

/* AAC Codec Specific */
#define A2D_AAC_IE_OBJ_TYPE_MSK                0xF0    /* b7-b4 Object Type */
#define A2D_AAC_IE_OBJ_TYPE_MPEG_2_AAC_LC      0x80    /* b7:MPEG-2 AAC LC */
#define A2D_AAC_IE_OBJ_TYPE_MPEG_4_AAC_LC      0x40    /* b7:MPEG-4 AAC LC */
#define A2D_AAC_IE_OBJ_TYPE_MPEG_4_AAC_LTP     0x20    /* b7:MPEG-4 AAC LTP */
#define A2D_AAC_IE_OBJ_TYPE_MPEG_4_AAC_SCA     0x10    /* b7:MPEG-4 AAC SCALABLE */

#define A2D_AAC_IE_SAMP_FREQ_MSK               0xFFF0    /* b15-b4 sampling frequency */
#define A2D_AAC_IE_SAMP_FREQ_8000              0x8000    /* b15: 8000 */
#define A2D_AAC_IE_SAMP_FREQ_11025             0x4000    /* b15: 11025 */
#define A2D_AAC_IE_SAMP_FREQ_12000             0x2000    /* b15: 12000 */
#define A2D_AAC_IE_SAMP_FREQ_16000             0x1000    /* b15: 16000 */
#define A2D_AAC_IE_SAMP_FREQ_22050             0x0800    /* b15: 22050 */
#define A2D_AAC_IE_SAMP_FREQ_24000             0x0400    /* b15: 24000 */
#define A2D_AAC_IE_SAMP_FREQ_32000             0x0200    /* b15: 32000 */
#define A2D_AAC_IE_SAMP_FREQ_44100             0x0100    /* b15: 441000 */
#define A2D_AAC_IE_SAMP_FREQ_48000             0x0080    /* b15: 48000 */
#define A2D_AAC_IE_SAMP_FREQ_64000             0x0040    /* b15: 64000 */
#define A2D_AAC_IE_SAMP_FREQ_88200             0x0020    /* b15: 88200 */
#define A2D_AAC_IE_SAMP_FREQ_96000             0x0010    /* b15: 96000 */


#define A2D_AAC_IE_CHANNELS_MSK                0x0C    /* b7-b6 channels supported */
#define A2D_AAC_IE_CHANNELS_1                  0x08    /* Channel 1 */
#define A2D_AAC_IE_CHANNELS_2                  0x04    /* Channel 2 */

#define A2D_AAC_IE_VBR_MSK                     0x80    /* b7 variable bit rate */
#define A2D_AAC_IE_VBR_SUPP                    0x80    /* supported */
#define A2D_AAC_IE_VBR_NOT_SUPP                0x00    /* supported */

#define A2D_AAC_IE_BIT_RATE_MSK                0x007FFFFF  /* bit rate */
#define A2D_AAC_IE_BIT_RATE                    0x007FFFFF


#define BTIF_AAC_DEFAULT_BIT_RATE 0x000409B6

typedef struct  {
    INT16 s16SamplingFreq;  /* 16k, 32k, 44.1k or 48k*/
    INT16 s16ChannelMode;   /* mono, dual, streo or joint streo*/
    UINT16 u16BitRate;
    UINT16 *ps16NextPcmBuffer;
    UINT8  *pu8Packet;
    UINT8  *pu8NextPacket;
    UINT16 u16PacketLength;
    void* encoder;
} A2D_AAC_ENC_PARAMS;

/*****************************************************************************
**  Type Definitions
*****************************************************************************/

/* AAC Codec Information data type */
typedef struct
{
    UINT8   object_type;    /* Object Type */
    UINT16  samp_freq;      /* Sampling Frequency */
    UINT8   channels;       /* Channels */
    UINT32  bit_rate;       /* bit_rate */
    UINT8   vbr;            /* variable bit rate */
} tA2D_AAC_CIE;


/*****************************************************************************
**  External Function Declarations
*****************************************************************************/
#ifdef __cplusplus
extern "C"
{
#endif
/******************************************************************************
**
** Function         A2D_BldAacInfo
**
** Description      This function builds byte sequence for
**                  Aac Codec Capabilities.
** Input :           media_type:  Audio or MultiMedia.
**                  p_ie: AAC Codec Information Element
**
** Output :          p_result: codec info.
**
** Returns          A2D_SUCCESS if successful.
**                  Error otherwise.
******************************************************************************/
extern tA2D_STATUS A2D_BldAacInfo(UINT8 media_type, tA2D_AAC_CIE *p_ie, UINT8 *p_result);

/******************************************************************************
**
** Function         A2D_ParsAacInfo
**
** Description      This function parse byte sequence for
**                  Aac Codec Capabilities.
** Input :          p_info:  input byte sequence.
**                  for_caps: True for getcap, false otherwise
**
** Output :          p_ie: Aac codec information.
**
** Returns          A2D_SUCCESS if successful.
**                  Error otherwise.
******************************************************************************/
extern tA2D_STATUS A2D_ParsAacInfo(tA2D_AAC_CIE *p_ie, UINT8 *p_info, BOOLEAN for_caps);

#ifdef __cplusplus
}
#endif

#endif /* A2D_AAC_H */
