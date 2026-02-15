LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_SRC_FILES:=     \
    bluedroidtest.c

LOCAL_C_INCLUDES :=
LOCAL_CFLAGS := -Wno-unused-parameter

LOCAL_CFLAGS += -std=c99

LOCAL_CFLAGS += -std=c99

LOCAL_MODULE_TAGS := debug optional

LOCAL_MODULE:= bdt

LOCAL_SHARED_LIBRARIES += libcutils   \
                          libhardware \
                          libhardware_legacy

LOCAL_MULTILIB := 32

include $(BUILD_EXECUTABLE)
