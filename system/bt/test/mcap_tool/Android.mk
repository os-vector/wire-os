#
#

LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)


LOCAL_SRC_FILES:= mcap_tool.c

LOCAL_C_INCLUDES += . \
        $(LOCAL_PATH)/../../stack/include \
        $(LOCAL_PATH)/../../include \
        $(LOCAL_PATH)/../../stack/l2cap \
        $(LOCAL_PATH)/../../utils/include \
        $(LOCAL_PATH)/../../ \
        $(LOCAL_PATH)/btif/include \
        $(bluetooth_C_INCLUDES)

LOCAL_CFLAGS += $(bluetooth_CFLAGS)
LOCAL_MODULE_PATH := $(TARGET_OUT_EXECUTABLES)
LOCAL_MODULE_TAGS := debug optional

LOCAL_MODULE:= mcap_tool


LOCAL_SHARED_LIBRARIES += libcutils   \
                          libutils    \
                          libhardware \
                          libhardware_legacy


LOCAL_MULTILIB := 32


include $(BUILD_EXECUTABLE)
