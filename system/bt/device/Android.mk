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

# Bluetooth device static library for target
# ========================================================
include $(CLEAR_VARS)

LOCAL_C_INCLUDES := \
    $(LOCAL_PATH)/.. \
    $(LOCAL_PATH)/include \
    $(LOCAL_PATH)/../btcore/include \
    $(LOCAL_PATH)/../hci/include \
    $(LOCAL_PATH)/../include \
    $(LOCAL_PATH)/../stack/include \
    vendor/qcom/opensource/bluetooth/system_bt_ext \
    $(bluetooth_C_INCLUDES)

LOCAL_SRC_FILES := \
    src/classic/peer.c \
    src/controller.c \
    ../../../vendor/qcom/opensource/bluetooth/system_bt_ext/device/src/interop.c

LOCAL_MODULE := libbtdevice
LOCAL_MODULE_TAGS := optional
LOCAL_SHARED_LIBRARIES := libc liblog
LOCAL_MODULE_CLASS := STATIC_LIBRARIES

LOCAL_CFLAGS += $(bluetooth_CFLAGS)
LOCAL_CONLYFLAGS += $(bluetooth_CONLYFLAGS)
LOCAL_CPPFLAGS += $(bluetooth_CPPFLAGS)

include $(BUILD_STATIC_LIBRARY)

# Bluetooth device unit tests for target
# ========================================================
include $(CLEAR_VARS)

LOCAL_C_INCLUDES := \
    $(LOCAL_PATH)/.. \
    $(bluetooth_C_INCLUDES)

LOCAL_SRC_FILES := \
    ../osi/test/AllocationTestHarness.cpp \
    ./test/interop_test.cpp \
    ./test/classic/peer_test.cpp

LOCAL_MODULE := net_test_device
LOCAL_MODULE_TAGS := tests
LOCAL_SHARED_LIBRARIES := liblog libdl
LOCAL_STATIC_LIBRARIES := libbtdevice libbtcore libosi libcutils

LOCAL_CFLAGS += $(bluetooth_CFLAGS)
LOCAL_CONLYFLAGS += $(bluetooth_CONLYFLAGS)
LOCAL_CPPFLAGS += $(bluetooth_CPPFLAGS)

include $(BUILD_NATIVE_TEST)
