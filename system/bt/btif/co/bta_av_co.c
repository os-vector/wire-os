/******************************************************************************
 *  Copyright (c) 2016, The Linux Foundation. All rights reserved.
 *
 *  Not a contribution.
 ******************************************************************************/
/******************************************************************************
 *
 *  Copyright (C) 2004-2012 Broadcom Corporation
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

/******************************************************************************
 *
 *  This is the advanced audio/video call-out function implementation for
 *  BTIF.
 *
 ******************************************************************************/

#include "string.h"
#include "a2d_api.h"
#include "a2d_sbc.h"
#include "bta_sys.h"
#include "bta_av_api.h"
#include "bta_av_co.h"
#include "bta_av_ci.h"
#include "bta_av_sbc.h"

#include "btif_media.h"
#include "sbc_encoder.h"
#include "btif_av.h"
#include "btif_av_co.h"
#include "btif_util.h"
#include "osi/include/mutex.h"
#include "device/include/interop.h"

#include "bt_utils.h"
#include "a2d_aptx.h"
#include "a2d_aptx_hd.h"
#if defined(AAC_ENCODER_INCLUDED) && (AAC_ENCODER_INCLUDED == TRUE)
#include "a2d_aac.h"
#include "bta_av_aac.h"
#endif

/*****************************************************************************
 **  Constants
 *****************************************************************************/

#define FUNC_TRACE()     APPL_TRACE_DEBUG("%s", __FUNCTION__);

/* Macro to retrieve the number of elements in a statically allocated array */
#define BTA_AV_CO_NUM_ELEMENTS(__a) (sizeof(__a)/sizeof((__a)[0]))

/* MIN and MAX macros */
#define BTA_AV_CO_MIN(X,Y) ((X) < (Y) ? (X) : (Y))
#define BTA_AV_CO_MAX(X,Y) ((X) > (Y) ? (X) : (Y))

/* Macro to convert audio handle to index and vice versa */
#define BTA_AV_CO_AUDIO_HNDL_TO_INDX(hndl) (((hndl) & (~BTA_AV_CHNL_MSK)) - 1)
#define BTA_AV_CO_AUDIO_INDX_TO_HNDL(indx) (((indx) + 1) | BTA_AV_CHNL_AUDIO)


/* Offsets to access codec information in SBC codec */
#define BTA_AV_CO_SBC_FREQ_CHAN_OFF    3
#define BTA_AV_CO_SBC_BLOCK_BAND_OFF   4
#define BTA_AV_CO_SBC_MIN_BITPOOL_OFF  5
#define BTA_AV_CO_SBC_MAX_BITPOOL_OFF  6

#ifdef BTA_AV_SPLIT_A2DP_DEF_FREQ_48KHZ
#define BTA_AV_CO_SBC_MAX_BITPOOL  51
#else
#define BTA_AV_CO_SBC_MAX_BITPOOL  53
#endif

/* SCMS-T protect info */
const UINT8 bta_av_co_cp_scmst[BTA_AV_CP_INFO_LEN] = "\x02\x02\x00";

/* codec type preferance, put corresponding codec id here */
UINT8 codec_type_pref[BTIF_SV_AV_AA_SRC_SEP_INDEX] = {
    A2D_NON_A2DP_MEDIA_CT,
    BTIF_AV_CODEC_SBC,
};

/* SBC SRC codec capabilities */
const tA2D_SBC_CIE bta_av_co_sbc_caps =
{
#ifdef BTA_AV_SPLIT_A2DP_DEF_FREQ_48KHZ
    (A2D_SBC_IE_SAMP_FREQ_48), /* samp_freq */
#else
    (A2D_SBC_IE_SAMP_FREQ_44), /* samp_freq */
#endif
    (A2D_SBC_IE_CH_MD_MONO | A2D_SBC_IE_CH_MD_JOINT), /* ch_mode */
    (A2D_SBC_IE_BLOCKS_16 | A2D_SBC_IE_BLOCKS_12 | A2D_SBC_IE_BLOCKS_8 | A2D_SBC_IE_BLOCKS_4), /* block_len */
    (A2D_SBC_IE_SUBBAND_8), /* num_subbands */
    (A2D_SBC_IE_ALLOC_MD_L), /* alloc_mthd */
    BTA_AV_CO_SBC_MAX_BITPOOL, /* max_bitpool */
    A2D_SBC_IE_MIN_BITPOOL /* min_bitpool */
};

/* SBC SINK codec capabilities */
const tA2D_SBC_CIE bta_av_co_sbc_sink_caps =
{
    (A2D_SBC_IE_SAMP_FREQ_48 | A2D_SBC_IE_SAMP_FREQ_44), /* samp_freq */
    (A2D_SBC_IE_CH_MD_MONO | A2D_SBC_IE_CH_MD_STEREO | A2D_SBC_IE_CH_MD_JOINT | A2D_SBC_IE_CH_MD_DUAL), /* ch_mode */
    (A2D_SBC_IE_BLOCKS_16 | A2D_SBC_IE_BLOCKS_12 | A2D_SBC_IE_BLOCKS_8 | A2D_SBC_IE_BLOCKS_4), /* block_len */
    (A2D_SBC_IE_SUBBAND_4 | A2D_SBC_IE_SUBBAND_8), /* num_subbands */
    (A2D_SBC_IE_ALLOC_MD_L | A2D_SBC_IE_ALLOC_MD_S), /* alloc_mthd */
    A2D_SBC_IE_MAX_BITPOOL, /* max_bitpool */
    A2D_SBC_IE_MIN_BITPOOL /* min_bitpool */
};

#if !defined(BTIF_AV_SBC_DEFAULT_SAMP_FREQ)
#ifdef BTA_AV_SPLIT_A2DP_DEF_FREQ_48KHZ
#define BTIF_AV_SBC_DEFAULT_SAMP_FREQ A2D_SBC_IE_SAMP_FREQ_48
#else
#define BTIF_AV_SBC_DEFAULT_SAMP_FREQ A2D_SBC_IE_SAMP_FREQ_44
#endif
#endif

/* A2dp offload capabilities */
#define SBC    0
#define APTX   1
#define AAC    2
#define APTXHD 3
/* Default SBC codec configuration */
const tA2D_SBC_CIE btif_av_sbc_default_config =
{
    BTIF_AV_SBC_DEFAULT_SAMP_FREQ,   /* samp_freq */
    A2D_SBC_IE_CH_MD_JOINT,         /* ch_mode */
    A2D_SBC_IE_BLOCKS_16,           /* block_len */
    A2D_SBC_IE_SUBBAND_8,           /* num_subbands */
    A2D_SBC_IE_ALLOC_MD_L,          /* alloc_mthd */
    BTA_AV_CO_SBC_MAX_BITPOOL,      /* max_bitpool */
    A2D_SBC_IE_MIN_BITPOOL          /* min_bitpool */
};

const tA2D_APTX_CIE bta_av_co_aptx_caps =
{
    A2D_APTX_VENDOR_ID,
    A2D_APTX_CODEC_ID_BLUETOOTH,
#ifndef BTA_AV_SPLIT_A2DP_DEF_FREQ_48KHZ
    A2D_APTX_SAMPLERATE_44100,
#else
    A2D_APTX_SAMPLERATE_48000,
#endif
    A2D_APTX_CHANNELS_STEREO,
    A2D_APTX_FUTURE_1,
    A2D_APTX_FUTURE_2
};

/* Default aptX codec configuration */
const tA2D_APTX_CIE btif_av_aptx_default_config =
{
    A2D_APTX_VENDOR_ID,
    A2D_APTX_CODEC_ID_BLUETOOTH,
#ifndef BTA_AV_SPLIT_A2DP_DEF_FREQ_48KHZ
    A2D_APTX_SAMPLERATE_44100,
#else
    A2D_APTX_SAMPLERATE_48000,
#endif
    A2D_APTX_CHANNELS_STEREO,
    A2D_APTX_FUTURE_1,
    A2D_APTX_FUTURE_2
};

const tA2D_APTX_HD_CIE bta_av_co_aptx_hd_caps =
{
    A2D_APTX_HD_VENDOR_ID,
    A2D_APTX_HD_CODEC_ID_BLUETOOTH,
#ifndef BTA_AV_SPLIT_A2DP_DEF_FREQ_48KHZ
    A2D_APTX_HD_SAMPLERATE_44100,
#else
    A2D_APTX_HD_SAMPLERATE_48000,
#endif
    A2D_APTX_HD_CHANNELS_STEREO,
    A2D_APTX_HD_ACL_SPRINT_RESERVED0,
    A2D_APTX_HD_ACL_SPRINT_RESERVED1,
    A2D_APTX_HD_ACL_SPRINT_RESERVED2,
    A2D_APTX_HD_ACL_SPRINT_RESERVED3
};

/* Default aptX_hd codec configuration */
const tA2D_APTX_HD_CIE btif_av_aptx_hd_default_config =
{
    A2D_APTX_HD_VENDOR_ID,
    A2D_APTX_HD_CODEC_ID_BLUETOOTH,
#ifndef BTA_AV_SPLIT_A2DP_DEF_FREQ_48KHZ
    A2D_APTX_HD_SAMPLERATE_44100,
#else
    A2D_APTX_HD_SAMPLERATE_48000,
#endif
    A2D_APTX_HD_CHANNELS_STEREO,
    A2D_APTX_HD_ACL_SPRINT_RESERVED0,
    A2D_APTX_HD_ACL_SPRINT_RESERVED1,
    A2D_APTX_HD_ACL_SPRINT_RESERVED2,
    A2D_APTX_HD_ACL_SPRINT_RESERVED3
};

#if defined(AAC_ENCODER_INCLUDED) && (AAC_ENCODER_INCLUDED == TRUE)
const tA2D_AAC_CIE bta_av_co_aac_caps =
{
    (A2D_AAC_IE_OBJ_TYPE_MPEG_2_AAC_LC|A2D_AAC_IE_OBJ_TYPE_MPEG_4_AAC_LC), /* obj type */
#ifndef BTA_AV_SPLIT_A2DP_DEF_FREQ_48KHZ
    (A2D_AAC_IE_SAMP_FREQ_44100),
#else
    (A2D_AAC_IE_SAMP_FREQ_44100 | A2D_AAC_IE_SAMP_FREQ_48000),
#endif
    (A2D_AAC_IE_CHANNELS_1 | A2D_AAC_IE_CHANNELS_2 ), /* channels  */
    A2D_AAC_IE_BIT_RATE, /* BIT RATE */
    A2D_AAC_IE_VBR_NOT_SUPP  /* variable bit rate */
};

/* Default AAC codec configuration */
const tA2D_AAC_CIE btif_av_aac_default_config =
{
    A2D_AAC_IE_OBJ_TYPE_MPEG_2_AAC_LC,  /* obj type */
#ifndef BTA_AV_SPLIT_A2DP_DEF_FREQ_48KHZ
    A2D_AAC_IE_SAMP_FREQ_44100,         /* samp_freq */
#else
    A2D_AAC_IE_SAMP_FREQ_48000,         /* samp_freq */
#endif
    A2D_AAC_IE_CHANNELS_2,              /* channels  */
    BTIF_AAC_DEFAULT_BIT_RATE,      /* bit rate */
    A2D_AAC_IE_VBR_NOT_SUPP
};
#endif

tBTA_AV_CO_CODEC_CAP_LIST *p_bta_av_codec_pri_list = NULL;
tBTA_AV_CO_CODEC_CAP_LIST bta_av_supp_codec_cap[BTIF_SV_AV_AA_SRC_SEP_INDEX];
UINT8 bta_av_num_codec_configs;
extern pthread_mutex_t src_codec_q_lock;
/*****************************************************************************
**  Local data
*****************************************************************************/
typedef struct
{
    UINT8 sep_info_idx;                 /* local SEP index (in BTA tables) */
    UINT8 seid;                         /* peer SEP index (in peer tables) */
    UINT8 codec_type;                   /* peer SEP codec type */
    UINT8 codec_caps[AVDT_CODEC_SIZE];  /* peer SEP codec capabilities */
    UINT8 num_protect;                  /* peer SEP number of CP elements */
    UINT8 protect_info[BTA_AV_CP_INFO_LEN];  /* peer SEP content protection info */
} tBTA_AV_CO_SINK;

typedef struct
{
    BD_ADDR         addr;               /* address of audio/video peer */
    /* array of supported sinks */
    tBTA_AV_CO_SINK snks[BTIF_SV_AV_AA_SRC_SEP_INDEX - BTIF_SV_AV_AA_SBC_INDEX];
    /* array of supported srcs */
    tBTA_AV_CO_SINK srcs[BTIF_SV_AV_AA_SNK_SEP_INDEX - BTIF_SV_AV_AA_SBC_SINK_INDEX];
    UINT8           num_snks;           /* total number of sinks at peer */
    UINT8           num_srcs;           /* total number of srcs at peer */
    UINT8           num_seps;           /* total number of seids at peer */
    UINT8           num_rx_snks;        /* number of received sinks */
    UINT8           num_rx_srcs;        /* number of received srcs */
    UINT8           num_sup_snks;       /* number of supported sinks in the snks array */
    UINT8           num_sup_srcs;       /* number of supported srcs in the srcs array */
    tBTA_AV_CO_SINK *p_snk;             /* currently selected sink */
    tBTA_AV_CO_SINK *p_src;             /* currently selected src */
    UINT8           codec_cfg[AVDT_CODEC_SIZE]; /* current codec configuration */
    BOOLEAN         cp_active;          /* current CP configuration */
    BOOLEAN         acp;                /* acceptor */
    BOOLEAN         recfg_needed;       /* reconfiguration is needed */
    BOOLEAN         opened;             /* opened */
    UINT16          mtu;                /* maximum transmit unit size */
    UINT16          uuid_to_connect;    /* uuid of peer device */
} tBTA_AV_CO_PEER;

typedef struct
{
    BOOLEAN active;
    UINT8 flag;
} tBTA_AV_CO_CP;

typedef struct
{
    /* Connected peer information */
    tBTA_AV_CO_PEER peers[BTA_AV_NUM_STRS];
    /* Current codec configuration - access to this variable must be protected */
    tBTIF_AV_CODEC_INFO* codec_cfg;
    tBTIF_AV_CODEC_INFO* codec_cfg_setconfig; /* remote peer setconfig preference */
    UINT8 current_codec_id;
    tBTIF_AV_CODEC_INFO codec_cfg_sbc;
    tBTIF_AV_CODEC_INFO codec_cfg_sbc_setconfig; /* remote peer setconfig preference (SBC) */
    tBTIF_AV_CODEC_INFO codec_cfg_aptx;
    tBTIF_AV_CODEC_INFO codec_cfg_aptx_setconfig; /* remote peer setconfig preference (aptX)*/
    tBTIF_AV_CODEC_INFO codec_cfg_aptx_hd;
    tBTIF_AV_CODEC_INFO codec_cfg_aptx_hd_setconfig; /* remote peer setconfig preference (aptX HD) */
#if defined(AAC_ENCODER_INCLUDED) && (AAC_ENCODER_INCLUDED == TRUE)
    tBTIF_AV_CODEC_INFO codec_cfg_aac;
    tBTIF_AV_CODEC_INFO codec_cfg_aac_setconfig; /* remote peer setconfig preference (AAC)*/
#endif
    tBTA_AV_CO_CP cp;
} tBTA_AV_CO_CB;

/* Control block instance */
static tBTA_AV_CO_CB bta_av_co_cb;

static BOOLEAN bta_av_co_audio_codec_build_config(const UINT8 *p_codec_caps, UINT8 *p_codec_cfg);
static void bta_av_co_audio_peer_reset_config(tBTA_AV_CO_PEER *p_peer);
static BOOLEAN bta_av_co_cp_is_scmst(const UINT8 *p_protectinfo);
static BOOLEAN bta_av_co_audio_sink_has_scmst(const tBTA_AV_CO_SINK *p_sink);
static BOOLEAN bta_av_co_audio_peer_supports_codec(tBTA_AV_CO_PEER *p_peer, UINT8 *p_snk_index, UINT8 *p_codec_type);
static BOOLEAN bta_av_co_audio_media_supports_config(UINT8 codec_type, const UINT8 *p_codec_cfg);
static BOOLEAN bta_av_co_audio_sink_supports_config(UINT8 codec_type, const UINT8 *p_codec_cfg);
static BOOLEAN bta_av_co_audio_peer_src_supports_codec(tBTA_AV_CO_PEER *p_peer, UINT8 *p_src_index);
#ifdef BTA_AV_SPLIT_A2DP_ENABLED
extern BOOLEAN btif_av_is_codec_offload_supported(int codec);
#else
#define btif_av_is_codec_offload_supported(codec) (0)
#endif
extern BOOLEAN btif_av_is_offload_supported();
extern BOOLEAN bt_split_a2dp_enabled;
/*******************************************************************************
 **
 ** Function         bta_av_co_cp_is_active
 **
 ** Description      Get the current configuration of content protection
 **
 ** Returns          TRUE if the current streaming has CP, FALSE otherwise
 **
 *******************************************************************************/
BOOLEAN bta_av_co_cp_is_active(void)
{
    FUNC_TRACE();
    return bta_av_co_cb.cp.active;
}

/*******************************************************************************
 **
 ** Function         bta_av_co_cp_get_flag
 **
 ** Description      Get content protection flag
 **                  BTA_AV_CP_SCMS_COPY_NEVER
 **                  BTA_AV_CP_SCMS_COPY_ONCE
 **                  BTA_AV_CP_SCMS_COPY_FREE
 **
 ** Returns          The current flag value
 **
 *******************************************************************************/
