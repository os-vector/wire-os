
LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES := mkbootimg.c
LOCAL_STATIC_LIBRARIES := libmincrypt

ifneq (, $(filter qsd8250_surf qsd8250_ffa, $(TARGET_PRODUCT)))
  LOCAL_CFLAGS += -DSURF8K
endif
ifneq (, $(filter msm7627_surf msm7627_ffa, $(TARGET_PRODUCT)))
  LOCAL_CFLAGS += -DSURF7X2X
endif
ifneq (, $(filter msm7630_surf msm7630_ffa, $(TARGET_PRODUCT)))
  LOCAL_CFLAGS += -DSURF7X30
endif

LOCAL_MODULE := mkbootimg-$(TARGET_PRODUCT)

include $(BUILD_HOST_EXECUTABLE)

$(call dist-for-goals,droid,$(LOCAL_BUILT_MODULE))
