#
# Copyright (C) 2015 The Android Open Source Project
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
#

LOCAL_PATH := $(call my-dir)

simpleperf_common_cppflags := -std=c++11 -Wall -Wextra -Werror -Wunused

simpleperf_common_shared_libraries := \
  libbase \
  libLLVM \

LLVM_ROOT_PATH := external/llvm

libsimpleperf_src_files := \
  cmd_dumprecord.cpp \
  cmd_help.cpp \
  cmd_list.cpp \
  cmd_record.cpp \
  cmd_stat.cpp \
  command.cpp \
  environment.cpp \
  event_attr.cpp \
  event_fd.cpp \
  event_selection_set.cpp \
  event_type.cpp \
  read_elf.cpp \
  record.cpp \
  record_file.cpp \
  utils.cpp \
  workload.cpp \

include $(CLEAR_VARS)
LOCAL_CLANG := true
LOCAL_CPPFLAGS := $(simpleperf_common_cppflags)
LOCAL_SRC_FILES := $(libsimpleperf_src_files)
LOCAL_SHARED_LIBRARIES := $(simpleperf_common_shared_libraries)
LOCAL_MODULE := libsimpleperf
LOCAL_MODULE_TAGS := debug
LOCAL_MODULE_PATH := $(TARGET_OUT_OPTIONAL_EXECUTABLES)
LOCAL_ADDITIONAL_DEPENDENCIES := $(LOCAL_PATH)/Android.mk
include $(LLVM_ROOT_PATH)/llvm.mk
include $(LLVM_DEVICE_BUILD_MK)
include $(BUILD_STATIC_LIBRARY)

ifeq ($(HOST_OS),linux)
include $(CLEAR_VARS)
LOCAL_CLANG := true
LOCAL_CPPFLAGS := $(simpleperf_common_cppflags)
LOCAL_SRC_FILES := $(libsimpleperf_src_files)
LOCAL_SHARED_LIBRARIES := $(simpleperf_common_shared_libraries)
LOCAL_LDLIBS := -lrt
LOCAL_MODULE := libsimpleperf
LOCAL_MODULE_TAGS := optional
LOCAL_ADDITIONAL_DEPENDENCIES := $(LOCAL_PATH)/Android.mk
include $(LLVM_ROOT_PATH)/llvm.mk
include $(LLVM_HOST_BUILD_MK)
include $(BUILD_HOST_STATIC_LIBRARY)
endif

include $(CLEAR_VARS)
LOCAL_CLANG := true
LOCAL_CPPFLAGS := $(simpleperf_common_cppflags)
LOCAL_SRC_FILES := main.cpp
LOCAL_WHOLE_STATIC_LIBRARIES := libsimpleperf
LOCAL_SHARED_LIBRARIES := $(simpleperf_common_shared_libraries)
LOCAL_MODULE := simpleperf
LOCAL_MODULE_TAGS := debug
LOCAL_MODULE_PATH := $(TARGET_OUT_OPTIONAL_EXECUTABLES)
LOCAL_ADDITIONAL_DEPENDENCIES := $(LOCAL_PATH)/Android.mk
include $(BUILD_EXECUTABLE)

ifeq ($(HOST_OS),linux)
include $(CLEAR_VARS)
LOCAL_CLANG := true
LOCAL_CPPFLAGS := $(simpleperf_common_cppflags)
LOCAL_SRC_FILES := main.cpp
LOCAL_WHOLE_STATIC_LIBRARIES := libsimpleperf
LOCAL_SHARED_LIBRARIES := $(simpleperf_common_shared_libraries)
LOCAL_LDLIBS := -lrt
LOCAL_MODULE := simpleperf
LOCAL_MODULE_TAGS := optional
LOCAL_ADDITIONAL_DEPENDENCIES := $(LOCAL_PATH)/Android.mk
include $(BUILD_HOST_EXECUTABLE)
endif

simpleperf_unit_test_src_files := \
  cmd_dumprecord_test.cpp \
  cmd_list_test.cpp \
  cmd_record_test.cpp \
  cmd_stat_test.cpp \
  command_test.cpp \
  environment_test.cpp \
  gtest_main.cpp \
  record_file_test.cpp \
  record_test.cpp \
  workload_test.cpp \

include $(CLEAR_VARS)
LOCAL_CLANG := true
LOCAL_CPPFLAGS := $(simpleperf_common_cppflags)
LOCAL_SRC_FILES := $(simpleperf_unit_test_src_files)
LOCAL_WHOLE_STATIC_LIBRARIES := libsimpleperf
LOCAL_SHARED_LIBRARIES := $(simpleperf_common_shared_libraries)
LOCAL_MODULE := simpleperf_unit_test
LOCAL_MODULE_TAGS := optional
LOCAL_ADDITIONAL_DEPENDENCIES := $(LOCAL_PATH)/Android.mk
include $(BUILD_NATIVE_TEST)

ifeq ($(HOST_OS),linux)
include $(CLEAR_VARS)
LOCAL_CLANG := true
LOCAL_CPPFLAGS := $(simpleperf_common_cppflags)
LOCAL_SRC_FILES := $(simpleperf_unit_test_src_files)
LOCAL_WHOLE_STATIC_LIBRARIES := libsimpleperf
LOCAL_SHARED_LIBRARIES := $(simpleperf_common_shared_libraries)
LOCAL_MODULE := simpleperf_unit_test
LOCAL_MODULE_TAGS := optional
LOCAL_ADDITIONAL_DEPENDENCIES := $(LOCAL_PATH)/Android.mk
include $(BUILD_HOST_NATIVE_TEST)
endif
