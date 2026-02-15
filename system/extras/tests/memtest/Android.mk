# Copyright 2006 The Android Open Source Project
ifeq ($(TARGET_ARCH),arm)
LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)
LOCAL_ADDITIONAL_DEPENDENCIES := $(LOCAL_PATH)/Android.mk

LOCAL_SRC_FILES:= \
		memtest.cpp.arm \
		fptest.cpp \
		thumb.cpp \
		bandwidth.cpp \

LOCAL_MODULE:= memtest
LOCAL_MODULE_TAGS := optional
LOCAL_CFLAGS += -fomit-frame-pointer

include $(BUILD_EXECUTABLE)
endif