UINT8 bta_av_co_cp_get_flag(void)
{
    FUNC_TRACE();
    return bta_av_co_cb.cp.flag;
}

/*******************************************************************************
 **
 ** Function         bta_av_co_cp_set_flag
 **
 ** Description      Set content protection flag
 **                  BTA_AV_CP_SCMS_COPY_NEVER
 **                  BTA_AV_CP_SCMS_COPY_ONCE
 **                  BTA_AV_CP_SCMS_COPY_FREE
 **
 ** Returns          TRUE if setting the SCMS flag is supported else FALSE
 **
 *******************************************************************************/
BOOLEAN bta_av_co_cp_set_flag(UINT8 cp_flag)
{
    FUNC_TRACE();

#if defined(BTA_AV_CO_CP_SCMS_T) && (BTA_AV_CO_CP_SCMS_T == TRUE)
#else
    if (cp_flag != BTA_AV_CP_SCMS_COPY_FREE)
    {
        return FALSE;
    }
#endif
    bta_av_co_cb.cp.flag = cp_flag;
    return TRUE;
}

/*******************************************************************************
 **
 ** Function         bta_av_co_get_peer
 **
 ** Description      find the peer entry for a given handle
 **
 ** Returns          the control block
 **
 *******************************************************************************/
static tBTA_AV_CO_PEER *bta_av_co_get_peer(tBTA_AV_HNDL hndl)
{
    UINT8 index;
    FUNC_TRACE();

    index = BTA_AV_CO_AUDIO_HNDL_TO_INDX(hndl);

    /* Sanity check */
    if (index >= BTA_AV_CO_NUM_ELEMENTS(bta_av_co_cb.peers))
    {
        APPL_TRACE_ERROR("bta_av_co_get_peer peer index out of bounds:%d", index);
        return NULL;
    }

    return &bta_av_co_cb.peers[index];
}

/*******************************************************************************
 **
 ** Function         bta_av_co_audio_init
 **
 ** Description      This callout function is executed by AV when it is
 **                  started by calling BTA_AvRegister().  This function can be
 **                  used by the phone to initialize audio paths or for other
 **                  initialization purposes.
 **
 **
 ** Returns          Stream codec and content protection capabilities info.
 **
 *******************************************************************************/
BOOLEAN bta_av_co_audio_init(UINT8 *p_codec_type, UINT8 *p_codec_info, UINT8 *p_num_protect,
        UINT8 *p_protect_info, UINT8 index)
{
    FUNC_TRACE();

    APPL_TRACE_DEBUG("bta_av_co_audio_init: %d", index);

    /* By default - no content protection info */
    *p_num_protect = 0;
    *p_protect_info = 0;

    /* reset remote preference through setconfig */
    bta_av_co_cb.codec_cfg_setconfig = NULL;

    switch (index)
    {
    case BTIF_SV_AV_AA_SBC_INDEX:
#if defined(BTA_AV_CO_CP_SCMS_T) && (BTA_AV_CO_CP_SCMS_T == TRUE)
    {
        UINT8 *p = p_protect_info;

        /* Content protection info - support SCMS-T */
        *p_num_protect = 1;
        *p++ = BTA_AV_CP_LOSC;
        UINT16_TO_STREAM(p, BTA_AV_CP_SCMS_T_ID);

    }
#endif
        /* Set up for SBC codec  for SRC*/
        *p_codec_type = BTA_AV_CODEC_SBC;

        /* This should not fail because we are using constants for parameters */
        A2D_BldSbcInfo(AVDT_MEDIA_AUDIO, (tA2D_SBC_CIE *) &bta_av_co_sbc_caps, p_codec_info);

        /* Codec is valid */
        return TRUE;

    case BTIF_SV_AV_AA_APTX_INDEX:
        APPL_TRACE_DEBUG("%s aptX", __func__);
        /* Set up for aptX codec */
        *p_codec_type = A2D_NON_A2DP_MEDIA_CT;
        A2D_BldAptxInfo(AVDT_MEDIA_AUDIO, (tA2D_APTX_CIE *) &bta_av_co_aptx_caps, p_codec_info);
        return TRUE;

    case BTIF_SV_AV_AA_APTX_HD_INDEX:
        APPL_TRACE_DEBUG("%s aptX HD", __func__);
        /* Set up for aptX HD codec */
        *p_codec_type = A2D_NON_A2DP_MEDIA_CT;
        A2D_BldAptx_hdInfo(AVDT_MEDIA_AUDIO, (tA2D_APTX_HD_CIE *) &bta_av_co_aptx_hd_caps, p_codec_info);
        return TRUE;

#if defined(AAC_ENCODER_INCLUDED) && (AAC_ENCODER_INCLUDED == TRUE)
    case BTIF_SV_AV_AA_AAC_INDEX:
        APPL_TRACE_DEBUG("%s AAC", __func__);
        *p_codec_type = BTA_AV_CODEC_M24;
        A2D_BldAacInfo(AVDT_MEDIA_AUDIO, (tA2D_AAC_CIE *) &bta_av_co_aac_caps ,p_codec_info);
        return TRUE;
#endif
#if (BTA_AV_SINK_INCLUDED == TRUE)
    case BTIF_SV_AV_AA_SBC_SINK_INDEX:
        *p_codec_type = BTA_AV_CODEC_SBC;

        /* This should not fail because we are using constants for parameters */
        A2D_BldSbcInfo(AVDT_MEDIA_AUDIO, (tA2D_SBC_CIE *) &bta_av_co_sbc_sink_caps, p_codec_info);

        /* Codec is valid */
        return TRUE;
#endif
    default:
        /* Not valid */
        return FALSE;
    }
}

/*******************************************************************************
 **
 ** Function         bta_av_co_audio_disc_res
 **
 ** Description      This callout function is executed by AV to report the
 **                  number of stream end points (SEP) were found during the
 **                  AVDT stream discovery process.
 **
 **
 ** Returns          void.
 **
 *******************************************************************************/
void bta_av_co_audio_disc_res(tBTA_AV_HNDL hndl, UINT8 num_seps, UINT8 num_snk,
        UINT8 num_src, BD_ADDR addr, UINT16 uuid_local)
{
    tBTA_AV_CO_PEER *p_peer;

    FUNC_TRACE();

    APPL_TRACE_DEBUG("bta_av_co_audio_disc_res h:x%x num_seps:%d num_snk:%d num_src:%d",
            hndl, num_seps, num_snk, num_src);

    /* Find the peer info */
    p_peer = bta_av_co_get_peer(hndl);
    if (p_peer == NULL)
    {
        APPL_TRACE_ERROR("bta_av_co_audio_disc_res could not find peer entry");
        return;
    }

    /* Sanity check : this should never happen */
    if (p_peer->opened)
    {
        APPL_TRACE_ERROR("bta_av_co_audio_disc_res peer already opened");
    }

    /* Copy the discovery results */
    bdcpy(p_peer->addr, addr);
    p_peer->num_snks = num_snk;
    p_peer->num_srcs = num_src;
    p_peer->num_seps = num_seps;
    p_peer->num_rx_snks = 0;
    p_peer->num_rx_srcs = 0;
    p_peer->num_sup_snks = 0;
    if (uuid_local == UUID_SERVCLASS_AUDIO_SINK)
        p_peer->uuid_to_connect = UUID_SERVCLASS_AUDIO_SOURCE;
    else if (uuid_local == UUID_SERVCLASS_AUDIO_SOURCE)
        p_peer->uuid_to_connect = UUID_SERVCLASS_AUDIO_SINK;
}

/*******************************************************************************
 **
 ** Function         bta_av_build_src_cfg
 **
 ** Description      This function will build preferred config from src capabilities
 **
 **
 ** Returns          Pass or Fail for current getconfig.
 **
 *******************************************************************************/
void bta_av_build_src_cfg (UINT8 *p_pref_cfg, UINT8 *p_src_cap)
{
    tA2D_SBC_CIE    src_cap;
    tA2D_SBC_CIE    pref_cap;
    UINT8           status = 0;

    /* initialize it to default SBC configuration */
    A2D_BldSbcInfo(AVDT_MEDIA_AUDIO, (tA2D_SBC_CIE *) &btif_av_sbc_default_config, p_pref_cfg);
    /* now try to build a preferred one */
    /* parse configuration */
    if ((status = A2D_ParsSbcInfo(&src_cap, p_src_cap, TRUE)) != 0)
    {
         APPL_TRACE_DEBUG(" Cant parse src cap ret = %d", status);
         return ;
    }

    if (src_cap.samp_freq & A2D_SBC_IE_SAMP_FREQ_48)
        pref_cap.samp_freq = A2D_SBC_IE_SAMP_FREQ_48;
    else if (src_cap.samp_freq & A2D_SBC_IE_SAMP_FREQ_44)
        pref_cap.samp_freq = A2D_SBC_IE_SAMP_FREQ_44;

    if (src_cap.ch_mode & A2D_SBC_IE_CH_MD_JOINT)
        pref_cap.ch_mode = A2D_SBC_IE_CH_MD_JOINT;
    else if (src_cap.ch_mode & A2D_SBC_IE_CH_MD_STEREO)
        pref_cap.ch_mode = A2D_SBC_IE_CH_MD_STEREO;
    else if (src_cap.ch_mode & A2D_SBC_IE_CH_MD_DUAL)
        pref_cap.ch_mode = A2D_SBC_IE_CH_MD_DUAL;
    else if (src_cap.ch_mode & A2D_SBC_IE_CH_MD_MONO)
        pref_cap.ch_mode = A2D_SBC_IE_CH_MD_MONO;

    if (src_cap.block_len & A2D_SBC_IE_BLOCKS_16)
        pref_cap.block_len = A2D_SBC_IE_BLOCKS_16;
    else if (src_cap.block_len & A2D_SBC_IE_BLOCKS_12)
        pref_cap.block_len = A2D_SBC_IE_BLOCKS_12;
    else if (src_cap.block_len & A2D_SBC_IE_BLOCKS_8)
        pref_cap.block_len = A2D_SBC_IE_BLOCKS_8;
    else if (src_cap.block_len & A2D_SBC_IE_BLOCKS_4)
        pref_cap.block_len = A2D_SBC_IE_BLOCKS_4;

    if (src_cap.num_subbands & A2D_SBC_IE_SUBBAND_8)
        pref_cap.num_subbands = A2D_SBC_IE_SUBBAND_8;
    else if(src_cap.num_subbands & A2D_SBC_IE_SUBBAND_4)
        pref_cap.num_subbands = A2D_SBC_IE_SUBBAND_4;

    if (src_cap.alloc_mthd & A2D_SBC_IE_ALLOC_MD_L)
        pref_cap.alloc_mthd = A2D_SBC_IE_ALLOC_MD_L;
    else if(src_cap.alloc_mthd & A2D_SBC_IE_ALLOC_MD_S)
        pref_cap.alloc_mthd = A2D_SBC_IE_ALLOC_MD_S;

    pref_cap.max_bitpool = src_cap.max_bitpool;
    pref_cap.min_bitpool = src_cap.min_bitpool;

    A2D_BldSbcInfo(AVDT_MEDIA_AUDIO, (tA2D_SBC_CIE *) &pref_cap, p_pref_cfg);
}

/*******************************************************************************
 **
 ** Function         bta_av_audio_sink_getconfig
 **
 ** Description      This callout function is executed by AV to retrieve the
 **                  desired codec and content protection configuration for the
 **                  A2DP Sink audio stream in Initiator.
 **
 **
 ** Returns          Pass or Fail for current getconfig.
 **
 *******************************************************************************/
UINT8 bta_av_audio_sink_getconfig(tBTA_AV_HNDL hndl, tBTA_AV_CODEC codec_type,
        UINT8 *p_codec_info, UINT8 *p_sep_info_idx, UINT8 seid, UINT8 *p_num_protect,
        UINT8 *p_protect_info)
{

    UINT8 result = A2D_FAIL;
    BOOLEAN supported;
    tBTA_AV_CO_PEER *p_peer;
    tBTA_AV_CO_SINK *p_src;
    UINT8 pref_cfg[AVDT_CODEC_SIZE];
    UINT8 index;

    FUNC_TRACE();

    APPL_TRACE_DEBUG("bta_av_audio_sink_getconfig handle:0x%x codec_type:%d seid:%d",
                                                               hndl, codec_type, seid);
    APPL_TRACE_DEBUG("num_protect:0x%02x protect_info:0x%02x%02x%02x",
        *p_num_protect, p_protect_info[0], p_protect_info[1], p_protect_info[2]);

    /* Retrieve the peer info */
    p_peer = bta_av_co_get_peer(hndl);
    if (p_peer == NULL)
    {
        APPL_TRACE_ERROR("bta_av_audio_sink_getconfig could not find peer entry");
        return A2D_FAIL;
    }

    APPL_TRACE_DEBUG("bta_av_audio_sink_getconfig peer(o=%d,n_snks=%d,n_rx_snks=%d,n_sup_snks=%d)",
            p_peer->opened, p_peer->num_srcs, p_peer->num_rx_srcs, p_peer->num_sup_srcs);

    p_peer->num_rx_srcs++;

    /* Check if this is a supported configuration */
    supported = FALSE;
    switch (codec_type)
    {
        case BTA_AV_CODEC_SBC:
            supported = TRUE;
            break;

        default:
            break;
    }

    if (supported)
    {
        /* If there is room for a new one */
        if (p_peer->num_sup_srcs < BTA_AV_CO_NUM_ELEMENTS(p_peer->srcs))
        {
            p_src = &p_peer->srcs[p_peer->num_sup_srcs++];

            APPL_TRACE_DEBUG("bta_av_audio_sink_getconfig saved caps[%x:%x:%x:%x:%x:%x]",
                    p_codec_info[1], p_codec_info[2], p_codec_info[3],
                    p_codec_info[4], p_codec_info[5], p_codec_info[6]);

            memcpy(p_src->codec_caps, p_codec_info, AVDT_CODEC_SIZE);
            p_src->codec_type = codec_type;
            p_src->sep_info_idx = *p_sep_info_idx;
            p_src->seid = seid;
            p_src->num_protect = *p_num_protect;
            memcpy(p_src->protect_info, p_protect_info, BTA_AV_CP_INFO_LEN);
        }
        else
        {
            APPL_TRACE_ERROR("bta_av_audio_sink_getconfig no more room for SRC info");
        }
    }

    /* If last SNK get capabilities or all supported codec caps retrieved */
    if ((p_peer->num_rx_srcs == p_peer->num_srcs) ||
        (p_peer->num_sup_srcs == BTA_AV_CO_NUM_ELEMENTS(p_peer->srcs)))
    {
        APPL_TRACE_DEBUG("bta_av_audio_sink_getconfig last SRC reached");

        /* Protect access to bta_av_co_cb.codec_cfg */
        mutex_global_lock();

        /* Find a src that matches the codec config */
        if (bta_av_co_audio_peer_src_supports_codec(p_peer, &index))
        {
            APPL_TRACE_DEBUG(" Codec Supported ");
            p_src = &p_peer->srcs[index];

            /* Build the codec configuration for this sink */
            {
                /* Save the new configuration */
                p_peer->p_src = p_src;
                /* get preferred config from src_caps */
                bta_av_build_src_cfg(pref_cfg, p_src->codec_caps);
                memcpy(p_peer->codec_cfg, pref_cfg, AVDT_CODEC_SIZE);

                APPL_TRACE_DEBUG("bta_av_audio_sink_getconfig  p_codec_info[%x:%x:%x:%x:%x:%x]",
                        p_peer->codec_cfg[1], p_peer->codec_cfg[2], p_peer->codec_cfg[3],
                        p_peer->codec_cfg[4], p_peer->codec_cfg[5], p_peer->codec_cfg[6]);
                /* By default, no content protection */
                *p_num_protect = 0;

#if defined(BTA_AV_CO_CP_SCMS_T) && (BTA_AV_CO_CP_SCMS_T == TRUE)
                    p_peer->cp_active = FALSE;
                    bta_av_co_cb.cp.active = FALSE;
#endif

                    *p_sep_info_idx = p_src->sep_info_idx;
                    memcpy(p_codec_info, p_peer->codec_cfg, AVDT_CODEC_SIZE);
                result =  A2D_SUCCESS;
            }
        }
        /* Protect access to bta_av_co_cb.codec_cfg */
        mutex_global_unlock();
    }
    return result;
}
/*******************************************************************************
 **
 ** Function         bta_av_co_audio_getconfig
 **
 ** Description      This callout function is executed by AV to retrieve the
 **                  desired codec and content protection configuration for the
 **                  audio stream.
 **
 **
 ** Returns          Stream codec and content protection configuration info.
 **
 *******************************************************************************/
UINT8 bta_av_co_audio_getconfig(tBTA_AV_HNDL hndl, tBTA_AV_CODEC codec_type,
                                UINT8 *p_codec_info, UINT8 *p_sep_info_idx, UINT8 seid, UINT8 *p_num_protect,
                                UINT8 *p_protect_info)

