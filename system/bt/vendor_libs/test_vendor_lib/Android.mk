LOCAL_PATH := $(call my-dir)

# test-vendor shared library for target
# ========================================================
include $(CLEAR_VARS)

BT_DIR := $(TOP_DIR)system/bt

LOCAL_SRC_FILES := \
    src/bt_vendor.cc \
    src/command_packet.cc \
    src/dual_mode_controller.cc \
    src/event_packet.cc \
    src/hci_transport.cc \
    src/packet.cc \
    src/packet_stream.cc \
    src/test_channel_transport.cc \
    src/vendor_manager.cc

# We pull in gtest because base/files/file_util.h, which is used to read the
# controller properties file, needs gtest/gtest_prod.h.
LOCAL_C_INCLUDES := \
    $(LOCAL_PATH)/include \
    $(BT_DIR) \
    $(BT_DIR)/hci/include \
    $(BT_DIR)/stack/include \
    $(BT_DIR)/third_party/gtest/include

LOCAL_SHARED_LIBRARIES := \
    liblog \
    libchrome

LOCAL_CPP_EXTENSION := .cc
LOCAL_MODULE := test-vendor
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_CLASS := SHARED_LIBRARIES

LOCAL_CFLAGS += $(bluetooth_CFLAGS)
LOCAL_CONLYFLAGS += $(bluetooth_CONLYFLAGS)
LOCAL_CPPFLAGS += $(bluetooth_CPPFLAGS)

include $(BUILD_SHARED_LIBRARY)

# test-vendor unit tests for host
# ========================================================
ifeq ($(HOST_OS), linux)
include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
    src/command_packet.cc \
    src/event_packet.cc \
    src/hci_transport.cc \
    src/packet.cc \
    src/packet_stream.cc \
    test/hci_transport_unittest.cc \
    test/packet_stream_unittest.cc

LOCAL_C_INCLUDES := \
    $(LOCAL_PATH)/include \
    $(BT_DIR) \
    $(BT_DIR)/hci/include \
    $(BT_DIR)/stack/include

LOCAL_SHARED_LIBRARIES := \
    liblog \
    libchrome

LOCAL_CPP_EXTENSION := .cc
LOCAL_MODULE := test-vendor_test_host
LOCAL_MODULE_TAGS := tests

LOCAL_CFLAGS += $(bluetooth_CFLAGS)
LOCAL_CONLYFLAGS += $(bluetooth_CONLYFLAGS)
LOCAL_CPPFLAGS += $(bluetooth_CPPFLAGS)

include $(BUILD_HOST_NATIVE_TEST)
endif
