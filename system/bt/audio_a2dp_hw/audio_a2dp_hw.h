/******************************************************************************
 *  Copyright (C) 2016, The Linux Foundation. All rights reserved.
 *
 *  Not a Contribution
 *****************************************************************************/
/******************************************************************************
 *
 *  Copyright (C) 2009-2012 Broadcom Corporation
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

/*****************************************************************************
 *
 *  Filename:      audio_a2dp_hw.h
 *
 *  Description:
 *
 *****************************************************************************/

#ifndef AUDIO_A2DP_HW_H
#define AUDIO_A2DP_HW_H
#include <pthread.h>
#include <stdint.h>
/*****************************************************************************
**  Constants & Macros
******************************************************************************/

#define A2DP_AUDIO_HARDWARE_INTERFACE "audio.a2dp"
#ifdef ANDROID
#define A2DP_CTRL_PATH "/data/misc/bluedroid/.a2dp_ctrl"
#define A2DP_DATA_PATH "/data/misc/bluedroid/.a2dp_data"
#else
#define A2DP_CTRL_PATH "/data/misc/bluetooth/.a2dp_ctrl"
#define A2DP_DATA_PATH "/data/misc/bluetooth/.a2dp_data"
#define A2DP_AVK_CTRL_PATH "/data/misc/bluetooth/.a2dp_avk_ctrl"
#define A2DP_AVK_DATA_PATH "/data/misc/bluetooth/.a2dp_avk_data"
#endif

#ifndef BTA_AV_SPLIT_A2DP_DEF_FREQ_48KHZ
#define AUDIO_STREAM_DEFAULT_RATE          44100
#else
#define AUDIO_STREAM_DEFAULT_RATE          48000
#endif

#define AUDIO_STREAM_DEFAULT_FORMAT        AUDIO_FORMAT_PCM_16_BIT
#define AUDIO_STREAM_DEFAULT_CHANNEL_FLAG  AUDIO_CHANNEL_OUT_STEREO

// AUDIO_STREAM_OUTPUT_BUFFER_SZ controls the size of the audio socket buffer.
// If one assumes the write buffer is always full during normal BT playback,
// then increasing this value increases our playback latency.
//
// FIXME: AUDIO_STREAM_OUTPUT_BUFFER_SZ should be controlled by the actual audio
// sample rate rather than being constant.
//
// FIXME: The BT HAL should consume data at a constant rate.
// AudioFlinger assumes that the HAL draws data at a constant rate, which is true
// for most audio devices; however, the BT engine reads data at a variable rate
// (over the short term), which confuses both AudioFlinger as well as applications
// which deliver data at a (generally) fixed rate.
//
// 20 * 512 is not sufficient size to smooth the variability for some BT devices,
// resulting in mixer sleep and throttling. We increase this to 28 * 512 to help
// reduce the effect of variable data consumption.
#define AUDIO_STREAM_OUTPUT_BUFFER_SZ      (28*512)

// AUDIO_STREAM_OUTPUT_BUFFER_PERIODS controls how the socket buffer is divided
// for AudioFlinger data delivery. The AudioFlinger mixer delivers data in chunks
// of AUDIO_STREAM_OUTPUT_BUFFER_SZ / AUDIO_STREAM_OUTPUT_BUFFER_PERIODS. If
// the number of periods is 2, the socket buffer represents "double buffering"
// of the AudioFlinger mixer buffer.
//
// In general, AUDIO_STREAM_OUTPUT_BUFFER_PERIODS * 16 * 4 should be a divisor of
// AUDIO_STREAM_OUTPUT_BUFFER_SZ.
//
// These values should be chosen such that
//
// AUDIO_STREAM_BUFFER_SIZE * 1000 / (AUDIO_STREAM_OUTPUT_BUFFER_PERIODS
//         * AUDIO_STREAM_DEFAULT_RATE * 4) > 20 (ms)
//
// to avoid introducing the FastMixer in AudioFlinger. Using the FastMixer results in
// unnecessary latency and CPU overhead for Bluetooth.
#define AUDIO_STREAM_OUTPUT_BUFFER_PERIODS 4

#define AUDIO_SKT_DISCONNECTED             (-1)

typedef enum {
    A2DP_CTRL_CMD_NONE,
    A2DP_CTRL_CMD_CHECK_READY,
    A2DP_CTRL_CMD_CHECK_STREAM_STARTED,
    A2DP_CTRL_CMD_START,
    A2DP_CTRL_CMD_STOP,
    A2DP_CTRL_CMD_SUSPEND,
    A2DP_CTRL_GET_AUDIO_CONFIG,
    A2DP_CTRL_CMD_OFFLOAD_START,
    A2DP_CTRL_CMD_OFFLOAD_SUPPORTED,
    A2DP_CTRL_CMD_OFFLOAD_NOT_SUPPORTED,
} tA2DP_CTRL_CMD;

typedef enum {
    A2DP_CTRL_ACK_SUCCESS,
    A2DP_CTRL_ACK_FAILURE,
    A2DP_CTRL_ACK_INCALL_FAILURE, /* Failure when in Call*/
    A2DP_CTRL_ACK_UNSUPPORTED
} tA2DP_CTRL_ACK;


typedef enum {
    AUDIO_A2DP_STATE_STARTING,
    AUDIO_A2DP_STATE_STARTED,
    AUDIO_A2DP_STATE_STOPPING,
    AUDIO_A2DP_STATE_STOPPED,
    AUDIO_A2DP_STATE_SUSPENDED, /* need explicit set param call to resume (suspend=false) */
    AUDIO_A2DP_STATE_STANDBY    /* allows write to autoresume */
} a2dp_state_t;

struct a2dp_config {
    uint32_t                rate;
    uint32_t                channel_flags;
    int                     format;
};

/* move ctrl_fd outside output stream and keep open until HAL unloaded ? */
#define  MAX_CODEC_CFG_SIZE  30

struct a2dp_stream_common {
    pthread_mutex_t         lock;
    int                     ctrl_fd;
    int                     audio_fd;
    size_t                  buffer_sz;
    struct a2dp_config      cfg;
    a2dp_state_t            state;
    uint8_t                 codec_cfg[MAX_CODEC_CFG_SIZE];
};
/*****************************************************************************
**  Type definitions for callback functions
******************************************************************************/

/*****************************************************************************
**  Type definitions and return values
******************************************************************************/

/*****************************************************************************
**  Extern variables and functions
******************************************************************************/

/*****************************************************************************
**  Functions
******************************************************************************/


/*****************************************************************************
**
** Function
**
** Description
**
** Returns
**
******************************************************************************/

#endif /* A2DP_AUDIO_HW_H */

