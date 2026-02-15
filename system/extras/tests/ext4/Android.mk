# Copyright 2012 The Android Open Source Project

local_target_dir := $(TARGET_OUT_DATA)/local/tmp
LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES:= rand_emmc_perf.c
LOCAL_MODULE:= rand_emmc_perf
LOCAL_MULTILIB := both
LOCAL_MODULE_STEM_32:= rand_emmc_perf
LOCAL_MODULE_STEM_64:= rand_emmc_perf64
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_PATH := $(local_target_dir)
LOCAL_FORCE_STATIC_EXECUTABLE := true
LOCAL_STATIC_LIBRARIES := libm libc

include $(BUILD_EXECUTABLE)