{
    UINT8 result = A2D_FAIL;
    BOOLEAN supported;
    tBTA_AV_CO_PEER *p_peer;
    tBTA_AV_CO_SINK *p_sink;
    UINT8 codec_cfg[AVDT_CODEC_SIZE];
    UINT8 index;

    FUNC_TRACE();

    /* Retrieve the peer info */
    p_peer = bta_av_co_get_peer(hndl);
    if (p_peer == NULL)
    {
        APPL_TRACE_ERROR("bta_av_co_audio_getconfig could not find peer entry");
        return A2D_FAIL;
    }

    if (p_peer->uuid_to_connect == UUID_SERVCLASS_AUDIO_SOURCE)
    {
        result = bta_av_audio_sink_getconfig(hndl, codec_type, p_codec_info, p_sep_info_idx,
                                             seid, p_num_protect, p_protect_info);
        return result;
    }
    APPL_TRACE_DEBUG("bta_av_co_audio_getconfig handle:0x%x codec_type:%d seid:%d",
                                                              hndl, codec_type, seid);
    APPL_TRACE_DEBUG("num_protect:0x%02x protect_info:0x%02x%02x%02x",
        *p_num_protect, p_protect_info[0], p_protect_info[1], p_protect_info[2]);

    APPL_TRACE_DEBUG("bta_av_co_audio_getconfig peer(o=%d,n_snks=%d,n_rx_snks=%d,n_sup_snks=%d)",
            p_peer->opened, p_peer->num_snks, p_peer->num_rx_snks, p_peer->num_sup_snks);

    p_peer->num_rx_snks++;

    /* Check if this is a supported configuration */
    supported = FALSE;
    switch (codec_type)
    {
    case BTA_AV_CODEC_SBC:
        supported = TRUE;
        break;
    case A2D_NON_A2DP_MEDIA_CT:
    {
        UINT16 codecId = ((tA2D_APTX_CIE*)(&p_codec_info[BTA_AV_CFG_START_IDX]))->codecId;
        UINT32 vendorId = ((tA2D_APTX_CIE*)(&p_codec_info[BTA_AV_CFG_START_IDX]))->vendorId;
        APPL_TRACE_DEBUG("%s codecId = %d", __func__, codecId );
        APPL_TRACE_DEBUG("%s vendorId = %x", __func__, vendorId );

        if (codecId ==  A2D_APTX_CODEC_ID_BLUETOOTH && vendorId == A2D_APTX_VENDOR_ID) {
            /* aptX */
            supported = TRUE;
        } else if (codecId ==  A2D_APTX_HD_CODEC_ID_BLUETOOTH && vendorId == A2D_APTX_HD_VENDOR_ID) {
            /*	aptX HD	*/
            supported = TRUE;
        }
        break;
    }
#if defined(AAC_ENCODER_INCLUDED) && (AAC_ENCODER_INCLUDED == TRUE)
    case BTA_AV_CODEC_M24:
        APPL_TRACE_DEBUG("%s: AAC is supported", __func__);
        supported = TRUE;
        break;
#endif
    default:
        break;
    }

    if (supported)
    {
        /* If there is room for a new one */
        if (p_peer->num_sup_snks < BTA_AV_CO_NUM_ELEMENTS(p_peer->snks))
        {
            int i = 0;
            p_sink = &p_peer->snks[p_peer->num_sup_snks++];

            APPL_TRACE_DEBUG("bta_av_co_audio_getconfig saved caps[%x:%x:%x:%x:%x:%x]",
                    p_codec_info[1], p_codec_info[2], p_codec_info[3],
                    p_codec_info[4], p_codec_info[5], p_codec_info[6]);

            for (i = 0 ; i < AVDT_CODEC_SIZE; i++)
                APPL_TRACE_DEBUG("%s p_codec_info[%d]: %x", __func__, i,  p_codec_info[i]);

            if (codec_type == A2D_NON_A2DP_MEDIA_CT)
                memcpy(p_sink->codec_caps, &p_codec_info[BTA_AV_CFG_START_IDX], AVDT_CODEC_SIZE);
            else
                memcpy(p_sink->codec_caps, p_codec_info, AVDT_CODEC_SIZE);

            p_sink->codec_type = codec_type;
            p_sink->sep_info_idx = *p_sep_info_idx;
            p_sink->seid = seid;
            p_sink->num_protect = *p_num_protect;
            memcpy(p_sink->protect_info, p_protect_info, BTA_AV_CP_INFO_LEN);
        }
        else
        {
            APPL_TRACE_ERROR("bta_av_co_audio_getconfig no more room for SNK info");
        }
    }

    /* If last SNK get capabilities or all supported codec capa retrieved */
    if ((p_peer->num_rx_snks == p_peer->num_snks) ||
        (p_peer->num_sup_snks == BTA_AV_CO_NUM_ELEMENTS(p_peer->snks)))
    {
        APPL_TRACE_DEBUG("bta_av_co_audio_getconfig last sink reached");

        /* Protect access to bta_av_co_cb.codec_cfg */
        mutex_global_lock();

        /* Find a sink that matches the codec config */
        if (bta_av_co_audio_peer_supports_codec(p_peer, &index, NULL))
        {
            /* stop fetching caps once we retrieved a supported codec */
            if (p_peer->acp)
            {
                *p_sep_info_idx = p_peer->num_seps;
                APPL_TRACE_EVENT("no need to fetch more SEPs");
            }

            p_sink = &p_peer->snks[index];

            /* Build the codec configuration for this sink */
            if (bta_av_co_audio_codec_build_config(p_sink->codec_caps, codec_cfg))
            {
                int i = 0;
                APPL_TRACE_DEBUG("bta_av_co_audio_getconfig setconfig "
                        "p_codec_info[%x:%x:%x:%x:%x:%x]",
                        codec_cfg[1], codec_cfg[2], codec_cfg[3],
                        codec_cfg[4], codec_cfg[5], codec_cfg[6]);

                for (i = 0 ; i < AVDT_CODEC_SIZE; i++)
                    APPL_TRACE_DEBUG("%s p_codec_info[%d]: %x", __func__, i,  p_codec_info[i]);

                /* Save the new configuration */
                p_peer->p_snk = p_sink;
                memcpy(p_peer->codec_cfg, codec_cfg, AVDT_CODEC_SIZE);

                /* By default, no content protection */
                *p_num_protect = 0;

#if defined(BTA_AV_CO_CP_SCMS_T) && (BTA_AV_CO_CP_SCMS_T == TRUE)
                /* Check if this sink supports SCMS */
                if (bta_av_co_audio_sink_has_scmst(p_sink))
                {
                    p_peer->cp_active = TRUE;
                    bta_av_co_cb.cp.active = TRUE;
                    *p_num_protect = BTA_AV_CP_INFO_LEN;
                    memcpy(p_protect_info, bta_av_co_cp_scmst, BTA_AV_CP_INFO_LEN);
                }
                else
                {
                    p_peer->cp_active = FALSE;
                    bta_av_co_cb.cp.active = FALSE;
                }
#endif

                /* If acceptor -> reconfig otherwise reply for configuration */
                if (p_peer->acp)
                {
                    if (p_peer->recfg_needed)
                    {
                        APPL_TRACE_DEBUG("bta_av_co_audio_getconfig call BTA_AvReconfig(x%x)", hndl);
                        BTA_AvReconfig(hndl, TRUE, p_sink->sep_info_idx, p_peer->codec_cfg, *p_num_protect, (UINT8 *)bta_av_co_cp_scmst);
                    }
                }
                else
                {
                    *p_sep_info_idx = p_sink->sep_info_idx;
                    memcpy(p_codec_info, p_peer->codec_cfg, AVDT_CODEC_SIZE);
                }
                result =  A2D_SUCCESS;
            }
        }
        /* Protect access to bta_av_co_cb.codec_cfg */
        mutex_global_unlock();
    }
    return result;
}

/*******************************************************************************
 **
 ** Function         bta_av_co_audio_setconfig
 **
 ** Description      This callout function is executed by AV to set the codec and
 **                  content protection configuration of the audio stream.
 **
 **
 ** Returns          void
 **
 *******************************************************************************/
void bta_av_co_audio_setconfig(tBTA_AV_HNDL hndl, tBTA_AV_CODEC codec_type,
        UINT8 *p_codec_info, UINT8 seid, BD_ADDR addr, UINT8 num_protect, UINT8 *p_protect_info,
        UINT8 t_local_sep, UINT8 avdt_handle)
{
    tBTA_AV_CO_PEER *p_peer;
    UINT8 status = A2D_SUCCESS;
    UINT8 category = A2D_SUCCESS;
    BOOLEAN recfg_needed = FALSE;
    BOOLEAN codec_cfg_supported = FALSE;
    UNUSED(seid);
    UNUSED(addr);

    FUNC_TRACE();

    APPL_TRACE_IMP("bta_av_co_audio_setconfig p_codec_info[%x:%x:%x:%x:%x:%x]",
            p_codec_info[1], p_codec_info[2], p_codec_info[3],
            p_codec_info[4], p_codec_info[5], p_codec_info[6]);
    APPL_TRACE_DEBUG("num_protect:0x%02x protect_info:0x%02x%02x%02x",
        num_protect, p_protect_info[0], p_protect_info[1], p_protect_info[2]);

    /* Retrieve the peer info */
    p_peer = bta_av_co_get_peer(hndl);
    if (p_peer == NULL)
    {
        APPL_TRACE_ERROR("bta_av_co_audio_setconfig could not find peer entry");

        /* Call call-in rejecting the configuration */
        bta_av_ci_setconfig(hndl, A2D_BUSY, AVDT_ASC_CODEC, 0, NULL, FALSE, avdt_handle);
        return;
    }
    APPL_TRACE_DEBUG("bta_av_co_audio_setconfig peer(o=%d,n_snks=%d,n_rx_snks=%d,n_sup_snks=%d)",
            p_peer->opened, p_peer->num_snks, p_peer->num_rx_snks, p_peer->num_sup_snks);

    /* Sanity check: should not be opened at this point */
    if (p_peer->opened)
    {
        APPL_TRACE_ERROR("bta_av_co_audio_setconfig peer already in use");
    }

#if defined(BTA_AV_CO_CP_SCMS_T) && (BTA_AV_CO_CP_SCMS_T == TRUE)
    if (num_protect != 0)
    {
        /* If CP is supported */
        if ((num_protect != 1) ||
            (bta_av_co_cp_is_scmst(p_protect_info) == FALSE))
        {
            APPL_TRACE_ERROR("bta_av_co_audio_setconfig wrong CP configuration");
            status = A2D_BAD_CP_TYPE;
            category = AVDT_ASC_PROTECT;
        }
    }
#else
    /* Do not support content protection for the time being */
    if (num_protect != 0)
    {
        APPL_TRACE_ERROR("bta_av_co_audio_setconfig wrong CP configuration");
        status = A2D_BAD_CP_TYPE;
        category = AVDT_ASC_PROTECT;
    }
#endif
    if (status == A2D_SUCCESS)
    {
        if(AVDT_TSEP_SNK == t_local_sep)
        {
            codec_cfg_supported = bta_av_co_audio_sink_supports_config(codec_type, p_codec_info);
            APPL_TRACE_DEBUG(" Peer is  A2DP SRC ");
        }
        if(AVDT_TSEP_SRC == t_local_sep)
        {
            codec_cfg_supported = bta_av_co_audio_media_supports_config(codec_type, p_codec_info);
            APPL_TRACE_DEBUG(" Peer is A2DP SINK ");
        }
        /* Check if codec configuration is supported */
        if (codec_cfg_supported)
        {

            /* Protect access to bta_av_co_cb.codec_cfg */
            mutex_global_lock();

            /* Check if the configuration matches the current codec config */
            switch (codec_type)
            {
            case BTIF_AV_CODEC_SBC:
                if ((codec_type != BTA_AV_CODEC_SBC) || memcmp(p_codec_info, bta_av_co_cb.codec_cfg_sbc.info, 5))
                {
                    recfg_needed = TRUE;
                }
                else if ((num_protect == 1) && (!bta_av_co_cb.cp.active))
                {
                    recfg_needed = TRUE;
                }

                /* if remote side requests a restricted notify sinks preferred bitpool range as all other params are
                   already checked for validify */
                APPL_TRACE_DEBUG("%s SBC", __func__);
                APPL_TRACE_EVENT("remote peer setconfig bitpool range [%d:%d]",
                   p_codec_info[BTA_AV_CO_SBC_MIN_BITPOOL_OFF],
                   p_codec_info[BTA_AV_CO_SBC_MAX_BITPOOL_OFF] );

                bta_av_co_cb.codec_cfg_sbc_setconfig.id = BTIF_AV_CODEC_SBC;
                memcpy(bta_av_co_cb.codec_cfg_sbc_setconfig.info, p_codec_info, AVDT_CODEC_SIZE);
                bta_av_co_cb.codec_cfg_setconfig = &bta_av_co_cb.codec_cfg_sbc_setconfig;
                memcpy(bta_av_co_cb.codec_cfg_sbc.info, p_codec_info, AVDT_CODEC_SIZE);
                bta_av_co_cb.codec_cfg = &bta_av_co_cb.codec_cfg_sbc;
                if(AVDT_TSEP_SNK == t_local_sep)
                {
                    /* If Peer is SRC, and our cfg subset matches with what is requested by peer, then
                                         just accept what peer wants */
                    recfg_needed = FALSE;
                }
                break;

            case A2D_NON_A2DP_MEDIA_CT:
            {
                UINT16 codecId;
                UINT32 vendorId;
                codecId = ((tA2D_APTX_CIE*)(&p_codec_info[BTA_AV_CFG_START_IDX]))->codecId;
                vendorId = ((tA2D_APTX_CIE*)(&p_codec_info[BTA_AV_CFG_START_IDX]))->vendorId;
                APPL_TRACE_DEBUG("%s codec_type = %x", __func__, codec_type);
                APPL_TRACE_DEBUG("%s codecId = %d", __func__, codecId);
                APPL_TRACE_DEBUG("%s vendorId = %x", __func__, vendorId);

                if ((codec_type != A2D_NON_A2DP_MEDIA_CT) ||
                    (codecId != A2D_APTX_CODEC_ID_BLUETOOTH) ||
                    (vendorId != A2D_APTX_VENDOR_ID) ||
                    memcmp(p_codec_info, bta_av_co_cb.codec_cfg_aptx.info, 5))
                {
                    APPL_TRACE_DEBUG("%s recfg_needed", __func__);
                    recfg_needed = TRUE;
                }
                else if ((num_protect == 1) && (!bta_av_co_cb.cp.active))
                {
                    APPL_TRACE_DEBUG("%s recfg_needed", __func__);
                    recfg_needed = TRUE;
                }

                if ((codecId == A2D_APTX_CODEC_ID_BLUETOOTH) && (vendorId == A2D_APTX_VENDOR_ID)) {
                    APPL_TRACE_DEBUG("%s aptX", __func__);
                    bta_av_co_cb.codec_cfg_aptx_setconfig.id = A2D_NON_A2DP_MEDIA_CT;
                    memcpy(bta_av_co_cb.codec_cfg_aptx_setconfig.info, p_codec_info, AVDT_CODEC_SIZE);
                    bta_av_co_cb.codec_cfg_setconfig = &bta_av_co_cb.codec_cfg_aptx_setconfig;
                } else if ((codecId == A2D_APTX_HD_CODEC_ID_BLUETOOTH) && (vendorId == A2D_APTX_HD_VENDOR_ID)) {
                    APPL_TRACE_DEBUG("%s aptX HD", __func__);
                    bta_av_co_cb.codec_cfg_aptx_hd_setconfig.id = A2D_NON_A2DP_MEDIA_CT;
                    memcpy(bta_av_co_cb.codec_cfg_aptx_hd_setconfig.info, p_codec_info, AVDT_CODEC_SIZE);
                    bta_av_co_cb.codec_cfg_setconfig = &bta_av_co_cb.codec_cfg_aptx_hd_setconfig;
                }
                break;
             }
#if defined(AAC_ENCODER_INCLUDED) && (AAC_ENCODER_INCLUDED == TRUE)
            case BTA_AV_CODEC_M24:
            {
                APPL_TRACE_DEBUG("%s AAC", __func__);
                bta_av_co_cb.codec_cfg_aac_setconfig.id = BTIF_AV_CODEC_M24;
                memcpy(bta_av_co_cb.codec_cfg_aac_setconfig.info, p_codec_info, AVDT_CODEC_SIZE);
                bta_av_co_cb.codec_cfg_setconfig = &bta_av_co_cb.codec_cfg_aac_setconfig;

                APPL_TRACE_DEBUG("%s codec_type = %x", __func__, codec_type);
            } break;
#endif
            default:
                APPL_TRACE_ERROR("bta_av_co_audio_setconfig unsupported cid %d", bta_av_co_cb.codec_cfg->id);
                recfg_needed = TRUE;
                break;
            }
            /* Protect access to bta_av_co_cb.codec_cfg */
            mutex_global_unlock();
        }
        else
        {
            category = AVDT_ASC_CODEC;
            status = A2D_WRONG_CODEC;
        }
    }

    if (status != A2D_SUCCESS)
    {
        APPL_TRACE_ERROR("bta_av_co_audio_setconfig reject s=%d c=%d", status, category);

        /* Call call-in rejecting the configuration */
        bta_av_ci_setconfig(hndl, status, category, 0, NULL, FALSE, avdt_handle);
    }
    else
    {
        /* Mark that this is an acceptor peer */
        p_peer->acp = TRUE;
        p_peer->recfg_needed = recfg_needed;

        APPL_TRACE_DEBUG("bta_av_co_audio_setconfig accept reconf=%d", recfg_needed);

        /* Call call-in accepting the configuration */
        bta_av_ci_setconfig(hndl, A2D_SUCCESS, A2D_SUCCESS, 0, NULL, recfg_needed, avdt_handle);
    }
}

