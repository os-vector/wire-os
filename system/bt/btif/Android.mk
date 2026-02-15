 ##############################################################################
 #
 #  Copyright (C) 2014 Google, Inc.
 #
 #  Licensed under the Apache License, Version 2.0 (the "License");
 #  you may not use this file except in compliance with the License.
 #  You may obtain a copy of the License at:
 #
 #  http://www.apache.org/licenses/LICENSE-2.0
 #
 #  Unless required by applicable law or agreed to in writing, software
 #  distributed under the License is distributed on an "AS IS" BASIS,
 #  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 #  See the License for the specific language governing permissions and
 #  limitations under the License.
 #
 ##############################################################################

LOCAL_PATH := $(call my-dir)

# Common variables
# ========================================================

# HAL layer
btifCommonSrc := \
  src/bluetooth.c \
  ../EventLogTags.logtags

# BTIF implementation
btifCommonSrc += \
  src/btif_av.c \
  src/btif_avrcp_audio_track.cpp \
  src/btif_config.c \
  src/btif_config_transcode.cpp \
  src/btif_core.c \
  src/btif_debug.c \
  src/btif_debug_btsnoop.c \
  src/btif_debug_conn.c \
  src/btif_dm.c \
  src/btif_gatt.c \
  src/btif_gatt_client.c \
  src/btif_gatt_multi_adv_util.c \
  src/btif_gatt_server.c \
  src/btif_gatt_test.c \
  src/btif_gatt_util.c \
  src/btif_hf.c \
  src/btif_hf_client.c \
  src/btif_hh.c \
  src/btif_hl.c \
  src/btif_sdp.c \
  src/btif_media_task.c \
  src/btif_pan.c \
  src/btif_profile_queue.c \
  src/btif_rc.c \
  src/btif_sm.c \
  src/btif_sock.c \
  src/btif_sock_rfc.c \
  src/btif_sock_l2cap.c \
  src/btif_sock_sco.c \
  src/btif_sock_sdp.c \
  src/btif_sock_thread.c \
  src/btif_sdp_server.c \
  src/btif_sock_util.c \
  src/btif_storage.c \
  src/btif_uid.c \
  src/btif_util.c \
  src/stack_manager.c \
  src/btif_stack_log.c \
  src/btif_rfcomm.c \
  src/btif_mcap.c \
  src/btif_l2cap.c \
  src/btif_vendor.c \
  src/btif_gatt_qual.c \
  src/btif_gap.c \
  src/btif_smp.c

# Callouts
btifCommonSrc += \
  co/bta_ag_co.c \
  co/bta_dm_co.c \
  co/bta_av_co.c \
  co/bta_hh_co.c \
  co/bta_hl_co.c \
  co/bta_pan_co.c \
  co/bta_gatts_co.c

# Tests
btifTestSrc := \
  test/btif_storage_test.cpp

# Includes
btifCommonIncludes := \
  $(LOCAL_PATH)/../ \
  $(LOCAL_PATH)/../bta/include \
  $(LOCAL_PATH)/../bta/sys \
  $(LOCAL_PATH)/../bta/dm \
  $(LOCAL_PATH)/../btcore/include \
  $(LOCAL_PATH)/../include \
  $(LOCAL_PATH)/../stack/include \
  $(LOCAL_PATH)/../stack/l2cap \
  $(LOCAL_PATH)/../stack/a2dp \
  $(LOCAL_PATH)/../stack/btm \
  $(LOCAL_PATH)/../stack/avdt \
  $(LOCAL_PATH)/../hcis \
  $(LOCAL_PATH)/../hcis/include \
  $(LOCAL_PATH)/../hcis/patchram \
  $(LOCAL_PATH)/../udrv/include \
  $(LOCAL_PATH)/../btif/include \
  $(LOCAL_PATH)/../btif/co \
  $(LOCAL_PATH)/../hci/include\
  $(LOCAL_PATH)/../vnd/include \
  $(LOCAL_PATH)/../brcm/include \
  $(LOCAL_PATH)/../embdrv/sbc/encoder/include \
  $(LOCAL_PATH)/../embdrv/sbc/decoder/include \
  $(LOCAL_PATH)/../audio_a2dp_hw \
  $(LOCAL_PATH)/../utils/include \
  $(bluetooth_C_INCLUDES) \
  external/tinyxml2 \
  external/zlib \
  vendor/qcom/opensource/bluetooth/system_bt_ext \
  $(call include-path-for, audio-utils)

ifneq ($(TARGET_SUPPORTS_WEARABLES),true)
btifCommonIncludes += \
   vendor/qcom/opensource/bluetooth/hal/include
else
btifCommonIncludes += \
   device/qcom/msm8909w/opensource/bluetooth/hal/include
endif

# libbtif static library for target
# ========================================================
include $(CLEAR_VARS)
LOCAL_C_INCLUDES := $(btifCommonIncludes)
LOCAL_SRC_FILES := $(btifCommonSrc)
# Many .h files have redefined typedefs
LOCAL_SHARED_LIBRARIES := libcutils liblog
LOCAL_MODULE_CLASS := STATIC_LIBRARIES
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE := libbtif

LOCAL_CFLAGS += $(bluetooth_CFLAGS) -DBUILDCFG
LOCAL_CONLYFLAGS += $(bluetooth_CONLYFLAGS)
LOCAL_CPPFLAGS += $(bluetooth_CPPFLAGS)


include $(BUILD_STATIC_LIBRARY)

# btif unit tests for target
# ========================================================
include $(CLEAR_VARS)
LOCAL_C_INCLUDES := $(btifCommonIncludes)
LOCAL_SRC_FILES := $(btifTestSrc)
LOCAL_SHARED_LIBRARIES += liblog libhardware libhardware_legacy libcutils
LOCAL_STATIC_LIBRARIES += libbtcore libbtif libosi
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE := net_test_btif

LOCAL_CFLAGS += $(bluetooth_CFLAGS) -DBUILDCFG
LOCAL_CONLYFLAGS += $(bluetooth_CONLYFLAGS)
LOCAL_CPPFLAGS += $(bluetooth_CPPFLAGS)

include $(BUILD_NATIVE_TEST)
