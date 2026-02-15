# Copyright 2010 The Android Open Source Project

LOCAL_PATH:= $(call my-dir)

libext4_utils_src_files := \
    make_ext4fs.c \
    ext4fixup.c \
    ext4_utils.c \
    allocate.c \
    contents.c \
    extent.c \
    indirect.c \
    sha1.c \
    wipe.c \
    crc16.c \
    ext4_sb.c

#
# -- All host/targets including windows
#

include $(CLEAR_VARS)
LOCAL_SRC_FILES := $(libext4_utils_src_files)
LOCAL_MODULE := libext4_utils_host
LOCAL_STATIC_LIBRARIES := \
    libsparse_host \
    libz
ifneq ($(HOST_OS),windows)
  LOCAL_STATIC_LIBRARIES += libselinux
endif
include $(BUILD_HOST_STATIC_LIBRARY)


include $(CLEAR_VARS)
LOCAL_SRC_FILES := make_ext4fs_main.c canned_fs_config.c
LOCAL_MODULE := make_ext4fs
LOCAL_SHARED_LIBRARIES += libcutils
LOCAL_STATIC_LIBRARIES += \
    libext4_utils_host \
    libsparse_host \
    libz
ifeq ($(HOST_OS),windows)
  LOCAL_LDLIBS += -lws2_32
else
  LOCAL_SHARED_LIBRARIES += libselinux
  LOCAL_CFLAGS := -DHOST
endif
include $(BUILD_HOST_EXECUTABLE)


#
# -- All host/targets excluding windows
#

libext4_utils_src_files += \
    key_control.cpp \
    ext4_crypt.cpp \
    unencrypted_properties.cpp

ifneq ($(HOST_OS),windows)

include $(CLEAR_VARS)
LOCAL_SRC_FILES := $(libext4_utils_src_files)
LOCAL_MODULE := libext4_utils
LOCAL_C_INCLUDES += system/core/logwrapper/include
LOCAL_SHARED_LIBRARIES := \
    libcutils \
    libext2_uuid \
    libselinux \
    libsparse \
    libz
LOCAL_CFLAGS := -DREAL_UUID
include $(BUILD_SHARED_LIBRARY)


include $(CLEAR_VARS)
LOCAL_SRC_FILES := $(libext4_utils_src_files) \
    ext4_crypt_init_extensions.cpp
LOCAL_MODULE := libext4_utils_static
LOCAL_STATIC_LIBRARIES := \
    libsparse_static
include $(BUILD_STATIC_LIBRARY)


include $(CLEAR_VARS)
LOCAL_SRC_FILES := make_ext4fs_main.c canned_fs_config.c
LOCAL_MODULE := make_ext4fs
LOCAL_SHARED_LIBRARIES := \
    libcutils \
    libext2_uuid \
    libext4_utils \
    libselinux \
    libz
LOCAL_CFLAGS := -DREAL_UUID
include $(BUILD_EXECUTABLE)


include $(CLEAR_VARS)
LOCAL_SRC_FILES := ext2simg.c
LOCAL_MODULE := ext2simg
LOCAL_SHARED_LIBRARIES += \
    libext4_utils \
    libselinux \
    libsparse \
    libz
include $(BUILD_EXECUTABLE)


include $(CLEAR_VARS)
LOCAL_SRC_FILES := ext2simg.c
LOCAL_MODULE := ext2simg
LOCAL_SHARED_LIBRARIES += \
    libselinux
LOCAL_STATIC_LIBRARIES += \
    libext4_utils_host \
    libsparse_host \
    libz
include $(BUILD_HOST_EXECUTABLE)


include $(CLEAR_VARS)
LOCAL_SRC_FILES := setup_fs.c
LOCAL_MODULE := setup_fs
LOCAL_SHARED_LIBRARIES += libcutils
include $(BUILD_EXECUTABLE)


include $(CLEAR_VARS)
LOCAL_SRC_FILES := ext4fixup_main.c
LOCAL_MODULE := ext4fixup
LOCAL_SHARED_LIBRARIES += \
    libext4_utils \
    libsparse \
    libz
include $(BUILD_EXECUTABLE)


include $(CLEAR_VARS)
LOCAL_SRC_FILES := ext4fixup_main.c
LOCAL_MODULE := ext4fixup
LOCAL_STATIC_LIBRARIES += \
    libext4_utils_host \
    libsparse_host \
    libz
include $(BUILD_HOST_EXECUTABLE)


include $(CLEAR_VARS)
LOCAL_MODULE := mkuserimg.sh
LOCAL_SRC_FILES := mkuserimg.sh
LOCAL_MODULE_CLASS := EXECUTABLES
# We don't need any additional suffix.
LOCAL_MODULE_SUFFIX :=
LOCAL_BUILT_MODULE_STEM := $(notdir $(LOCAL_SRC_FILES))
LOCAL_IS_HOST_MODULE := true
include $(BUILD_PREBUILT)

endif