/*******************************************************************************
 **
 ** Function         bta_av_co_audio_open
 **
 ** Description      This function is called by AV when the audio stream connection
 **                  is opened.
 **
 **
 ** Returns          void
 **
 *******************************************************************************/
void bta_av_co_audio_open(tBTA_AV_HNDL hndl, tBTA_AV_CODEC codec_type, UINT8 *p_codec_info,
                          UINT16 mtu)
{
    tBTA_AV_CO_PEER *p_peer;
    UNUSED(p_codec_info);

    FUNC_TRACE();

    APPL_TRACE_DEBUG("bta_av_co_audio_open mtu:%d codec_type:%d", mtu, codec_type);

    /* Retrieve the peer info */
    p_peer = bta_av_co_get_peer(hndl);
    if (p_peer == NULL)
    {
        APPL_TRACE_ERROR("bta_av_co_audio_setconfig could not find peer entry");
    }
    else
    {
        p_peer->opened = TRUE;
        p_peer->mtu = mtu;
    }
}

/*******************************************************************************
 **
 ** Function         bta_av_co_audio_close
 **
 ** Description      This function is called by AV when the audio stream connection
 **                  is closed.
 **
 **
 ** Returns          void
 **
 *******************************************************************************/
void bta_av_co_audio_close(tBTA_AV_HNDL hndl, tBTA_AV_CODEC codec_type, UINT16 mtu)

{
    tBTA_AV_CO_PEER *p_peer;
    UNUSED(codec_type);
    UNUSED(mtu);

    FUNC_TRACE();

    APPL_TRACE_DEBUG("bta_av_co_audio_close");

    /* Retrieve the peer info */
    p_peer = bta_av_co_get_peer(hndl);
    if (p_peer)
    {
        /* Mark the peer closed and clean the peer info */
        memset(p_peer, 0, sizeof(*p_peer));
    }
    else
    {
        APPL_TRACE_ERROR("bta_av_co_audio_close could not find peer entry");
    }

    /* reset remote preference through setconfig */
    bta_av_co_cb.codec_cfg_setconfig = NULL;
}

/*******************************************************************************
 **
 ** Function         bta_av_co_audio_start
 **
 ** Description      This function is called by AV when the audio streaming data
 **                  transfer is started.
 **
 **
 ** Returns          void
 **
 *******************************************************************************/
void bta_av_co_audio_start(tBTA_AV_HNDL hndl, tBTA_AV_CODEC codec_type,
                           UINT8 *p_codec_info, BOOLEAN *p_no_rtp_hdr)
{
    UNUSED(hndl);

    FUNC_TRACE();

    APPL_TRACE_DEBUG("bta_av_co_audio_start");

    if (codec_type == A2D_NON_A2DP_MEDIA_CT) {
        UINT16 codecId = ((tA2D_APTX_CIE*)(&p_codec_info[BTA_AV_CFG_START_IDX]))->codecId;
        UINT32 vendorId = ((tA2D_APTX_CIE*)(&p_codec_info[BTA_AV_CFG_START_IDX]))->vendorId;

        // for aptX, we only add RTP hdr along with CP
        if (codecId == A2D_APTX_CODEC_ID_BLUETOOTH && vendorId == A2D_APTX_VENDOR_ID) {
            if (!bta_av_co_cb.cp.active)
                *p_no_rtp_hdr = TRUE;
        }
    }
}

/*******************************************************************************
 **
 ** Function         bta_av_co_audio_stop
 **
 ** Description      This function is called by AV when the audio streaming data
 **                  transfer is stopped.
 **
 **
 ** Returns          void
 **
 *******************************************************************************/
extern void bta_av_co_audio_stop(tBTA_AV_HNDL hndl, tBTA_AV_CODEC codec_type)
{
    UNUSED(hndl);
    UNUSED(codec_type);

    FUNC_TRACE();

    APPL_TRACE_DEBUG("bta_av_co_audio_stop");
}

/*******************************************************************************
 **
 ** Function         bta_av_co_audio_src_data_path
 **
 ** Description      This function is called to manage data transfer from
 **                  the audio codec to AVDTP.
 **
 ** Returns          Pointer to the GKI buffer to send, NULL if no buffer to send
 **
 *******************************************************************************/
void * bta_av_co_audio_src_data_path(tBTA_AV_CODEC codec_type, UINT32 *p_len,
                                     UINT32 *p_timestamp)
{
    BT_HDR *p_buf;
    UNUSED(p_len);

    FUNC_TRACE();

    p_buf = btif_media_aa_readbuf();
    if (p_buf != NULL)
    {
        switch (codec_type)
        {
        case BTA_AV_CODEC_SBC:
            /* In media packet SBC, the following information is available:
             * p_buf->layer_specific : number of SBC frames in the packet
             * p_buf->word[0] : timestamp
             */
            /* Retrieve the timestamp information from the media packet */
            *p_timestamp = *((UINT32 *) (p_buf + 1));

            /* Set up packet header */
            bta_av_sbc_bld_hdr(p_buf, p_buf->layer_specific);
            break;
        case A2D_NON_A2DP_MEDIA_CT:
            /* Retrieve the timestamp information from the media packet */
            *p_timestamp = *((UINT32 *) (p_buf + 1));
            break;
        default:
            APPL_TRACE_ERROR("bta_av_co_audio_src_data_path Unsupported codec type (%d)", codec_type);
            break;
        }
#if defined(BTA_AV_CO_CP_SCMS_T) && (BTA_AV_CO_CP_SCMS_T == TRUE)
        {
            UINT8 *p;
            if (bta_av_co_cp_is_active())
            {
                p_buf->len++;
                p_buf->offset--;
                p = (UINT8 *)(p_buf + 1) + p_buf->offset;
                *p = bta_av_co_cp_get_flag();
            }
        }
#endif
    }
    return p_buf;
}

/*******************************************************************************
 **
 ** Function         bta_av_co_audio_drop
 **
 ** Description      An Audio packet is dropped. .
 **                  It's very likely that the connected headset with this handle
 **                  is moved far away. The implementation may want to reduce
 **                  the encoder bit rate setting to reduce the packet size.
 **
 ** Returns          void
 **
 *******************************************************************************/
void bta_av_co_audio_drop(tBTA_AV_HNDL hndl)
{
    FUNC_TRACE();

    APPL_TRACE_ERROR("bta_av_co_audio_drop dropped: x%x", hndl);
}

/*******************************************************************************
 **
 ** Function         bta_av_co_audio_delay
 **
 ** Description      This function is called by AV when the audio stream connection
 **                  needs to send the initial delay report to the connected SRC.
 **
 **
 ** Returns          void
 **
 *******************************************************************************/
void bta_av_co_audio_delay(tBTA_AV_HNDL hndl, UINT16 delay)
{
    FUNC_TRACE();

    APPL_TRACE_ERROR("bta_av_co_audio_delay handle: x%x, delay:0x%x", hndl, delay);
}



/*******************************************************************************
 **
 ** Function         bta_av_co_audio_codec_build_config
 **
 ** Description      Build the codec configuration
 **
 ** Returns          TRUE if the codec was built successfully, FALSE otherwise
 **
 *******************************************************************************/
static BOOLEAN bta_av_co_audio_codec_build_config(const UINT8 *p_codec_caps, UINT8 *p_codec_cfg)
{
    FUNC_TRACE();
    tA2D_SBC_CIE peer_sbc_cfg;
    tA2D_SBC_CIE sbc_cfg_selected;
    tA2D_AAC_CIE peer_aac_cfg;
    tA2D_AAC_CIE aac_cfg_selected;
    tA2D_APTX_CIE aptx_cfg_selected;
    UINT8           status = 0;

    memset(p_codec_cfg, 0, AVDT_CODEC_SIZE);

    switch (bta_av_co_cb.codec_cfg->id)
    {
    case BTIF_AV_CODEC_SBC:
        if ((status = A2D_ParsSbcInfo (&peer_sbc_cfg ,
                (UINT8*)p_codec_caps, FALSE)) != 0)
        {
             APPL_TRACE_DEBUG(" Cant parse peer_sbc_cfg ret = %d", status);
             return FALSE;
        }
        if ((status = A2D_ParsSbcInfo (&sbc_cfg_selected ,
            bta_av_co_cb.codec_cfg->info, FALSE)) != 0)
        {
             APPL_TRACE_DEBUG(" Cant parse sbc_cfg_selected ret = %d", status);
             return FALSE;
        }
        if ((peer_sbc_cfg.samp_freq & A2D_SBC_IE_SAMP_FREQ_48) &&
            (sbc_cfg_selected.samp_freq & A2D_SBC_IE_SAMP_FREQ_48))
            sbc_cfg_selected.samp_freq = A2D_SBC_IE_SAMP_FREQ_48;
        else if ((peer_sbc_cfg.samp_freq & A2D_SBC_IE_SAMP_FREQ_44) &&
            (sbc_cfg_selected.samp_freq & A2D_SBC_IE_SAMP_FREQ_44))
            sbc_cfg_selected.samp_freq = A2D_SBC_IE_SAMP_FREQ_44;
        else if ((peer_sbc_cfg.samp_freq & A2D_SBC_IE_SAMP_FREQ_32) &&
            (sbc_cfg_selected.samp_freq & A2D_SBC_IE_SAMP_FREQ_32))
            sbc_cfg_selected.samp_freq = A2D_SBC_IE_SAMP_FREQ_32;
        else if ((peer_sbc_cfg.samp_freq & A2D_SBC_IE_SAMP_FREQ_16) &&
            (sbc_cfg_selected.samp_freq & A2D_SBC_IE_SAMP_FREQ_16))
            sbc_cfg_selected.samp_freq = A2D_SBC_IE_SAMP_FREQ_16;

        if ((peer_sbc_cfg.ch_mode & A2D_SBC_IE_CH_MD_JOINT) &&
            (sbc_cfg_selected.ch_mode & A2D_SBC_IE_CH_MD_JOINT))
            sbc_cfg_selected.ch_mode = A2D_SBC_IE_CH_MD_JOINT;
        else if ((peer_sbc_cfg.ch_mode & A2D_SBC_IE_CH_MD_STEREO) &&
            (sbc_cfg_selected.ch_mode & A2D_SBC_IE_CH_MD_STEREO))
            sbc_cfg_selected.ch_mode = A2D_SBC_IE_CH_MD_STEREO;
        else if ((peer_sbc_cfg.ch_mode & A2D_SBC_IE_CH_MD_DUAL) &&
            (sbc_cfg_selected.ch_mode & A2D_SBC_IE_CH_MD_DUAL))
            sbc_cfg_selected.ch_mode = A2D_SBC_IE_CH_MD_DUAL;
        else if ((peer_sbc_cfg.ch_mode & A2D_SBC_IE_CH_MD_MONO) &&
            (sbc_cfg_selected.ch_mode & A2D_SBC_IE_CH_MD_MONO))
            sbc_cfg_selected.ch_mode = A2D_SBC_IE_CH_MD_MONO;

        if ((peer_sbc_cfg.block_len & A2D_SBC_IE_BLOCKS_16) &&
            (sbc_cfg_selected.block_len & A2D_SBC_IE_BLOCKS_16))
            sbc_cfg_selected.block_len = A2D_SBC_IE_BLOCKS_16;
        else if ((peer_sbc_cfg.block_len & A2D_SBC_IE_BLOCKS_12) &&
            (sbc_cfg_selected.block_len & A2D_SBC_IE_BLOCKS_12))
            sbc_cfg_selected.block_len = A2D_SBC_IE_BLOCKS_12;
        else if ((peer_sbc_cfg.block_len & A2D_SBC_IE_BLOCKS_8) &&
            (sbc_cfg_selected.block_len & A2D_SBC_IE_BLOCKS_8))
            sbc_cfg_selected.block_len = A2D_SBC_IE_BLOCKS_8;
        else if ((peer_sbc_cfg.block_len & A2D_SBC_IE_BLOCKS_4) &&
            (sbc_cfg_selected.block_len & A2D_SBC_IE_BLOCKS_4))
            sbc_cfg_selected.block_len = A2D_SBC_IE_BLOCKS_4;

        if ((peer_sbc_cfg.num_subbands & A2D_SBC_IE_SUBBAND_8) &&
            (sbc_cfg_selected.num_subbands & A2D_SBC_IE_SUBBAND_8))
            sbc_cfg_selected.num_subbands = A2D_SBC_IE_SUBBAND_8;
        else if ((peer_sbc_cfg.num_subbands & A2D_SBC_IE_SUBBAND_4) &&
            (sbc_cfg_selected.num_subbands & A2D_SBC_IE_SUBBAND_4))
            sbc_cfg_selected.num_subbands = A2D_SBC_IE_SUBBAND_4;

        if ((peer_sbc_cfg.alloc_mthd & A2D_SBC_IE_ALLOC_MD_L) &&
            (sbc_cfg_selected.alloc_mthd & A2D_SBC_IE_ALLOC_MD_L))
            sbc_cfg_selected.alloc_mthd = A2D_SBC_IE_ALLOC_MD_L;
        else if ((peer_sbc_cfg.alloc_mthd & A2D_SBC_IE_ALLOC_MD_S) &&
            (sbc_cfg_selected.alloc_mthd & A2D_SBC_IE_ALLOC_MD_S))
            sbc_cfg_selected.alloc_mthd = A2D_SBC_IE_ALLOC_MD_S;

        sbc_cfg_selected.min_bitpool =
            BTA_AV_CO_MAX(peer_sbc_cfg.min_bitpool,
                            sbc_cfg_selected.min_bitpool);
        sbc_cfg_selected.max_bitpool =
            BTA_AV_CO_MIN(peer_sbc_cfg.max_bitpool,
                            sbc_cfg_selected.max_bitpool);

        //update with new value
        A2D_BldSbcInfo (AVDT_MEDIA_AUDIO, &sbc_cfg_selected, bta_av_co_cb.codec_cfg->info);

        memcpy(p_codec_cfg, bta_av_co_cb.codec_cfg->info, A2D_SBC_INFO_LEN + 1);
        memcpy(bta_av_co_cb.codec_cfg_sbc.info, bta_av_co_cb.codec_cfg->info,
            A2D_SBC_INFO_LEN + 1);
        APPL_TRACE_DEBUG("%s SBC", __func__);
        APPL_TRACE_EVENT("bta_av_co_audio_codec_build_config : freq %d, chan mode %d "
                        "block len %d, subbands %d alloc %d bitpool min %d, max %d",
                    sbc_cfg_selected.samp_freq, sbc_cfg_selected.ch_mode,
                    sbc_cfg_selected.block_len, sbc_cfg_selected.num_subbands,
                    sbc_cfg_selected.alloc_mthd, sbc_cfg_selected.min_bitpool,
                    sbc_cfg_selected.max_bitpool);
        break;
#if defined(AAC_ENCODER_INCLUDED) && (AAC_ENCODER_INCLUDED == TRUE)
    case BTIF_AV_CODEC_M24:
        /*  only copy the relevant portions for this codec to avoid issues when
            comparing codec configs covering larger codec sets than SBC (7 bytes) */
        A2D_ParsAacInfo (&peer_aac_cfg ,(UINT8*)p_codec_caps, FALSE);
        A2D_ParsAacInfo (&aac_cfg_selected ,bta_av_co_cb.codec_cfg->info, FALSE);

        aac_cfg_selected.bit_rate =
                        BTA_AV_CO_MIN(peer_aac_cfg.bit_rate,
                                      aac_cfg_selected.bit_rate);
        APPL_TRACE_EVENT("%s AAC bitrate selected %d", __func__,
                                      aac_cfg_selected.bit_rate);
        //update with new value
        A2D_BldAacInfo (AVDT_MEDIA_AUDIO, &aac_cfg_selected, bta_av_co_cb.codec_cfg->info);

        memcpy(p_codec_cfg, bta_av_co_cb.codec_cfg->info, A2D_AAC_INFO_LEN+1);
        memcpy(bta_av_co_cb.codec_cfg_aac.info, bta_av_co_cb.codec_cfg->info,
            A2D_AAC_INFO_LEN + 1);
        APPL_TRACE_DEBUG("%s AAC", __func__);
        break;
#endif
    case A2D_NON_A2DP_MEDIA_CT:
    {
        UINT16 codecId;
        UINT16 vendorId;
        codecId = ((tA2D_APTX_CIE*)p_codec_caps)->codecId;
        vendorId = ((tA2D_APTX_CIE*)p_codec_caps)->vendorId;
        APPL_TRACE_DEBUG("%s codecId = %d", __func__, codecId);
        APPL_TRACE_DEBUG("%s vendorId = %x", __func__, vendorId);

        if ((codecId == A2D_APTX_CODEC_ID_BLUETOOTH) && (vendorId == A2D_APTX_VENDOR_ID)) {
            UINT16 sampleRate = p_codec_caps[6] & A2D_APTX_SAMPLERATE_MSK;
            UINT16 channelMode = p_codec_caps[6] & A2D_APTX_CHANNELS_MSK;
            A2D_ParsAptxInfo (&aptx_cfg_selected ,bta_av_co_cb.codec_cfg->info, FALSE);
            if ((sampleRate & A2D_APTX_SAMPLERATE_48000) &&
                (aptx_cfg_selected.sampleRate & A2D_APTX_SAMPLERATE_48000))
                aptx_cfg_selected.sampleRate = A2D_APTX_SAMPLERATE_48000;
            else if ((sampleRate & A2D_APTX_SAMPLERATE_44100) &&
                (aptx_cfg_selected.sampleRate & A2D_APTX_SAMPLERATE_44100))
                aptx_cfg_selected.sampleRate = A2D_APTX_SAMPLERATE_44100;

            if ((channelMode & A2D_APTX_CHANNELS_STEREO) &&
                (aptx_cfg_selected.channelMode & A2D_APTX_CHANNELS_STEREO))
                aptx_cfg_selected.channelMode = A2D_APTX_CHANNELS_STEREO;
            else if ((channelMode & A2D_APTX_CHANNELS_MONO) &&
                (aptx_cfg_selected.channelMode & A2D_APTX_CHANNELS_MONO))
                aptx_cfg_selected.channelMode = A2D_APTX_CHANNELS_MONO;

            //update with new value
            A2D_BldAptxInfo (AVDT_MEDIA_AUDIO, &aptx_cfg_selected, bta_av_co_cb.codec_cfg->info);
            memcpy(p_codec_cfg, bta_av_co_cb.codec_cfg->info, A2D_APTX_CODEC_LEN+1);
            memcpy(bta_av_co_cb.codec_cfg_aptx.info, bta_av_co_cb.codec_cfg->info,
                A2D_APTX_CODEC_LEN + 1);
            APPL_TRACE_DEBUG("%s aptX",__func__);
        } else if ((codecId == A2D_APTX_HD_CODEC_ID_BLUETOOTH)
            && (vendorId == A2D_APTX_HD_VENDOR_ID)) {
            memcpy(p_codec_cfg, bta_av_co_cb.codec_cfg->info, A2D_APTX_HD_CODEC_LEN+1);
            memcpy(bta_av_co_cb.codec_cfg_aptx_hd.info, bta_av_co_cb.codec_cfg->info,
                A2D_APTX_HD_CODEC_LEN + 1);
            APPL_TRACE_DEBUG("%s aptX HD",__func__);
        }
        break;
    }

    default:
        APPL_TRACE_ERROR("bta_av_co_audio_codec_build_config: unsupported codec id %d", bta_av_co_cb.codec_cfg->id);
        mutex_global_unlock();
        return FALSE;
        break;
    }
    mutex_global_unlock();
    return TRUE;
}

