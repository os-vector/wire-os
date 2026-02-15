# Copyright 2015 The Android Open Source Project

LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_SRC_FILES := slideshow.cpp
LOCAL_MODULE := slideshow
LOCAL_MODULE_TAGS := optional
LOCAL_FORCE_STATIC_EXECUTABLE := true
LOCAL_MODULE_PATH := $(TARGET_ROOT_OUT_SBIN)
LOCAL_UNSTRIPPED_PATH := $(TARGET_ROOT_OUT_SBIN_UNSTRIPPED)

LOCAL_CFLAGS := -D__STDC_LIMIT_MACROS -Werror
LOCAL_C_INCLUDES := bootable/recovery
LOCAL_STATIC_LIBRARIES := libminui libpng libz libutils libstdc++ libcutils liblog libm libc
include $(BUILD_EXECUTABLE)
