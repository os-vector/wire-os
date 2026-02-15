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
   interface to aptX HD codec
 ******************************************************************************/
#ifndef A2D_APTX_HD_H
#define A2D_APTX_HD_H

/* aptX HD codec specific settings*/
#define A2D_APTX_HD_CODEC_LEN         13

#define A2D_APTX_HD_VENDOR_ID               (0x000000D7)
#define A2D_APTX_HD_CODEC_ID_BLUETOOTH      ((UINT16) 0x0024)
#define A2D_APTX_HD_SAMPLERATE_44100        (0x20)
#define A2D_APTX_HD_SAMPLERATE_48000        (0x10)
#define A2D_APTX_HD_CHANNELS_STEREO         (0x02)
#define A2D_APTX_HD_CHANNELS_MONO           (0x01)
#define A2D_APTX_HD_ACL_SPRINT_RESERVED0    (0x00)
#define A2D_APTX_HD_ACL_SPRINT_RESERVED1    (0x00)
#define A2D_APTX_HD_ACL_SPRINT_RESERVED2    (0x00)
#define A2D_APTX_HD_ACL_SPRINT_RESERVED3    (0x00)
#define A2D_APTX_HD_OTHER_FEATURES_NONE     (0x00000000)
#define A2D_APTX_HD_AV_AUDIO                (0x00)
#define A2D_APTX_HD_CODEC_ID                (0xff)
#define A2D_APTX_HD_CHANNEL                 (0x0001)
#define A2D_APTX_HD_SAMPLERATE              (0x22)

/*****************************************************************************
**  Type Definitions
*****************************************************************************/

typedef struct {
    UINT32 vendorId;
    UINT16 codecId;         /* Codec ID for aptX HD */
    UINT8  sampleRate;      /* Sampling Frequency */
    UINT8  channelMode;     /* STEREO/DUAL/MONO */
    UINT8 acl_sprint_reserved0;
    UINT8 acl_sprint_reserved1;
    UINT8 acl_sprint_reserved2;
    UINT8 acl_sprint_reserved3;
} tA2D_APTX_HD_CIE;

typedef struct {
    INT16 s16SamplingFreq;     /* 16k, 32k, 44.1k or 48k*/
    INT16 s16ChannelMode;      /* mono, dual, streo or joint streo*/
    UINT16 u16BitRate;
    UINT16 *ps16NextPcmBuffer;
    UINT8  *pu8Packet;
    UINT8  *pu8NextPacket;
    UINT16 u16PacketLength;
    void* encoder;
} A2D_APTX_HD_ENC_PARAMS;

extern BOOLEAN isA2dAptXHdEnabled;

/*****************************************************************************
**  external function declarations
*****************************************************************************/
#ifdef __cplusplus
extern "C"
{
#endif
extern UINT8 A2D_BldAptx_hdInfo(UINT8 media_type, tA2D_APTX_HD_CIE *p_ie,
                             UINT8 *p_result);
extern UINT8 A2D_ParsAptx_hdInfo(tA2D_APTX_HD_CIE *p_ie, UINT8 *p_info,
                              BOOLEAN for_caps);
extern int (*A2D_aptx_hd_encoder_init)(void);
extern void (*A2D_aptx_hd_encoder_deinit)(void);
extern UINT8 a2d_av_aptx_hd_cfg_in_cap(UINT8 *p_cfg, tA2D_APTX_HD_CIE *p_cap);
extern BOOLEAN A2D_check_and_init_aptX_HD();
extern void A2D_deinit_aptX_HD();

#ifdef __cplusplus
}
#endif

#endif /* A2D_APTX_HD_H */