/*******************************************************************************
 **
 ** Function         bta_av_co_audio_codec_cfg_matches_caps
 **
 ** Description      Check if a codec config matches a codec capabilities
 **
 ** Returns          TRUE if it codec config is supported, FALSE otherwise
 **
 *******************************************************************************/
static BOOLEAN bta_av_co_audio_codec_cfg_matches_caps(UINT8 codec_id, const UINT8 *p_codec_caps, const UINT8 *p_codec_cfg)
{
    FUNC_TRACE();

    switch(codec_id)
    {
    case BTIF_AV_CODEC_SBC:

        APPL_TRACE_EVENT("%s: min %d/%d max %d/%d", __func__,
           p_codec_caps[BTA_AV_CO_SBC_MIN_BITPOOL_OFF],
           p_codec_cfg[BTA_AV_CO_SBC_MIN_BITPOOL_OFF],
           p_codec_caps[BTA_AV_CO_SBC_MAX_BITPOOL_OFF],
           p_codec_cfg[BTA_AV_CO_SBC_MAX_BITPOOL_OFF]);

        /* Must match all items exactly except bitpool boundaries which can be adjusted */
        if (!(((p_codec_caps[BTA_AV_CO_SBC_FREQ_CHAN_OFF] & A2D_SBC_IE_SAMP_FREQ_MSK) &
            p_codec_cfg[BTA_AV_CO_SBC_FREQ_CHAN_OFF] & A2D_SBC_IE_SAMP_FREQ_MSK) &&
            ((p_codec_caps[BTA_AV_CO_SBC_FREQ_CHAN_OFF] & A2D_SBC_IE_CH_MD_MSK) &
            p_codec_cfg[BTA_AV_CO_SBC_FREQ_CHAN_OFF] & A2D_SBC_IE_CH_MD_MSK) &&
            ((p_codec_caps[BTA_AV_CO_SBC_BLOCK_BAND_OFF] & A2D_SBC_IE_BLOCKS_MSK) &
            p_codec_cfg[BTA_AV_CO_SBC_BLOCK_BAND_OFF] & A2D_SBC_IE_BLOCKS_MSK) &&
            ((p_codec_caps[BTA_AV_CO_SBC_BLOCK_BAND_OFF] & A2D_SBC_IE_SUBBAND_MSK) &
            p_codec_cfg[BTA_AV_CO_SBC_BLOCK_BAND_OFF] & A2D_SBC_IE_SUBBAND_MSK) &&
            ((p_codec_caps[BTA_AV_CO_SBC_BLOCK_BAND_OFF] & A2D_SBC_IE_ALLOC_MD_MSK) &
            p_codec_cfg[BTA_AV_CO_SBC_BLOCK_BAND_OFF] & A2D_SBC_IE_ALLOC_MD_MSK)))
        {
            APPL_TRACE_EVENT("%s: FALSE %x %x %x %x", __func__,
                    p_codec_caps[BTA_AV_CO_SBC_FREQ_CHAN_OFF],
                    p_codec_cfg[BTA_AV_CO_SBC_FREQ_CHAN_OFF],
                    p_codec_caps[BTA_AV_CO_SBC_BLOCK_BAND_OFF],
                    p_codec_cfg[BTA_AV_CO_SBC_BLOCK_BAND_OFF]);
            return FALSE;
        }
        break;
#if defined(AAC_ENCODER_INCLUDED) && (AAC_ENCODER_INCLUDED == TRUE)
        /* in case of Sink we have to match if Src Cap is a subset of ours */
    case BTA_AV_CODEC_M24:
    {
        tBTIF_AV_CODEC_INFO *p_local_aac_cfg = (tBTIF_AV_CODEC_INFO*)p_codec_cfg;
        tA2D_AAC_CIE p_local_aac_caps;
        tA2D_AAC_CIE p_snk_aac_caps;

        if (A2D_ParsAacInfo(&p_local_aac_caps, (UINT8*)p_local_aac_cfg, FALSE) != A2D_SUCCESS) {
            APPL_TRACE_ERROR("%s: A2D_BldAacInfo: LOCAL failed", __func__);
        }
        if (A2D_ParsAacInfo(&p_snk_aac_caps, (UINT8*)p_codec_cfg, FALSE) != A2D_SUCCESS) {
            APPL_TRACE_ERROR("%s: A2D_BldAacInfo: SNK failed", __func__);
        }

        APPL_TRACE_EVENT("AAC obj_type: snk %x local %x",
                         p_snk_aac_caps.object_type, p_local_aac_caps.object_type);
        APPL_TRACE_EVENT("AAC samp_freq: snk %x local %x",
                         p_snk_aac_caps.samp_freq, p_local_aac_caps.samp_freq);
        APPL_TRACE_EVENT("AAC channels: snk %x local %x",
                         p_snk_aac_caps.channels, p_local_aac_caps.channels);
        APPL_TRACE_EVENT("AAC bit_rate: snk %x local %x",
                         p_snk_aac_caps.bit_rate, p_local_aac_caps.bit_rate);
        APPL_TRACE_EVENT("AAC vbr: snk %x local %x",
                         p_snk_aac_caps.vbr, p_local_aac_caps.vbr);
        return (((p_snk_aac_caps.object_type)&(p_local_aac_caps.object_type))&&
               ((p_snk_aac_caps.samp_freq)&(p_local_aac_caps.samp_freq))&&
               ((p_snk_aac_caps.channels)&(p_local_aac_caps.channels)));
    }
        break;
#endif
    case A2D_NON_A2DP_MEDIA_CT:
    {
        UINT16 codecId;
        UINT32 vendorId;
        UINT8* aptx_capabilities;

        APPL_TRACE_DEBUG("%s aptX", __func__);
        aptx_capabilities = &(((tBTA_AV_CO_SINK*)p_codec_cfg)->codec_caps[0]);
        codecId = ((tA2D_APTX_CIE*)p_codec_caps)->codecId;
        vendorId = ((tA2D_APTX_CIE*)p_codec_caps)->vendorId;
        APPL_TRACE_DEBUG("%s codecId = %d", __func__, codecId);
        APPL_TRACE_DEBUG("%s vendorId = %x", __func__, vendorId);

        int i = 0;
        for (i = 0 ; i < AVDT_CODEC_SIZE; i++)
            APPL_TRACE_DEBUG("%s p_codec_cfg[%d]: %x", __func__, i,  p_codec_cfg[i]);

        APPL_TRACE_EVENT("%s Caps -> sample rate/channel mode: %x configured %x", __func__, p_codec_caps[6], p_codec_cfg[9]);

        if (((vendorId != ((tA2D_APTX_CIE*)(aptx_capabilities))->vendorId) || /*vendor id*/
            (codecId !=  ((tA2D_APTX_CIE*)(aptx_capabilities))->codecId) || /*codec id*/
            (((p_codec_caps[6] & A2D_APTX_SAMPLERATE_MSK) &
            (p_codec_cfg[9] & A2D_APTX_SAMPLERATE_MSK)) == 0 ) || /*sampling rate */
             (((p_codec_caps[6] & A2D_APTX_CHANNELS_MSK) &
             (p_codec_cfg[9] & A2D_APTX_CHANNELS_MSK)) == 0)
           ))
        {
            APPL_TRACE_DEBUG("%s aptX config don't match", __func__);
            APPL_TRACE_EVENT("%s Caps -> vendor id: %x %x %x %x", __func__, p_codec_caps[0], p_codec_caps[1], p_codec_caps[2], p_codec_caps[3]);
            APPL_TRACE_EVENT("%s Configured: %x %x %x %x", __func__, p_codec_cfg[3], p_codec_cfg[4], p_codec_cfg[5], p_codec_cfg[6]);
            APPL_TRACE_EVENT("%s Caps -> codec id: %x", __func__, p_codec_caps[4]);
            APPL_TRACE_EVENT("%s Configured: %x ", __func__, p_codec_cfg[7]);
            APPL_TRACE_EVENT("%s Caps -> Sample Rate/Channel Mode: %x Configured: %x", __func__, p_codec_caps[6], p_codec_cfg[9]);
            return FALSE;
        }
        break;
    }
    default:
        APPL_TRACE_ERROR("bta_av_co_audio_codec_cfg_matches_caps: unsupported codec id %d", codec_id);
        return FALSE;
        break;
    }
    APPL_TRACE_EVENT("TRUE");

    return TRUE;
}

/*******************************************************************************
 **
 ** Function         bta_av_co_audio_codec_match
 **
 ** Description      Check if a codec capabilities supports the codec config
 **
 ** Returns          TRUE if the connection supports this codec, FALSE otherwise
 **
 *******************************************************************************/
static BOOLEAN bta_av_co_audio_codec_match(const UINT8 *p_codec_caps, UINT8 codec_id)
{
    FUNC_TRACE();

    switch(codec_id)
    {
      case BTIF_AV_CODEC_SBC:
        return bta_av_co_audio_codec_cfg_matches_caps(bta_av_co_cb.codec_cfg_sbc.id, p_codec_caps, bta_av_co_cb.codec_cfg_sbc.info);
        break;
#if defined(AAC_ENCODER_INCLUDED) && (AAC_ENCODER_INCLUDED == TRUE)
      case BTIF_AV_CODEC_M24:
        return bta_av_co_audio_codec_cfg_matches_caps(bta_av_co_cb.codec_cfg_aac.id, p_codec_caps, bta_av_co_cb.codec_cfg_aac.info);
        break;
#endif
     case A2D_NON_A2DP_MEDIA_CT:
        {
            UINT16 codecId;
            UINT32 vendorId;
            int i = 0;
            for (i = 0 ; i < AVDT_CODEC_SIZE; i++)
                APPL_TRACE_DEBUG("%s p_codec_caps[%d]: %x", __func__, i,  p_codec_caps[i]);

            codecId = ((tA2D_APTX_CIE*)p_codec_caps)->codecId;
            vendorId = ((tA2D_APTX_CIE*)p_codec_caps)->vendorId;
            APPL_TRACE_DEBUG("%s codecId = %d ", __func__, codecId);
            APPL_TRACE_DEBUG("%s vendorId = %x ", __func__, vendorId);

            if (codecId ==  A2D_APTX_CODEC_ID_BLUETOOTH && vendorId == A2D_APTX_VENDOR_ID) {
                /* aptX Classic */
                APPL_TRACE_DEBUG("%s aptX", __func__);
                return bta_av_co_audio_codec_cfg_matches_caps(bta_av_co_cb.codec_cfg_aptx.id, p_codec_caps, bta_av_co_cb.codec_cfg_aptx.info);
                break;
            } else if (codecId ==  A2D_APTX_HD_CODEC_ID_BLUETOOTH && vendorId == A2D_APTX_HD_VENDOR_ID) {
                /* aptX HD */
                APPL_TRACE_DEBUG("%s aptX HD", __func__);
                return bta_av_co_audio_codec_cfg_matches_caps(bta_av_co_cb.codec_cfg_aptx_hd.id, p_codec_caps, bta_av_co_cb.codec_cfg_aptx_hd.info);
                break;
            } else {
                APPL_TRACE_ERROR("%s incorrect codecId (%d)", __func__, codecId);
                APPL_TRACE_ERROR("%s incorrect vendorId (%x)", __func__, vendorId);
                break;
            }
        }
    default:
        return bta_av_co_audio_codec_cfg_matches_caps(bta_av_co_cb.codec_cfg_sbc.id, p_codec_caps, bta_av_co_cb.codec_cfg_sbc.info);
        break;
    }
    return TRUE;
}

/*******************************************************************************
 **
 ** Function         bta_av_co_audio_peer_reset_config
 **
 ** Description      Reset the peer codec configuration
 **
 ** Returns          Nothing
 **
 *******************************************************************************/
static void bta_av_co_audio_peer_reset_config(tBTA_AV_CO_PEER *p_peer)
{
    FUNC_TRACE();

    /* Indicate that there is no currently selected sink */
    p_peer->p_snk = NULL;
}

/*******************************************************************************
 **
 ** Function         bta_av_co_cp_is_scmst
 **
 ** Description      Check if a content protection service is SCMS-T
 **
 ** Returns          TRUE if this CP is SCMS-T, FALSE otherwise
 **
 *******************************************************************************/
static BOOLEAN bta_av_co_cp_is_scmst(const UINT8 *p_protectinfo)
{
    UINT16 cp_id;
    FUNC_TRACE();

    if (*p_protectinfo >= BTA_AV_CP_LOSC)
    {
        p_protectinfo++;
        STREAM_TO_UINT16(cp_id, p_protectinfo);
        if (cp_id == BTA_AV_CP_SCMS_T_ID)
        {
            APPL_TRACE_DEBUG("bta_av_co_cp_is_scmst: SCMS-T found");
            return TRUE;
        }
    }

    return FALSE;
}

/*******************************************************************************
 **
 ** Function         bta_av_co_audio_sink_has_scmst
 **
 ** Description      Check if a sink supports SCMS-T
 **
 ** Returns          TRUE if the sink supports this CP, FALSE otherwise
 **
 *******************************************************************************/
static BOOLEAN bta_av_co_audio_sink_has_scmst(const tBTA_AV_CO_SINK *p_sink)
{
    UINT8 index;
    const UINT8 *p;
    FUNC_TRACE();

    /* Check if sink supports SCMS-T */
    index = p_sink->num_protect;
    p = &p_sink->protect_info[0];

    while (index)
    {
        if (bta_av_co_cp_is_scmst(p))
        {
            return TRUE;
        }
        /* Move to the next SC */
        p += *p + 1;
        /* Decrement the SC counter */
        index--;
    }
    APPL_TRACE_DEBUG("bta_av_co_audio_sink_has_scmst: SCMS-T not found");
    return FALSE;
}

/*******************************************************************************
 **
 ** Function         bta_av_co_audio_sink_supports_cp
 **
 ** Description      Check if a sink supports the current content protection
 **
 ** Returns          TRUE if the sink supports this CP, FALSE otherwise
 **
 *******************************************************************************/
static BOOLEAN bta_av_co_audio_sink_supports_cp(const tBTA_AV_CO_SINK *p_sink)
{
    FUNC_TRACE();

    /* Check if content protection is enabled for this stream */
    if (bta_av_co_cp_get_flag() != BTA_AV_CP_SCMS_COPY_FREE)
    {
        return bta_av_co_audio_sink_has_scmst(p_sink);
    }
    else
    {
        APPL_TRACE_DEBUG("bta_av_co_audio_sink_supports_cp: not required");
        return TRUE;
    }
}

/*******************************************************************************
 **
 ** Function         bta_av_co_audio_peer_supports_codec
 **
 ** Description      Check if a connection supports the codec config
 **
 ** Returns          TRUE if the connection supports this codec, FALSE otherwise
 **
 *******************************************************************************/
