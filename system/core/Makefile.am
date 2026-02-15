BUILD_COMPONENTS :=

# Builds for both host and device
#BUILD_COMPONENTS += base
#BUILD_COMPONENTS += libzarchive

if BUILD_DEVICE 
BUILD_COMPONENTS += logwrapper
if !NO_LIBSYNC
BUILD_COMPONENTS += libsync
endif
BUILD_COMPONENTS += fs_mgr
BUILD_COMPONENTS += adb
if USE_LOGD
BUILD_COMPONENTS += logcat
BUILD_COMPONENTS += libsysutils
BUILD_COMPONENTS += logd
endif
BUILD_COMPONENTS += libbacktrace
BUILD_COMPONENTS += leproperties
BUILD_COMPONENTS += toolbox
BUILD_COMPONENTS += usb
BUILD_COMPONENTS += include
if !NO_DEBUGGERD
BUILD_COMPONENTS += debuggerd
endif
endif

if BUILD_HOST
BUILD_COMPONENTS += fastboot
endif

SUBDIRS := $(BUILD_COMPONENTS)
