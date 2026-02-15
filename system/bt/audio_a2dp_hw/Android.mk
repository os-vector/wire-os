LOCAL_PATH := $(call my-dir)

# Audio A2DP shared library for target
# ========================================================
include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
	audio_a2dp_hw.c

LOCAL_C_INCLUDES += \
	. \
	$(LOCAL_PATH)/../ \
	$(LOCAL_PATH)/../utils/include

LOCAL_CFLAGS = -DBT_HOST_IPC_ENABLED
LOCAL_MODULE := audio.a2dp.default
LOCAL_MODULE_RELATIVE_PATH := hw

LOCAL_SHARED_LIBRARIES := liblog
LOCAL_STATIC_LIBRARIES := libosi

LOCAL_MODULE_TAGS := optional

LOCAL_CFLAGS += $(bluetooth_CFLAGS)
LOCAL_CONLYFLAGS += $(bluetooth_CONLYFLAGS)
LOCAL_CPPFLAGS += $(bluetooth_CPPFLAGS)

include $(BUILD_SHARED_LIBRARY)

##########################################
include $(CLEAR_VARS)

LOCAL_SRC_FILES := bthost_ipc.c

LOCAL_C_INCLUDES += \
	$(LOCAL_PATH)/bthost_ipc.h \
	$(LOCAL_PATH)/../ \
	$(LOCAL_PATH)/../utils/include

LOCAL_CFLAGS = $(bdroid_CFLAGS)

LOCAL_MODULE := libbthost_if
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
LOCAL_SHARED_LIBRARIES := liblog
LOCAL_STATIC_LIBRARIES := libosi

LOCAL_MODULE_TAGS := optional

include $(BUILD_SHARED_LIBRARY)
