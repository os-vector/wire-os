# Copyright (C) 2013 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

LOCAL_PATH:= $(call my-dir)

src_files := \
    memtrack.cpp

include $(CLEAR_VARS)
LOCAL_ADDITIONAL_DEPENDENCIES := $(LOCAL_PATH)/Android.mk

LOCAL_SRC_FILES := $(src_files)

LOCAL_MODULE_PATH := $(TARGET_OUT_OPTIONAL_EXECUTABLES)
LOCAL_MODULE := memtrack_share

LOCAL_C_INCLUDES += $(includes)
LOCAL_SHARED_LIBRARIES := \
    liblog \

include $(BUILD_EXECUTABLE)

include $(CLEAR_VARS)
LOCAL_ADDITIONAL_DEPENDENCIES := $(LOCAL_PATH)/Android.mk

LOCAL_SRC_FILES := $(src_files)
LOCAL_MODULE_PATH := $(TARGET_OUT_OPTIONAL_EXECUTABLES)
LOCAL_MODULE := memtrack

LOCAL_FORCE_STATIC_EXECUTABLE := true
LOCAL_STATIC_LIBRARIES := \
    libc \
    liblog \
    libc++abi \
    libdl \

LOCAL_CXX_STL := libc++_static

# Bug: 18389563 - Today, libc++_static and libgcc have duplicate sybols for
# __aeabi_uidiv(). Allowing multiple definitions lets the build proceed, but
# updating compiler-rt to be a superset of libgcc will allow this WAR to be
# removed.
LOCAL_LDFLAGS := -Wl,-z,muldefs

include $(BUILD_EXECUTABLE)
