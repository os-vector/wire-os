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

# Common variables
# ========================================================

# TODO(mcchou): Remove socket_utils sources after platform specific
# dependencies are abstracted.
btosiCommonSrc := \
    ./src/alarm.c \
    ./src/allocation_tracker.c \
    ./src/allocator.c \
    ./src/array.c \
    ./src/buffer.c \
    ./src/compat.c \
    ./src/config.c \
    ./src/data_dispatcher.c \
    ./src/eager_reader.c \
    ./src/fixed_queue.c \
    ./src/future.c \
    ./src/hash_functions.c \
    ./src/hash_map.c \
    ./src/hash_map_utils.c \
    ./src/list.c \
    ./src/metrics.cpp \
    ./src/mutex.c \
    ./src/osi.c \
    ./src/properties.c \
    ./src/reactor.c \
    ./src/ringbuffer.c \
    ./src/semaphore.c \
    ./src/socket.c \
    ./src/socket_utils/socket_local_client.c \
    ./src/socket_utils/socket_local_server.c \
    ./src/thread.c \
    ./src/time.c \
    ./src/wakelock.c \
    ./src/vnd_log.c

btosiCommonTestSrc := \
    ./test/AlarmTestHarness.cpp \
    ./test/AllocationTestHarness.cpp \
    ./test/alarm_test.cpp \
    ./test/allocation_tracker_test.cpp \
    ./test/allocator_test.cpp \
    ./test/array_test.cpp \
    ./test/config_test.cpp \
    ./test/data_dispatcher_test.cpp \
    ./test/eager_reader_test.cpp \
    ./test/fixed_queue_test.cpp \
    ./test/future_test.cpp \
    ./test/hash_map_test.cpp \
    ./test/hash_map_utils_test.cpp \
    ./test/list_test.cpp \
    ./test/properties_test.cpp \
    ./test/rand_test.cpp \
    ./test/reactor_test.cpp \
    ./test/ringbuffer_test.cpp \
    ./test/semaphore_test.cpp \
    ./test/thread_test.cpp \
    ./test/time_test.cpp

btosiCommonIncludes := \
    $(LOCAL_PATH)/.. \
    $(LOCAL_PATH)/../utils/include \
    $(LOCAL_PATH)/../stack/include \
    $(bluetooth_C_INCLUDES)

# Bluetooth Protobuf static library for target
# ========================================================
include $(CLEAR_VARS)
LOCAL_MODULE := libbt-protos
LOCAL_MODULE_CLASS := STATIC_LIBRARIES
generated_sources_dir := $(call local-generated-sources-dir)
LOCAL_EXPORT_C_INCLUDE_DIRS += \
    $(generated_sources_dir)/proto/system/bt
LOCAL_SRC_FILES := $(call all-proto-files-under,src/protos/)

LOCAL_CFLAGS += $(bluetooth_CFLAGS)
LOCAL_CONLYFLAGS += $(bluetooth_CONLYFLAGS)
LOCAL_CPPFLAGS += $(bluetooth_CPPFLAGS)

include $(BUILD_STATIC_LIBRARY)

# Bluetooth Protobuf static library for host
# ========================================================
include $(CLEAR_VARS)
LOCAL_MODULE := libbt-protos
LOCAL_MODULE_CLASS := STATIC_LIBRARIES
LOCAL_IS_HOST_MODULE := true
generated_sources_dir := $(call local-generated-sources-dir)
LOCAL_EXPORT_C_INCLUDE_DIRS += \
    $(generated_sources_dir)/proto/system/bt
LOCAL_SRC_FILES := $(call all-proto-files-under,src/protos/)

LOCAL_CFLAGS += $(bluetooth_CFLAGS)
LOCAL_CONLYFLAGS += $(bluetooth_CONLYFLAGS)
LOCAL_CPPFLAGS += $(bluetooth_CPPFLAGS)

include $(BUILD_HOST_STATIC_LIBRARY)

# libosi static library for target
# ========================================================
include $(CLEAR_VARS)
LOCAL_C_INCLUDES := $(btosiCommonIncludes)
LOCAL_SRC_FILES := $(btosiCommonSrc)
LOCAL_MODULE := libosi
LOCAL_MODULE_TAGS := optional
LOCAL_SHARED_LIBRARIES := libc liblog libchrome
LOCAL_STATIC_LIBRARIES := libbt-protos
LOCAL_MODULE_CLASS := STATIC_LIBRARIES

LOCAL_CFLAGS += $(bluetooth_CFLAGS)
LOCAL_CONLYFLAGS += $(bluetooth_CONLYFLAGS)
LOCAL_CPPFLAGS += $(bluetooth_CPPFLAGS)

include $(BUILD_STATIC_LIBRARY)

# libosi static library for host
# ========================================================
ifeq ($(HOST_OS),linux)
include $(CLEAR_VARS)
LOCAL_C_INCLUDES := $(btosiCommonIncludes)
LOCAL_SRC_FILES := $(btosiCommonSrc)
LOCAL_MODULE := libosi-host
LOCAL_MODULE_TAGS := optional
LOCAL_SHARED_LIBRARIES := liblog libchrome
LOCAL_STATIC_LIBRARIES := libbt-protos
LOCAL_MODULE_CLASS := STATIC_LIBRARIES

# TODO(armansito): Setting _GNU_SOURCE isn't very platform-independent but
# should be compatible for a Linux host OS. We should figure out what to do for
# a non-Linux host OS.
LOCAL_CFLAGS += $(bluetooth_CFLAGS) -D_GNU_SOURCE -DOS_GENERIC
LOCAL_CONLYFLAGS += $(bluetooth_CONLYFLAGS)
LOCAL_CPPFLAGS += $(bluetooth_CPPFLAGS)

include $(BUILD_HOST_STATIC_LIBRARY)
endif

#
# Note: It's good to get the tests compiled both for the host and the target so
# we get to test with both Bionic libc and glibc
#
# libosi unit tests for target
# ========================================================
include $(CLEAR_VARS)
LOCAL_C_INCLUDES := $(btosiCommonIncludes)
LOCAL_SRC_FILES := $(btosiCommonTestSrc)
LOCAL_MODULE := net_test_osi
LOCAL_MODULE_TAGS := tests
LOCAL_SHARED_LIBRARIES := libc liblog libprotobuf-cpp-full libchrome libcutils
LOCAL_STATIC_LIBRARIES := libosi libbt-protos

LOCAL_CFLAGS += $(bluetooth_CFLAGS)
LOCAL_CONLYFLAGS += $(bluetooth_CONLYFLAGS)
LOCAL_CPPFLAGS += $(bluetooth_CPPFLAGS)

include $(BUILD_NATIVE_TEST)

# libosi unit tests for host
# ========================================================
ifeq ($(HOST_OS),linux)
include $(CLEAR_VARS)
LOCAL_C_INCLUDES := $(btosiCommonIncludes)
LOCAL_SRC_FILES := $(btosiCommonTestSrc)
LOCAL_LDLIBS := -lrt -lpthread
LOCAL_MODULE := net_test_osi
LOCAL_MODULE_TAGS := tests
LOCAL_SHARED_LIBRARIES := liblog libprotobuf-cpp-full libchrome
LOCAL_STATIC_LIBRARIES := libosi-host libbt-protos

LOCAL_CFLAGS += $(bluetooth_CFLAGS) -DOS_GENERIC
LOCAL_CONLYFLAGS += $(bluetooth_CONLYFLAGS)
LOCAL_CPPFLAGS += $(bluetooth_CPPFLAGS)

include $(BUILD_HOST_NATIVE_TEST)
endif