static BOOLEAN bta_av_co_audio_peer_supports_codec(tBTA_AV_CO_PEER *p_peer, UINT8 *p_snk_index, UINT8 *p_codec_type)
{
    int index;
    UINT8 codec_type;
    int preference_index = 0;
    bt_bdaddr_t remote_bdaddr;
    bdcpy(remote_bdaddr.address, p_peer->addr);

    FUNC_TRACE();

    /* Configure the codec type to look for */
    if (p_codec_type != NULL)
    {
        APPL_TRACE_DEBUG("%s Incoming codec_type = %d", __func__, *p_codec_type);
    }
    pthread_mutex_lock(&src_codec_q_lock);
    do
    {
        /* Configure the codec type to look for */
        codec_type = p_bta_av_codec_pri_list[preference_index].codec_type;
        for (index = 0; index < p_peer->num_sup_snks; index++) {
            APPL_TRACE_DEBUG("%s source preferred_type = %d  sink_codec_type = %d",
                __func__, codec_type, p_peer->snks[index].codec_type);
            switch (codec_type)
            {
                case BTIF_AV_CODEC_SBC:
                if (((p_codec_type != NULL) && (p_peer->snks[index].codec_type == *p_codec_type)) ||
                    ((p_codec_type == NULL) && (p_peer->snks[index].codec_type == codec_type ||
                    p_peer->snks[index].codec_type == bta_av_co_cb.codec_cfg_sbc.id)))
                    {
                        APPL_TRACE_DEBUG("%s SBC", __func__);
                        bta_av_co_cb.codec_cfg_sbc.id =
                            p_bta_av_codec_pri_list[preference_index].codec_type;
                        A2D_BldSbcInfo(A2D_MEDIA_TYPE_AUDIO,
                            (tA2D_SBC_CIE *)&p_bta_av_codec_pri_list[preference_index]
                            .codec_cap.sbc_caps, bta_av_co_cb.codec_cfg_sbc.info);
                        if (bta_av_co_audio_codec_match(p_peer->snks[index].codec_caps,
                            BTIF_AV_CODEC_SBC))
                        {
#if  defined(BTA_AV_CO_CP_SCMS_T) && (BTA_AV_CO_CP_SCMS_T == TRUE)
                            if (bta_av_co_audio_sink_has_scmst(&p_peer->snks[index]))
#endif
                            {
                                bta_av_co_cb.current_codec_id = bta_av_co_cb.codec_cfg_sbc.id;
                                /* Protect access to bta_av_co_cb.codec_cfg */
                                mutex_global_lock();
                                bta_av_co_cb.codec_cfg = &bta_av_co_cb.codec_cfg_sbc;
                                mutex_global_unlock();
                                APPL_TRACE_DEBUG("%s SBC matched", __func__);
                                if (p_snk_index) *p_snk_index = index;
                                pthread_mutex_unlock(&src_codec_q_lock);
                                return TRUE;
                            }
                        }
                    }
                break;
                case BTIF_AV_CODEC_M24:
#if defined(AAC_ENCODER_INCLUDED) && (AAC_ENCODER_INCLUDED == TRUE)
                if (bt_split_a2dp_enabled && btif_av_is_codec_offload_supported(AAC) &&
                      !interop_match_addr(INTEROP_DISABLE_AAC_CODEC, &remote_bdaddr)) {
                        APPL_TRACE_DEBUG("%s AAC: index: %d, codec_type: %d", __func__, index,
                            p_peer->snks[index].codec_type);
                   if (((p_codec_type != NULL) && (p_peer->snks[index].codec_type == *p_codec_type))
                       || ((p_codec_type == NULL) && (p_peer->snks[index].codec_type == codec_type
                       || p_peer->snks[index].codec_type == bta_av_co_cb.codec_cfg_aac.id)))
                   {
                        APPL_TRACE_DEBUG("%s AAC", __func__);
                        bta_av_co_cb.codec_cfg_aac.id =
                            p_bta_av_codec_pri_list[preference_index].codec_type;
                        A2D_BldAacInfo(A2D_MEDIA_TYPE_AUDIO,
                            (tA2D_AAC_CIE *)&p_bta_av_codec_pri_list[preference_index]
                            .codec_cap.aac_caps, bta_av_co_cb.codec_cfg_aac.info);
                        if (bta_av_co_audio_codec_match(p_peer->snks[index].codec_caps,
                            BTIF_AV_CODEC_M24))
                        {
#if  defined(BTA_AV_CO_CP_SCMS_T) && (BTA_AV_CO_CP_SCMS_T == TRUE)
                           if (bta_av_co_audio_sink_has_scmst(&p_peer->snks[index]))
#endif
                            {
                                bta_av_co_cb.current_codec_id = bta_av_co_cb.codec_cfg_aac.id;
                                /* Protect access to bta_av_co_cb.codec_cfg */
                                mutex_global_lock();
                                bta_av_co_cb.codec_cfg = &bta_av_co_cb.codec_cfg_aac;
                                mutex_global_unlock();
                                APPL_TRACE_DEBUG("%s AAC matched", __func__);
                                if (p_snk_index) *p_snk_index = index;
                                pthread_mutex_unlock(&src_codec_q_lock);
                                return TRUE;
                            }
                        }
                    }
                } else
                    APPL_TRACE_DEBUG("%s AAC is disabled", __func__);
#endif
                break;
                case A2D_NON_A2DP_MEDIA_CT:
                if ((!bt_split_a2dp_enabled && isA2dAptXEnabled &&
                    (btif_av_is_multicast_supported() == FALSE)) ||
                    (bt_split_a2dp_enabled && (btif_av_is_codec_offload_supported(APTX)||
                    btif_av_is_codec_offload_supported(APTXHD))))
                {
                    UINT16 codecId;
                    UINT32 vendorId;
                    UINT8* aptx_capabilities;

                    if ((bt_split_a2dp_enabled && btif_av_is_codec_offload_supported(APTXHD)) ||
                        isA2dAptXHdEnabled) {
                        if (((p_codec_type != NULL) && (p_peer->snks[index].codec_type ==
                            *p_codec_type)) || ((p_codec_type == NULL) &&
                            (p_peer->snks[index].codec_type == codec_type)))
                        {
                            aptx_capabilities = &(p_peer->snks[index].codec_caps[0]);
                            codecId = ((tA2D_APTX_HD_CIE*)aptx_capabilities)->codecId;
                            vendorId = ((tA2D_APTX_HD_CIE*)aptx_capabilities)->vendorId;
                            int i = 0;
                            for ( i = 0 ; i < AVDT_CODEC_SIZE; i++) {
                                APPL_TRACE_DEBUG("%s codec_caps[%d]: %x", __func__, i,
                                    p_peer->snks[index].codec_caps[i]);
                            }
                            APPL_TRACE_DEBUG("%s codecId = %d", __func__, codecId);
                            APPL_TRACE_DEBUG("%s vendorId = %x", __func__, vendorId);
                            APPL_TRACE_DEBUG("%s p_peer->snks[index].codec_type = %x", __func__,
                                p_peer->snks[index].codec_type );

                            if (codecId ==  A2D_APTX_HD_CODEC_ID_BLUETOOTH && vendorId ==
                                A2D_APTX_HD_VENDOR_ID)
                            {
                                APPL_TRACE_DEBUG("%s aptX HD", __func__);

                                bta_av_co_cb.codec_cfg_aptx_hd.id =
                                    p_bta_av_codec_pri_list[preference_index].codec_type;
                                A2D_BldAptx_hdInfo(A2D_MEDIA_TYPE_AUDIO,
                                    (tA2D_APTX_HD_CIE *)&p_bta_av_codec_pri_list[preference_index]
                                    .codec_cap.aptx_hd_caps, bta_av_co_cb.codec_cfg_aptx_hd.info);
                                if (bta_av_co_audio_codec_match(p_peer->snks[index].codec_caps,
                                    A2D_NON_A2DP_MEDIA_CT))
                                {
            #if defined(BTA_AV_CO_CP_SCMS_T) && (BTA_AV_CO_CP_SCMS_T == TRUE)
                                    if (bta_av_co_audio_sink_has_scmst(&p_peer->snks[index]))
            #endif
                                    {
                                        bta_av_co_cb.current_codec_id =
                                            bta_av_co_cb.codec_cfg_aptx_hd.id;
                                        /* Protect access to bta_av_co_cb.codec_cfg */
                                        mutex_global_lock();
                                        bta_av_co_cb.codec_cfg = &bta_av_co_cb.codec_cfg_aptx_hd;
                                        mutex_global_unlock();
                                        APPL_TRACE_DEBUG("%s aptX HD matched", __func__);
                                        if (p_snk_index) *p_snk_index = index;
                                        pthread_mutex_unlock(&src_codec_q_lock);
                                        return TRUE;
                                    }
                                }
                            }
                        }
                    } else
                            APPL_TRACE_DEBUG("%s aptXHD is disabled", __func__);
                    if ((bt_split_a2dp_enabled && btif_av_is_codec_offload_supported(APTX)) ||
                        isA2dAptXEnabled) {
                        if (((p_codec_type != NULL) && (p_peer->snks[index].codec_type ==
                            *p_codec_type)) ||
                         ((p_codec_type == NULL) && (p_peer->snks[index].codec_type == codec_type)))
                        {
                            aptx_capabilities = &(p_peer->snks[index].codec_caps[0]);
                            codecId = ((tA2D_APTX_CIE*)aptx_capabilities)->codecId;
                            vendorId = ((tA2D_APTX_CIE*)aptx_capabilities)->vendorId;
                            int i = 0;
                            for ( i = 0 ; i < AVDT_CODEC_SIZE; i++) {
                              APPL_TRACE_DEBUG("%s codec_caps[%d]: %x", __func__, i,
                                p_peer->snks[index].codec_caps[i]);
                            }
                            APPL_TRACE_DEBUG("%s codecId = %d", __func__, codecId);
                            APPL_TRACE_DEBUG("%s vendorId = %x", __func__, vendorId);
                            APPL_TRACE_DEBUG("%s p_peer->snks[index].codec_type = %x", __func__,
                                p_peer->snks[index].codec_type );

                            if (codecId ==  A2D_APTX_CODEC_ID_BLUETOOTH && vendorId ==
                                A2D_APTX_VENDOR_ID)
                            {
                                 APPL_TRACE_DEBUG("%s aptX", __func__);

                                bta_av_co_cb.codec_cfg_aptx.id =
                                    p_bta_av_codec_pri_list[preference_index].codec_type;
                                A2D_BldAptxInfo(A2D_MEDIA_TYPE_AUDIO,
                                    (tA2D_APTX_CIE *)&p_bta_av_codec_pri_list[preference_index]
                                    .codec_cap.aptx_caps, bta_av_co_cb.codec_cfg_aptx.info);
                                if (bta_av_co_audio_codec_match(p_peer->snks[index].codec_caps,
                                    A2D_NON_A2DP_MEDIA_CT))
                                {
#if defined(BTA_AV_CO_CP_SCMS_T) && (BTA_AV_CO_CP_SCMS_T == TRUE)
                                    if (bta_av_co_audio_sink_has_scmst(&p_peer->snks[index]))
#endif
                                    {
                                        bta_av_co_cb.current_codec_id =
                                            bta_av_co_cb.codec_cfg_aptx.id;
                                        /* Protect access to bta_av_co_cb.codec_cfg */
                                        mutex_global_lock();
                                        bta_av_co_cb.codec_cfg = &bta_av_co_cb.codec_cfg_aptx;
                                        mutex_global_unlock();
                                        APPL_TRACE_DEBUG("%s aptX matched", __func__);
                                        if (p_snk_index) *p_snk_index = index;
                                        pthread_mutex_unlock(&src_codec_q_lock);
                                        return TRUE;
                                    }
                                }
                            }
                        }
                    } else
                        APPL_TRACE_DEBUG("%s aptX is disabled", __func__);
                }
                break;

                default:
                APPL_TRACE_ERROR("bta_av_co_audio_peer_supports_codec: unsupported codec id %d",
                    codec_type);
                pthread_mutex_unlock(&src_codec_q_lock);
                return FALSE;
                break;
            }
        }
        preference_index ++;
        APPL_TRACE_DEBUG(" preferred codec index = %d ", preference_index);
    } while (preference_index < bta_av_num_codec_configs);
    pthread_mutex_unlock(&src_codec_q_lock);
    return FALSE;
}

/*******************************************************************************
 **
 ** Function         bta_av_co_audio_peer_src_supports_codec
 **
 ** Description      Check if a peer acting as src supports codec config
 **
 ** Returns          TRUE if the connection supports this codec, FALSE otherwise
 **
 *******************************************************************************/
static BOOLEAN bta_av_co_audio_peer_src_supports_codec(tBTA_AV_CO_PEER *p_peer, UINT8 *p_src_index)
{
    int index;
    UINT8 codec_type;
    FUNC_TRACE();

    /* Configure the codec type to look for */
    codec_type = bta_av_co_cb.codec_cfg->id;


    for (index = 0; index < p_peer->num_sup_srcs; index++)
    {
        if (p_peer->srcs[index].codec_type == codec_type)
        {
            switch (bta_av_co_cb.codec_cfg->id)
            {
            case BTIF_AV_CODEC_SBC:
                if (p_src_index) *p_src_index = index;
                if (0 ==  bta_av_sbc_cfg_matches_cap((UINT8 *)p_peer->srcs[index].codec_caps,
                                                     (tA2D_SBC_CIE *)&bta_av_co_sbc_sink_caps))
                {
                    return TRUE;
                }
                break;

            default:
                APPL_TRACE_ERROR("peer_src_supports_codec: unsupported codec id %d",
                                                            bta_av_co_cb.codec_cfg->id);
                return FALSE;
                break;
            }
        }
    }
    return FALSE;
}

/*******************************************************************************
 **
 ** Function         bta_av_co_audio_sink_supports_config
 **
 ** Description      Check if the media source supports a given configuration
 **
 ** Returns          TRUE if the media source supports this config, FALSE otherwise
 **
 *******************************************************************************/
static BOOLEAN bta_av_co_audio_sink_supports_config(UINT8 codec_type, const UINT8 *p_codec_cfg)
{
    FUNC_TRACE();

    switch (codec_type)
    {
    case BTA_AV_CODEC_SBC:
        if (bta_av_sbc_cfg_in_cap((UINT8 *)p_codec_cfg, (tA2D_SBC_CIE *)&bta_av_co_sbc_sink_caps))
        {
            return FALSE;
        }
        break;


    default:
        APPL_TRACE_ERROR("bta_av_co_audio_media_supports_config unsupported codec type %d", codec_type);
        return FALSE;
        break;
    }
    return TRUE;
}

/*******************************************************************************
 **
 ** Function         bta_av_co_audio_media_supports_config
 **
 ** Description      Check if the media sink supports a given configuration
 **
 ** Returns          TRUE if the media source supports this config, FALSE otherwise
 **
 *******************************************************************************/
static BOOLEAN bta_av_co_audio_media_supports_config(UINT8 codec_type, const UINT8 *p_codec_cfg)
{
    FUNC_TRACE();

    APPL_TRACE_DEBUG("%s codec_type = %x", __func__, codec_type);

    UINT16 codecId;
    UINT32 vendorId;
    UINT8* aptx_capabilities;

    switch (codec_type)
    {
    case BTA_AV_CODEC_SBC:
        pthread_mutex_lock(&src_codec_q_lock);
        if (bta_av_sbc_cfg_in_cap((UINT8 *)p_codec_cfg,
            (tA2D_SBC_CIE *)&bta_av_supp_codec_cap[BTIF_SV_AV_AA_SBC_INDEX]
            .codec_cap.sbc_caps))
        {
            APPL_TRACE_DEBUG("%s SBC ",__func__);
            pthread_mutex_unlock(&src_codec_q_lock);
            return FALSE;
        }
        pthread_mutex_unlock(&src_codec_q_lock);
        break;
#if defined(AAC_ENCODER_INCLUDED) && (AAC_ENCODER_INCLUDED == TRUE)
    case BTA_AV_CODEC_M24:
        pthread_mutex_lock(&src_codec_q_lock);
        if (bta_av_aac_cfg_in_cap((UINT8 *)p_codec_cfg,
            (tA2D_AAC_CIE *)&bta_av_supp_codec_cap[BTIF_SV_AV_AA_AAC_INDEX]
            .codec_cap.aac_caps))
        {
            APPL_TRACE_DEBUG("%s AAC ",__func__);
            pthread_mutex_unlock(&src_codec_q_lock);
            return FALSE;
        }
        pthread_mutex_unlock(&src_codec_q_lock);
        break;
#endif
    case A2D_NON_A2DP_MEDIA_CT:
        aptx_capabilities = &(((tBTA_AV_CO_SINK*)p_codec_cfg)->codec_caps[0]);
        codecId = ((tA2D_APTX_CIE*)(aptx_capabilities))->codecId;
        vendorId = ((tA2D_APTX_CIE*)(aptx_capabilities))->vendorId;
        APPL_TRACE_DEBUG("%s codecId = %d ", __func__, codecId);
        APPL_TRACE_DEBUG("%s vendorId = %x ", __func__, vendorId);

        if (codecId ==  A2D_APTX_HD_CODEC_ID_BLUETOOTH && vendorId == A2D_APTX_HD_VENDOR_ID) {
            APPL_TRACE_DEBUG("%s tA2D_APTX_CIE aptX HD", __func__);
            pthread_mutex_lock(&src_codec_q_lock);
            if (a2d_av_aptx_hd_cfg_in_cap((UINT8 *)p_codec_cfg,
                (tA2D_APTX_HD_CIE *)&bta_av_supp_codec_cap[BTIF_SV_AV_AA_APTX_HD_INDEX]
                .codec_cap.aptx_hd_caps)) {
                APPL_TRACE_DEBUG("%s aptX HD", __func__);
                pthread_mutex_unlock(&src_codec_q_lock);
                return FALSE;
            }
            pthread_mutex_unlock(&src_codec_q_lock);
            break;
        } else if (codecId ==  A2D_APTX_CODEC_ID_BLUETOOTH && vendorId == A2D_APTX_VENDOR_ID) {
            APPL_TRACE_DEBUG("%s tA2D_APTX_CIE aptX", __func__);
            pthread_mutex_lock(&src_codec_q_lock);
            if (a2d_av_aptx_cfg_in_cap((UINT8 *)p_codec_cfg,
                (tA2D_APTX_CIE *)&bta_av_supp_codec_cap[BTIF_SV_AV_AA_APTX_INDEX]
                .codec_cap.aptx_caps)) {
                APPL_TRACE_DEBUG("%s aptX", __func__);
                pthread_mutex_unlock(&src_codec_q_lock);
                return FALSE;
            }
            pthread_mutex_unlock(&src_codec_q_lock);
            break;
        }
    default:
        APPL_TRACE_ERROR("bta_av_co_audio_media_supports_config unsupported codec type %d", codec_type);
        return FALSE;
        break;
    }
    return TRUE;
}

