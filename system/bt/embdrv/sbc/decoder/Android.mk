LOCAL_PATH:= $(call my-dir)

# Bluetooth SBC decoder static library for target
# ========================================================
include $(CLEAR_VARS)

# sbc decoder
LOCAL_SRC_FILES+= \
        ./srce/alloc.c \
        ./srce/bitalloc.c \
        ./srce/bitalloc-sbc.c \
        ./srce/bitstream-decode.c \
        ./srce/decoder-oina.c \
        ./srce/decoder-private.c \
        ./srce/decoder-sbc.c \
        ./srce/dequant.c \
        ./srce/framing.c \
        ./srce/framing-sbc.c \
        ./srce/oi_codec_version.c \
        ./srce/synthesis-sbc.c \
        ./srce/synthesis-dct8.c \
        ./srce/synthesis-8-generated.c \

LOCAL_C_INCLUDES += $(LOCAL_PATH)/include
LOCAL_C_INCLUDES += $(LOCAL_PATH)/srce

LOCAL_MODULE:= libbt-qcom_sbc_decoder
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_CLASS := STATIC_LIBRARIES

LOCAL_CFLAGS += $(bluetooth_CFLAGS)
LOCAL_CONLYFLAGS += $(bluetooth_CONLYFLAGS)
LOCAL_CPPFLAGS += $(bluetooth_CPPFLAGS)

include $(BUILD_STATIC_LIBRARY)
