LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)


LOCAL_SRC_FILES:= gatt_test.c

LOCAL_C_INCLUDES += . \
         $(LOCAL_PATH)/../../stack/include \
         $(LOCAL_PATH)/../../include \
         $(LOCAL_PATH)/../../stack/l2cap \
         $(LOCAL_PATH)/../../utils/include \
         $(LOCAL_PATH)/../../ \
         $(LOCAL_PATH)/btif/include \
         $(LOCAL_PATH)/../../stack/gatt \
         $(LOCAL_PATH)/../../stack/btm \
         $(LOCAL_PATH)/../../stack/avdt \
         $(LOCAL_PATH)/../../stack/btm \
         $(LOCAL_PATH)/../../hcis \
         $(LOCAL_PATH)/../../hci/include \
         $(LOCAL_PATH)/../../bta/include \
         $(LOCAL_PATH)/../../bta/sys \
         $(LOCAL_PATH)/../../osi/include \
         $(bluetooth_C_INCLUDES)

LOCAL_CFLAGS += $(bluetooth_CFLAGS)
LOCAL_CONLYFLAGS += $(bluetooth_CONLYFLAGS)
LOCAL_CPPFLAGS += $(bluetooth_CPPFLAGS)
LOCAL_MODULE_PATH := $(TARGET_OUT_EXECUTABLES)
LOCAL_MODULE_TAGS := debug optional
LOCAL_MODULE:= gatt_test

#LOCAL_LDLIBS +=  -ldl -llog
#LIBS_c += -lreadline

LOCAL_SHARED_LIBRARIES += libcutils   \
                          libutils    \
                          libhardware \
                          libhardware_legacy

LOCAL_MULTILIB := 32

include $(BUILD_EXECUTABLE)
