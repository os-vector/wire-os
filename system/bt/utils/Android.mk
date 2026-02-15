LOCAL_PATH := $(call my-dir)

# Utils static library for target
# ========================================================
include $(CLEAR_VARS)

LOCAL_C_INCLUDES := \
	$(LOCAL_PATH)/include \
	$(LOCAL_PATH)/../btcore/include \
	$(LOCAL_PATH)/../stack/include \
	$(LOCAL_PATH)/../gki/common \
	$(LOCAL_PATH)/../include \
	$(LOCAL_PATH)/../ \
	$(bluetooth_C_INCLUDES)

LOCAL_SRC_FILES := \
        ./src/bt_utils.c

LOCAL_STATIC_LIBRARIES := \
        libosi

LOCAL_MODULE := libbt-utils
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_CLASS := STATIC_LIBRARIES

LOCAL_CFLAGS += $(bluetooth_CFLAGS)
LOCAL_CONLYFLAGS += $(bluetooth_CONLYFLAGS)
LOCAL_CPPFLAGS += $(bluetooth_CPPFLAGS)

include $(BUILD_STATIC_LIBRARY)