/*******************************************************************************
 **
 ** Function         bta_av_co_audio_codec_supported
 **
 ** Description      Check if all opened connections are compatible with a codec
 **                  configuration and content protection
 **
 ** Returns          TRUE if all opened devices support this codec, FALSE otherwise
 **
 *******************************************************************************/
BOOLEAN bta_av_co_audio_codec_supported(tBTIF_STATUS *p_status)
{
    UINT8 index;
    UINT8 snk_index;
    tBTA_AV_CO_PEER *p_peer;
    tBTA_AV_CO_SINK *p_sink;
    UINT8 codec_cfg[AVDT_CODEC_SIZE];
    UINT8 num_protect = 0;
#if defined(BTA_AV_CO_CP_SCMS_T) && (BTA_AV_CO_CP_SCMS_T == TRUE)
    BOOLEAN cp_active;
#endif
    UINT16 current_codec_id;
    current_codec_id = bta_av_co_cb.current_codec_id;
    UINT8 p_scb_codec_type = 0;
    FUNC_TRACE();

    APPL_TRACE_DEBUG("bta_av_co_audio_codec_supported");

    /* Check AV feeding is supported */
    *p_status = BTIF_ERROR_SRV_AV_FEEDING_NOT_SUPPORTED;

    for (index = 0; index < BTA_AV_CO_NUM_ELEMENTS(bta_av_co_cb.peers); index++)
    {
        p_peer = &bta_av_co_cb.peers[index];
        if (p_peer->opened)
        {
            if (bta_av_co_audio_peer_supports_codec(p_peer, &snk_index, NULL))
            {
                APPL_TRACE_DEBUG("%s current_codec_id: %x", __func__, bta_av_co_cb.current_codec_id);
                p_scb_codec_type = bta_av_get_codec_type();
                APPL_TRACE_DEBUG("%s p_scb_codec_type: %x", __func__, p_scb_codec_type);
                APPL_TRACE_DEBUG("%s current_sink_index: %x", __func__, snk_index);
                if (bta_av_co_cb.current_codec_id != p_scb_codec_type)
                {
                    APPL_TRACE_WARNING("%s Mismatch found in selected codec and configured codec type", __func__);
                    if (!bta_av_co_audio_peer_supports_codec(p_peer, &snk_index, &p_scb_codec_type))
                    {
                        APPL_TRACE_ERROR("bta_av_co_audio_codec_supported index %d doesn't support codec", index);
                        return FALSE;
                    }
                    APPL_TRACE_WARNING("%s current_sink_index changed to: %x", __func__, snk_index);
                }
                p_sink = &p_peer->snks[snk_index];

                /* Check that this sink is compatible with the CP */
                if (!bta_av_co_audio_sink_supports_cp(p_sink))
                {
                    APPL_TRACE_DEBUG("bta_av_co_audio_codec_supported sink %d of peer %d doesn't support cp",
                            snk_index, index);
                    *p_status = BTIF_ERROR_SRV_AV_CP_NOT_SUPPORTED;
#if defined(BTA_AV_CO_CP_SCMS_T) && (BTA_AV_CO_CP_SCMS_T == TRUE)
                    if (!bta_av_co_audio_codec_build_config(p_sink->codec_caps, codec_cfg))
                    {
                        APPL_TRACE_DEBUG("%s index %d doesn't support codec", __func__, index);
                        return FALSE;
                    }
                    return TRUE;
#else
                    return FALSE;
#endif
                }

                /* Build the codec configuration for this sink */
                if (bta_av_co_audio_codec_build_config(p_sink->codec_caps, codec_cfg))
                {
#if defined(BTA_AV_CO_CP_SCMS_T) && (BTA_AV_CO_CP_SCMS_T == TRUE)
                    /* Check if this sink supports SCMS */
                    cp_active = bta_av_co_audio_sink_has_scmst(p_sink);
#endif
                    APPL_TRACE_DEBUG("%s current_codec_id: %x", __func__, bta_av_co_cb.current_codec_id);
                    APPL_TRACE_DEBUG("%s p_scb_codec_type: %x", __func__, p_scb_codec_type);
                    p_scb_codec_type = bta_av_get_codec_type();
                    APPL_TRACE_DEBUG("%s p_scb_codec_type: %x", __func__, p_scb_codec_type);
                    /* Check if this is a new configuration (new sink or new config) */
                    if ((p_sink != p_peer->p_snk) ||
                        (memcmp(codec_cfg, p_peer->codec_cfg, AVDT_CODEC_SIZE))
#if defined(BTA_AV_CO_CP_SCMS_T) && (BTA_AV_CO_CP_SCMS_T == TRUE)
                        || (p_peer->cp_active != cp_active)
#endif
                        || (current_codec_id != p_scb_codec_type))
                    {
                        /* Save the new configuration */
                        p_peer->p_snk = p_sink;
                        memcpy(p_peer->codec_cfg, codec_cfg, AVDT_CODEC_SIZE);
#if defined(BTA_AV_CO_CP_SCMS_T) && (BTA_AV_CO_CP_SCMS_T == TRUE)
                        p_peer->cp_active = cp_active;
                        if (p_peer->cp_active)
                        {
                            bta_av_co_cb.cp.active = TRUE;
                            num_protect = BTA_AV_CP_INFO_LEN;
                        }
                        else
                        {
                            bta_av_co_cb.cp.active = FALSE;
                        }
#endif
                        APPL_TRACE_DEBUG("bta_av_co_audio_codec_supported call BTA_AvReconfig(x%x)", BTA_AV_CO_AUDIO_INDX_TO_HNDL(index));
                        BTA_AvReconfig(BTA_AV_CO_AUDIO_INDX_TO_HNDL(index), TRUE, p_sink->sep_info_idx,
                                p_peer->codec_cfg, num_protect, (UINT8 *)bta_av_co_cp_scmst);
                    }
                }
            }
            else
            {
                APPL_TRACE_DEBUG("bta_av_co_audio_codec_supported index %d doesn't support codec", index);
                return FALSE;
            }
        }
    }

    *p_status = BTIF_SUCCESS;
    return TRUE;
}

/*******************************************************************************
 **
 ** Function         bta_av_co_audio_codec_reset
 **
 ** Description      Reset the current codec configuration
 **
 ** Returns          void
 **
 *******************************************************************************/
void bta_av_co_audio_codec_reset(void)
{
    UINT16 codecId;
    UINT8 vendorId;

    mutex_global_lock();
    FUNC_TRACE();

    pthread_mutex_lock(&src_codec_q_lock);
    switch (p_bta_av_codec_pri_list[0].codec_type) {
    case BTIF_AV_CODEC_SBC:
        /* Reset the current configuration to SBC */
        bta_av_co_cb.codec_cfg_sbc.id = BTIF_AV_CODEC_SBC;
        if (A2D_BldSbcInfo(A2D_MEDIA_TYPE_AUDIO,
            (tA2D_SBC_CIE *)&p_bta_av_codec_pri_list[0].codec_cap.sbc_caps,
            bta_av_co_cb.codec_cfg_sbc.info) != A2D_SUCCESS)
        {
            APPL_TRACE_ERROR("bta_av_co_audio_codec_reset A2D_BldSbcInfo failed");
        } else
            bta_av_co_cb.codec_cfg = &(bta_av_co_cb.codec_cfg_sbc);
        break;
#if defined(AAC_ENCODER_INCLUDED) && (AAC_ENCODER_INCLUDED == TRUE)
    case BTIF_AV_CODEC_M24:
        /* Reset the current configuration to AAC */
        bta_av_co_cb.codec_cfg_aac.id = BTIF_AV_CODEC_M24;
        if (A2D_BldAacInfo(A2D_MEDIA_TYPE_AUDIO,
            (tA2D_AAC_CIE *)&p_bta_av_codec_pri_list[0].codec_cap.aac_caps,
            bta_av_co_cb.codec_cfg_aac.info) != A2D_SUCCESS)
        {
            APPL_TRACE_ERROR("bta_av_co_audio_codec_reset A2D_BldAacInfo failed");
        } else
            bta_av_co_cb.codec_cfg = &(bta_av_co_cb.codec_cfg_sbc);
#endif
        break;
    case A2D_NON_A2DP_MEDIA_CT:
        codecId = p_bta_av_codec_pri_list[0].codec_cap.aptx_caps.codecId;
        vendorId = p_bta_av_codec_pri_list[0].codec_cap.aptx_caps.vendorId;
        if (codecId ==  A2D_APTX_CODEC_ID_BLUETOOTH && vendorId == A2D_APTX_VENDOR_ID) {
            /* Reset the Current configuration to aptX */
            bta_av_co_cb.codec_cfg_aptx.id = A2D_NON_A2DP_MEDIA_CT;
            if (A2D_BldAptxInfo(A2D_MEDIA_TYPE_AUDIO,
                (tA2D_APTX_CIE *)&p_bta_av_codec_pri_list[0].codec_cap.aptx_caps,
                bta_av_co_cb.codec_cfg_aptx.info) != A2D_SUCCESS)
                APPL_TRACE_ERROR("%s A2D_BldAptxInfo failed", __func__);
        } else if (codecId ==  A2D_APTX_HD_CODEC_ID_BLUETOOTH &&
            vendorId == A2D_APTX_HD_VENDOR_ID) {
            /* Reset the Current configuration to aptX HD */
            bta_av_co_cb.codec_cfg_aptx_hd.id = A2D_NON_A2DP_MEDIA_CT;
            if (A2D_BldAptx_hdInfo(A2D_MEDIA_TYPE_AUDIO,
                (tA2D_APTX_HD_CIE *)&p_bta_av_codec_pri_list[0].codec_cap.aptx_hd_caps,
                bta_av_co_cb.codec_cfg_aptx_hd.info) != A2D_SUCCESS)
                APPL_TRACE_ERROR("%s A2D_BldAptx_hdInfo failed", __func__);
        }
        break;
    }
    pthread_mutex_unlock(&src_codec_q_lock);

    mutex_global_unlock();
}

/*******************************************************************************
 **
 ** Function         bta_av_co_audio_set_codec
 **
 ** Description      Set the current codec configuration from the feeding type.
 **                  This function is starting to modify the configuration, it
 **                  should be protected.
 **
 ** Returns          TRUE if successful, FALSE otherwise
 **
 *******************************************************************************/
BOOLEAN bta_av_co_audio_set_codec(const tBTIF_AV_MEDIA_FEEDINGS *p_feeding, tBTIF_STATUS *p_status)
{
    tA2D_SBC_CIE sbc_config;
    tBTIF_AV_CODEC_INFO new_cfg_sbc;
    tA2D_APTX_CIE aptx_config;
    tBTIF_AV_CODEC_INFO new_cfg_aptx;
    tA2D_APTX_HD_CIE aptx_hd_config;
    tBTIF_AV_CODEC_INFO new_cfg_aptx_hd;
#if defined(AAC_ENCODER_INCLUDED) && (AAC_ENCODER_INCLUDED == TRUE)
    tA2D_AAC_CIE aac_config;
    tBTIF_AV_CODEC_INFO new_cfg_aac;
#endif
    FUNC_TRACE();

    /* Check AV feeding is supported */
    *p_status = BTIF_ERROR_SRV_AV_FEEDING_NOT_SUPPORTED;

    APPL_TRACE_DEBUG("bta_av_co_audio_set_codec cid=%d", p_feeding->format);

    /* Supported codecs */
    switch (p_feeding->format)
    {
    case BTIF_AV_CODEC_PCM:
        new_cfg_sbc.id = BTIF_AV_CODEC_SBC;

        sbc_config = btif_av_sbc_default_config;
        if ((p_feeding->cfg.pcm.num_channel != 1) &&
            (p_feeding->cfg.pcm.num_channel != 2))
        {
            APPL_TRACE_ERROR("bta_av_co_audio_set_codec PCM channel number unsupported");
            return FALSE;
        }
        if ((p_feeding->cfg.pcm.bit_per_sample != 8) &&
            (p_feeding->cfg.pcm.bit_per_sample != 16) &&
            (p_feeding->cfg.pcm.bit_per_sample != 32))
        {
            APPL_TRACE_ERROR("bta_av_co_audio_set_codec PCM sample size unsupported");
            return FALSE;
        }
        new_cfg_aptx.id = A2D_NON_A2DP_MEDIA_CT;
        aptx_config = btif_av_aptx_default_config;
        new_cfg_aptx_hd.id = A2D_NON_A2DP_MEDIA_CT;
        aptx_hd_config = btif_av_aptx_hd_default_config;
#if defined(AAC_ENCODER_INCLUDED) && (AAC_ENCODER_INCLUDED == TRUE)
        new_cfg_aac.id = BTIF_AV_CODEC_M24;
        aac_config = btif_av_aac_default_config;
#endif
        switch (p_feeding->cfg.pcm.sampling_freq)
        {
        case 8000:
        case 12000:
        case 16000:
        case 24000:
        case 32000:
        case 48000:
            sbc_config.samp_freq = A2D_SBC_IE_SAMP_FREQ_48;
            aptx_config.sampleRate = A2D_APTX_SAMPLERATE_48000;
            aptx_hd_config.sampleRate = A2D_APTX_HD_SAMPLERATE_48000;
#if defined(AAC_ENCODER_INCLUDED) && (AAC_ENCODER_INCLUDED == TRUE)
            aac_config.samp_freq = A2D_AAC_IE_SAMP_FREQ_48000;
#endif
            break;

        case 11025:
        case 22050:
        case 44100:
            sbc_config.samp_freq = A2D_SBC_IE_SAMP_FREQ_44;
            aptx_config.sampleRate = A2D_APTX_SAMPLERATE_44100;
            aptx_hd_config.sampleRate = A2D_APTX_HD_SAMPLERATE_44100;
#if defined(AAC_ENCODER_INCLUDED) && (AAC_ENCODER_INCLUDED == TRUE)
            aac_config.samp_freq = A2D_AAC_IE_SAMP_FREQ_44100;
#endif
            break;
        default:
            APPL_TRACE_ERROR("bta_av_co_audio_set_codec PCM sampling frequency unsupported");
            return FALSE;
            break;
        }
        /* Build the codec config */
        if (A2D_BldSbcInfo(A2D_MEDIA_TYPE_AUDIO, &sbc_config, new_cfg_sbc.info) != A2D_SUCCESS)
        {
            APPL_TRACE_ERROR("bta_av_co_audio_set_codec A2D_BldSbcInfo failed");
            return FALSE;
        }
        if (A2D_BldAptxInfo(A2D_MEDIA_TYPE_AUDIO, &aptx_config, new_cfg_aptx.info) != A2D_SUCCESS)
        {
            APPL_TRACE_ERROR("%s A2D_BldAptxInfo failed", __func__);
            return FALSE;
        }
        if (A2D_BldAptx_hdInfo(A2D_MEDIA_TYPE_AUDIO, &aptx_hd_config, new_cfg_aptx_hd.info) != A2D_SUCCESS)
        {
            APPL_TRACE_ERROR("%s A2D_BldAptx_hdInfo failed", __func__);
            return FALSE;
        }
#if defined(AAC_ENCODER_INCLUDED) && (AAC_ENCODER_INCLUDED == TRUE)
        if (A2D_BldAacInfo(A2D_MEDIA_TYPE_AUDIO, &aac_config, new_cfg_aac.info) != A2D_SUCCESS)
        {
            APPL_TRACE_ERROR("%s A2D_BldAacInfo failed", __func__);
            return FALSE;
        }
#endif
        break;


    default:
        APPL_TRACE_ERROR("bta_av_co_audio_set_codec Feeding format unsupported");
        return FALSE;
        break;
    }

    /* The new config was correctly built. The default codec is set to be SBC */
    bta_av_co_cb.codec_cfg_sbc = new_cfg_sbc;
    mutex_global_lock();
    bta_av_co_cb.codec_cfg = &bta_av_co_cb.codec_cfg_sbc;
    mutex_global_unlock();
    bta_av_co_cb.codec_cfg_aptx= new_cfg_aptx;
    bta_av_co_cb.codec_cfg_aptx_hd = new_cfg_aptx_hd;
#if defined(AAC_ENCODER_INCLUDED) && (AAC_ENCODER_INCLUDED == TRUE)
    bta_av_co_cb.codec_cfg_aac = new_cfg_aac;
#endif

    /* Check all devices support it */
    *p_status = BTIF_SUCCESS;
    return bta_av_co_audio_codec_supported(p_status);
}

