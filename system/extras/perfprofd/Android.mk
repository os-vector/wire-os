LOCAL_PATH:= $(call my-dir)

perfprofd_cppflags := \
  -Wall \
  -Wno-sign-compare \
  -Wno-unused-parameter \
  -Werror \
  -std=gnu++11 \

#
# Static library containing guts of AWP daemon.
#
include $(CLEAR_VARS)
LOCAL_CLANG := true
LOCAL_CPP_EXTENSION := cc
LOCAL_MODULE := libperfprofdcore
LOCAL_MODULE_CLASS := STATIC_LIBRARIES
LOCAL_MODULE_PATH := $(TARGET_OUT_OPTIONAL_EXECUTABLES)
LOCAL_MODULE_TAGS := debug
proto_header_dir := $(call local-generated-sources-dir)/proto/$(LOCAL_PATH)
LOCAL_C_INCLUDES += $(proto_header_dir) $(LOCAL_PATH)/quipper/kernel-headers
LOCAL_STATIC_LIBRARIES := libbase
LOCAL_EXPORT_C_INCLUDE_DIRS += $(proto_header_dir)
LOCAL_SRC_FILES :=  \
	perf_profile.proto \
	quipper/perf_utils.cc \
	quipper/base/logging.cc \
	quipper/address_mapper.cc \
	quipper/perf_reader.cc \
	quipper/perf_parser.cc \
	perf_data_converter.cc \
	cpuconfig.cc \
	perfprofdcore.cc \

LOCAL_CPPFLAGS += $(perfprofd_cppflags)
include $(BUILD_STATIC_LIBRARY)

#
# Static library with primary utilities layer (called by perfprofd core)
#
include $(CLEAR_VARS)
LOCAL_CLANG := true
LOCAL_CPP_EXTENSION := cc
LOCAL_CXX_STL := libc++
LOCAL_MODULE := libperfprofdutils
LOCAL_MODULE_PATH := $(TARGET_OUT_OPTIONAL_EXECUTABLES)
LOCAL_MODULE_TAGS := debug
LOCAL_CPPFLAGS += $(perfprofd_cppflags)
LOCAL_SRC_FILES := perfprofdutils.cc
include $(BUILD_STATIC_LIBRARY)

#
# Main daemon
#
include $(CLEAR_VARS)
LOCAL_CLANG := true
LOCAL_CPP_EXTENSION := cc
LOCAL_CXX_STL := libc++
LOCAL_SRC_FILES := perfprofdmain.cc
LOCAL_STATIC_LIBRARIES := libperfprofdcore libperfprofdutils
LOCAL_SHARED_LIBRARIES := liblog libprotobuf-cpp-lite libbase
LOCAL_SYSTEM_SHARED_LIBRARIES := libc libstdc++
LOCAL_CPPFLAGS += $(perfprofd_cppflags)
LOCAL_CFLAGS := -Wall -Werror -std=gnu++11
LOCAL_MODULE := perfprofd
LOCAL_MODULE_PATH := $(TARGET_OUT_OPTIONAL_EXECUTABLES)
LOCAL_MODULE_TAGS := debug
LOCAL_SHARED_LIBRARIES += libcutils
include $(BUILD_EXECUTABLE)

# Clean temp vars
perfprofd_cppflags :=
proto_header_dir :=