UINT8 bta_av_select_codec(UINT8 hdl)
{
    if (NULL == bta_av_co_cb.codec_cfg)
    {
        // Some circumstances - bta_av_co functions are called before
        // codec clock is initialised
        APPL_TRACE_ERROR("%s hdl = %d, no codec configured", __func__, hdl);
        return BTIF_AV_CODEC_NONE;
    }
    else
    {
        tBTA_AV_CO_PEER *p_peer;
        tBTA_AV_CO_SINK *p_sink;
        UINT8 codec_cfg[AVDT_CODEC_SIZE];
        UINT8 index;
        APPL_TRACE_ERROR("%s hdl = %d",__func__,hdl);
        /* Retrieve the peer info */
        p_peer = bta_av_co_get_peer(hdl);
        /* Fix for below KW Issue
           Pointer 'p_peer' returned from call to function 'bta_av_co_get_peer' at line
           1993 may be NULL, will be passed to function and may be dereferenced there
           by passing argument 1 to function 'bta_av_co_audio_peer_supports_codec' at
           line 2001.*/
        if (p_peer != NULL)
        {
            /* Find a sink that matches the codec config */
            if (bta_av_co_audio_peer_supports_codec(p_peer, &index, NULL))
            {
                p_sink = &p_peer->snks[index];
                /* Build the codec configuration for this sink */
                bta_av_co_audio_codec_build_config(p_sink->codec_caps, codec_cfg);
            }
        }
        return bta_av_co_cb.codec_cfg->id;
    }
}

UINT8 bta_av_co_get_current_codec()
{
    // Some circumstances - bta_av_co functions are called before codec clock is initialised
    if (NULL == bta_av_co_cb.codec_cfg)
        return BTIF_AV_CODEC_NONE;
    else
        return bta_av_co_cb.codec_cfg->id;
}

UINT8* bta_av_co_get_current_codecInfo()
{
    // We assume that the configuration block is always valid when this is called.
    return &bta_av_co_cb.codec_cfg->info[0];
}

/*******************************************************************************
 **
 ** Function         bta_av_co_audio_get_codec_config
 **
 ** Description      Retrieves the current codec configuration.  In case of failure return
 **                       the default SBC codec configuration.
 **
 ** Returns          TRUE if returned current codec config, FALSE otherwise
 **
 *******************************************************************************/
BOOLEAN bta_av_co_audio_get_codec_config(UINT8 *p_config, UINT16 *p_minmtu, UINT8 type)
{
    BOOLEAN result = FALSE;
    UINT8 index, jndex;
    tBTA_AV_CO_PEER *p_peer;
    tBTA_AV_CO_SINK *p_sink;
    tA2D_SBC_CIE *sbc_config;
#if defined(AAC_ENCODER_INCLUDED) && (AAC_ENCODER_INCLUDED == TRUE)
    tA2D_AAC_CIE *aac_config;
#endif

    APPL_TRACE_EVENT("%s codec 0x%x", __func__, bta_av_co_cb.codec_cfg->id);

    /* Minimum MTU is by default very large */
    *p_minmtu = 0xFFFF;

    mutex_global_lock();
    if (type == BTIF_AV_CODEC_SBC)
    {
        APPL_TRACE_DEBUG("%s SBC", __func__);
        sbc_config = (tA2D_SBC_CIE *)p_config;
        if (A2D_ParsSbcInfo(sbc_config, bta_av_co_cb.codec_cfg_sbc.info, FALSE) == A2D_SUCCESS)
            result = TRUE;
        else
            memcpy((tA2D_SBC_CIE *) p_config, &btif_av_sbc_default_config, sizeof(tA2D_SBC_CIE));
    } else if (type == A2D_NON_A2DP_MEDIA_CT && ((tA2D_APTX_CIE *)p_config)->vendorId == A2D_APTX_VENDOR_ID && ((tA2D_APTX_CIE *)p_config)->codecId == A2D_APTX_CODEC_ID_BLUETOOTH) {
        APPL_TRACE_DEBUG("%s aptX", __func__);
        tA2D_APTX_CIE *aptx_config = (tA2D_APTX_CIE *)p_config;
        if (A2D_ParsAptxInfo(aptx_config, bta_av_co_cb.codec_cfg_aptx.info, FALSE) == A2D_SUCCESS)
            result = TRUE;
        else
            memcpy((tA2D_APTX_CIE *) p_config, &btif_av_aptx_default_config, sizeof(tA2D_APTX_CIE));
    } else if (type == A2D_NON_A2DP_MEDIA_CT && ((tA2D_APTX_HD_CIE *)p_config)->vendorId == A2D_APTX_HD_VENDOR_ID && ((tA2D_APTX_HD_CIE *)p_config)->codecId == A2D_APTX_HD_CODEC_ID_BLUETOOTH) {
        APPL_TRACE_DEBUG("%s aptX HD", __func__);
        tA2D_APTX_HD_CIE *aptx_hd_config = (tA2D_APTX_HD_CIE *)p_config;
        if (A2D_ParsAptx_hdInfo(aptx_hd_config, bta_av_co_cb.codec_cfg_aptx_hd.info, FALSE) == A2D_SUCCESS)
            result = TRUE;
        else
            memcpy((tA2D_APTX_HD_CIE *) p_config, &btif_av_aptx_hd_default_config, sizeof(tA2D_APTX_HD_CIE));
    } else {
        APPL_TRACE_DEBUG("%s vendorId: %d  codecId: %d\n", __func__, ((tA2D_APTX_CIE *)p_config)->vendorId, ((tA2D_APTX_CIE *)p_config)->codecId);
    }
#if defined(AAC_ENCODER_INCLUDED) && (AAC_ENCODER_INCLUDED == TRUE)
    if (type == BTIF_AV_CODEC_M24)
    {
        APPL_TRACE_DEBUG("%s AAC", __func__);
        aac_config = (tA2D_AAC_CIE *)p_config;
        if (A2D_ParsAacInfo(aac_config, bta_av_co_cb.codec_cfg_aac.info, FALSE) == A2D_SUCCESS)
            result = TRUE;
        else
            memcpy((tA2D_AAC_CIE *) p_config, &btif_av_aac_default_config, sizeof(tA2D_AAC_CIE));
    }
#endif
    for (index = 0; index < BTA_AV_CO_NUM_ELEMENTS(bta_av_co_cb.peers); index++)
    {
        p_peer = &bta_av_co_cb.peers[index];
        if (p_peer->opened)
        {
            if (p_peer->mtu < *p_minmtu)
                *p_minmtu = p_peer->mtu;

            for (jndex = 0; jndex < p_peer->num_sup_snks; jndex++)
            {
                p_sink = &p_peer->snks[jndex];
                if (type == BTIF_AV_CODEC_SBC && p_sink->codec_type == A2D_MEDIA_CT_SBC)
                {
                    /* Update the bitpool boundaries of the current config */
                    sbc_config->min_bitpool =
                        BTA_AV_CO_MAX(p_sink->codec_caps[BTA_AV_CO_SBC_MIN_BITPOOL_OFF],
                                      sbc_config->min_bitpool);
                    sbc_config->max_bitpool =
                        BTA_AV_CO_MIN(p_sink->codec_caps[BTA_AV_CO_SBC_MAX_BITPOOL_OFF],
                                      sbc_config->max_bitpool);
                    APPL_TRACE_EVENT("%s : freq %d, chan mode %d "
                                    "block len %d, subbands %d alloc %d "
                                    "bitpool min %d, max %d",
                                __func__, sbc_config->samp_freq, sbc_config->ch_mode,
                                sbc_config->block_len, sbc_config->num_subbands,
                                sbc_config->alloc_mthd, sbc_config->min_bitpool,
                                sbc_config->max_bitpool);
                    break;
                }
            }
        }
    }
    mutex_global_unlock();

    return result;
}

/*******************************************************************************
 **
 ** Function         bta_av_co_audio_get_sbc_config
 **
 ** Description      Retrieves the SBC codec configuration.  If the codec in use
 **                  is not SBC, return the default SBC codec configuration.
 **
 ** Returns          TRUE if codec is SBC, FALSE otherwise
 **
 *******************************************************************************/
BOOLEAN bta_av_co_audio_get_sbc_config(tA2D_SBC_CIE *p_sbc_config, UINT16 *p_minmtu)
{
    BOOLEAN result = FALSE;
    UINT8 index, jndex;
    tBTA_AV_CO_PEER *p_peer;
    tBTA_AV_CO_SINK *p_sink;

    APPL_TRACE_EVENT("bta_av_co_cb.codec_cfg->id : codec 0x%x", bta_av_co_cb.codec_cfg->id);

    /* Minimum MTU is by default very large */
    *p_minmtu = 0xFFFF;

    mutex_global_lock();
    if (bta_av_co_cb.codec_cfg->id == BTIF_AV_CODEC_SBC)
    {
        if (A2D_ParsSbcInfo(p_sbc_config, bta_av_co_cb.codec_cfg->info, FALSE) == A2D_SUCCESS)
        {
            for (index = 0; index < BTA_AV_CO_NUM_ELEMENTS(bta_av_co_cb.peers); index++)
            {
                p_peer = &bta_av_co_cb.peers[index];
                if (p_peer->opened)
                {
                    APPL_TRACE_EVENT("%s on index= %d", __func__, index);
                    if (p_peer->mtu < *p_minmtu)
                    {
                        *p_minmtu = p_peer->mtu;
                    }
                    for (jndex = 0; jndex < p_peer->num_sup_snks; jndex++)
                    {
                        p_sink = &p_peer->snks[jndex];
                        if (p_sink->codec_type == A2D_MEDIA_CT_SBC)
                        {
                            /* Update the bitpool boundaries of the current config */
                            APPL_TRACE_EVENT("%s Update the bitpool boundaries on index= %d", __func__, jndex);
                            p_sbc_config->min_bitpool =
                               BTA_AV_CO_MAX(p_sink->codec_caps[BTA_AV_CO_SBC_MIN_BITPOOL_OFF],
                                             p_sbc_config->min_bitpool);
                            p_sbc_config->max_bitpool =
                               BTA_AV_CO_MIN(p_sink->codec_caps[BTA_AV_CO_SBC_MAX_BITPOOL_OFF],
                                             p_sbc_config->max_bitpool);
                            APPL_TRACE_EVENT("bta_av_co_audio_get_sbc_config : sink bitpool min %d, max %d",
                                 p_sbc_config->min_bitpool, p_sbc_config->max_bitpool);
                            break;
                        }
                    }
                }
            }
            result = TRUE;
        }
    }

    if (!result)
    {
        /* Not SBC, still return the default values */
        APPL_TRACE_EVENT("%s Not SBC, still return the default values", __func__);
        *p_sbc_config = btif_av_sbc_default_config;
    }
    mutex_global_unlock();

    return result;
}
#if defined(AAC_ENCODER_INCLUDED) && (AAC_ENCODER_INCLUDED == TRUE)
/*******************************************************************************
 **
 ** Function         bta_av_co_audio_get_aac_config
 **
 ** Description      Retrieves the AAC codec configuration.  If the codec in use
 **                  is not AAC, return the default AAC codec configuration.
 **
 ** Returns          TRUE if codec is AAC, FALSE otherwise
 **
 *******************************************************************************/
BOOLEAN bta_av_co_audio_get_aac_config(tA2D_AAC_CIE *p_aac_config, UINT16 *p_minmtu)
{
    BOOLEAN result = FALSE;
    UINT8 index;
    tBTA_AV_CO_PEER *p_peer;

    APPL_TRACE_EVENT("bta_av_co_cb.codec_cfg->id : codec 0x%x", bta_av_co_cb.codec_cfg->id);

    /* Minimum MTU is by default very large */
    *p_minmtu = 0xFFFF;

    mutex_global_lock();
    if (bta_av_co_cb.codec_cfg->id == BTIF_AV_CODEC_M24)
    {
        if (A2D_ParsAacInfo(p_aac_config, bta_av_co_cb.codec_cfg->info, FALSE) == A2D_SUCCESS)
        {
            for (index = 0; index < BTA_AV_CO_NUM_ELEMENTS(bta_av_co_cb.peers); index++)
            {
                p_peer = &bta_av_co_cb.peers[index];
                if (p_peer->opened)
                {
                    APPL_TRACE_EVENT("%s on index= %d", __func__, index);
                    if (p_peer->mtu < *p_minmtu)
                    {
                        *p_minmtu = p_peer->mtu;
                    }
                }
            }
            result = TRUE;
        }
    }

    if (!result)
    {
        /* Not AAC, still return the default values */
        APPL_TRACE_EVENT("%s Not SBC, still return the default values", __func__);
        *p_aac_config = btif_av_aac_default_config;
    }
    mutex_global_unlock();

    return result;
}
#endif

/*******************************************************************************
 **
 ** Function         bta_av_co_audio_discard_config
 **
 ** Description      Discard the codec configuration of a connection
 **
 ** Returns          Nothing
 **
 *******************************************************************************/
void bta_av_co_audio_discard_config(tBTA_AV_HNDL hndl)
{
    tBTA_AV_CO_PEER *p_peer;

    FUNC_TRACE();

    /* Find the peer info */
    p_peer = bta_av_co_get_peer(hndl);
    if (p_peer == NULL)
    {
        APPL_TRACE_ERROR("bta_av_co_audio_discard_config could not find peer entry");
        return;
    }

    /* Reset the peer codec configuration */
    bta_av_co_audio_peer_reset_config(p_peer);
}

/*******************************************************************************
 **
 ** Function         bta_av_co_init
 **
 ** Description      Initialization
 **
 ** Returns          Nothing
 **
 *******************************************************************************/
void bta_av_co_init(void)
{
    FUNC_TRACE();

    /* Reset the control block */
    memset(&bta_av_co_cb, 0, sizeof(bta_av_co_cb));

    bta_av_co_cb.codec_cfg_setconfig = NULL;

#if defined(BTA_AV_CO_CP_SCMS_T) && (BTA_AV_CO_CP_SCMS_T == TRUE)
    bta_av_co_cp_set_flag(BTA_AV_CP_SCMS_COPY_NEVER);
#else
    bta_av_co_cp_set_flag(BTA_AV_CP_SCMS_COPY_FREE);
#endif

    /* Reset the current config */
    bta_av_co_audio_codec_reset();
}


/*******************************************************************************
 **
 ** Function         bta_av_co_peer_cp_supported
 **
 ** Description      Checks if the peer supports CP
 **
 ** Returns          TRUE if the peer supports CP
 **
 *******************************************************************************/
BOOLEAN bta_av_co_peer_cp_supported(tBTA_AV_HNDL hndl)
{
    tBTA_AV_CO_PEER *p_peer;
    tBTA_AV_CO_SINK *p_sink;
    UINT8 index;

    FUNC_TRACE();

    /* Find the peer info */
    p_peer = bta_av_co_get_peer(hndl);
    if (p_peer == NULL)
    {
        APPL_TRACE_ERROR("bta_av_co_peer_cp_supported could not find peer entry");
        return FALSE;
    }

    for (index = 0; index < p_peer->num_sup_snks; index++)
    {
        p_sink = &p_peer->snks[index];
        if (p_sink->codec_type == A2D_MEDIA_CT_SBC)
        {
            return bta_av_co_audio_sink_has_scmst(p_sink);
        }
    }
    APPL_TRACE_ERROR("bta_av_co_peer_cp_supported did not find SBC sink");
    return FALSE;
}


/*******************************************************************************
 **
 ** Function         bta_av_co_get_remote_bitpool_pref
 **
 ** Description      Check if remote side did a setconfig within the limits
 **                  of our exported bitpool range. If set we will set the
 **                  remote preference.
 **
 ** Returns          TRUE if config set, FALSE otherwize
 **
 *******************************************************************************/

BOOLEAN bta_av_co_get_remote_bitpool_pref(UINT8 *min, UINT8 *max)
{
    /* check if remote peer did a set config */
    if (bta_av_co_cb.codec_cfg_setconfig == NULL)
        return FALSE;

    *min = bta_av_co_cb.codec_cfg_setconfig->info[BTA_AV_CO_SBC_MIN_BITPOOL_OFF];
    *max = bta_av_co_cb.codec_cfg_setconfig->info[BTA_AV_CO_SBC_MAX_BITPOOL_OFF];

    return TRUE;
}

/*******************************************************************************
**
** Function         bta_av_co_audio_is_offload_supported
**
** Description      This function is called by AV to check if DUT is in offload
**                  mode.
**
** Returns          TRUE if offload is enabled, FALSE otherwise
**
*******************************************************************************/
BOOLEAN bta_av_co_audio_is_offload_supported(void)
{
    return btif_av_is_offload_supported();
}

/*******************************************************************************
**
** Function         bta_av_co_audio_is_codec_supported
**
** Description      This function is called by AV to check if corresponding
**                  codec is supported in offload mode.
**
** Returns          TRUE if codec is supported in offload, FALSE otherwise
**
*******************************************************************************/
BOOLEAN bta_av_co_audio_is_codec_supported(int codec)
{
    return btif_av_is_codec_offload_supported(codec);
}
