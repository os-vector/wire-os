# sources and intermediate files are separated

AM_CFLAGS = -Wall \
        -Wundef \
        -Wstrict-prototypes \
        -Wno-trigraphs \
        -std=c99 \
        -DHAS_NO_BDROID_BUILDCFG

AM_CPPFLAGS= \
        -I../ \
        -I../bta/include \
        -I../bta/sys \
        -I../bta/dm \
        -I../btcore/include \
        -I../include \
        -I../stack/include \
        -I../stack/l2cap \
        -I../stack/a2dp \
        -I../stack/btm \
        -I../stack/avdt \
        -I../hcis \
        -I../hcis/include \
        -I../hcis/patchram \
        -I../udrv/include \
        -I../btif/include \
        -I../btif/co \
        -I../hci/include\
        -I../vnd/include \
        -I../brcm/include \
        -I../embdrv/sbc/encoder/include \
        -I../embdrv/sbc/decoder/include \
        -I../audio_a2dp_hw \
        -I../utils/include \
        -I${WORKSPACE}/vendor/qcom/opensource/bluetooth/system_bt_ext/bta/include \
        -I${WORKSPACE}/vendor/qcom/opensource/bluetooth/system_bt_ext/bta/ar \
        -I${WORKSPACE}/vendor/qcom/opensource/bluetooth/system_bt_ext/btif/include \
        -I${WORKSPACE}/vendor/qcom/opensource/bluetooth/system_bt_ext/udrv/include \
        -I${WORKSPACE}/external/tinyxml2 \
        -I${WORKSPACE}/external/zlib \
        -I${WORKSPACE}/vendor/qcom/opensource/bluetooth/hal/include \
        -I${WORKSPACE}/vendor/qcom/opensource/bluetooth/vhal/include

if MDM_ROME
AM_CFLAGS += -DBT_SOC_TYPE_ROME
AM_CPPFLAGS += -DBT_SOC_TYPE_ROME
else
if MDM_PRONTO
AM_CFLAGS += -DHCI_USE_MCT
AM_CPPFLAGS += -DHCI_USE_MCT
endif
endif

if MDM_CHEROKEE
AM_CFLAGS += -DBT_SOC_TYPE_CHEROKEE
AM_CPPFLAGS += -DBT_SOC_TYPE_CHEROKEE
endif
# platform specific
c_sources = \
        bte_main.c \
        bte_init.c \
        bte_logmsg.c \
        bte_conf.c \
        stack_config.c \
        ../embdrv/sbc/decoder/srce/alloc.c \
        ../embdrv/sbc/decoder/srce/bitalloc.c \
        ../embdrv/sbc/decoder/srce/bitalloc-sbc.c \
        ../embdrv/sbc/decoder/srce/bitstream-decode.c \
        ../embdrv/sbc/decoder/srce/decoder-oina.c \
        ../embdrv/sbc/decoder/srce/decoder-private.c \
        ../embdrv/sbc/decoder/srce/decoder-sbc.c \
        ../embdrv/sbc/decoder/srce/dequant.c \
        ../embdrv/sbc/decoder/srce/framing.c \
        ../embdrv/sbc/decoder/srce/framing-sbc.c \
        ../embdrv/sbc/decoder/srce/oi_codec_version.c \
        ../embdrv/sbc/decoder/srce/synthesis-sbc.c \
        ../embdrv/sbc/decoder/srce/synthesis-dct8.c \
        ../embdrv/sbc/decoder/srce/synthesis-8-generated.c

# BTC
if BTC_SUPPORT
AM_CFLAGS += -DBTC_INCLUDED=TRUE
AM_CPPFLAGS += -DBTC_INCLUDED=TRUE -I${WORKSPACE}/vendor/qcom/opensource/bluetooth/system_bt_ext/btc/include
c_sources += \
        ${WORKSPACE}/vendor/qcom/opensource/bluetooth/system_bt_ext/btc/src/btc_common.c
endif


# SSR_CLEANUP
if SSR_CLEANUP_SUPPORT
AM_CFLAGS += -DSSR_CLEANUP=TRUE
endif

# sbc encoder
c_sources += \
    ../embdrv/sbc/encoder/srce/sbc_analysis.c \
    ../embdrv/sbc/encoder/srce/sbc_dct.c \
    ../embdrv/sbc/encoder/srce/sbc_dct_coeffs.c \
    ../embdrv/sbc/encoder/srce/sbc_enc_bit_alloc_mono.c \
    ../embdrv/sbc/encoder/srce/sbc_enc_bit_alloc_ste.c \
    ../embdrv/sbc/encoder/srce/sbc_enc_coeffs.c \
    ../embdrv/sbc/encoder/srce/sbc_encoder.c \
    ../embdrv/sbc/encoder/srce/sbc_packing.c

c_sources += \
    ../udrv/ulinux/uipc.c

#if ($(BOARD_USES_WIPOWER), true)
#c_sources += \
#   ../wipowerif/src/wipower.c
#endif

common_LDFLAGS = -ldl -lrt -lresolv -Wl,--whole-archive,../hci/.libs/libbt-hci.a,--no-whole-archive \
                 -Wl,--whole-archive,../bta/.libs/libbtbta.a,--no-whole-archive -Wl,--whole-archive,../btif/.libs/libbtif.a,--no-whole-archive \
                 -Wl,--whole-archive,../stack/.libs/libbtstack.a,--no-whole-archive \
                 -Wl,--whole-archive,../utils/.libs/libbtutils.a,--no-whole-archive -Wl,--whole-archive,../btcore/.libs/libbtcore.a,--no-whole-archive \
                 -Wl,--whole-archive,../osi/.libs/libbtosi.a,--no-whole-archive -Wl,--whole-archive,../device/.libs/libbtdevice.a,--no-whole-archive \
				 -laudioutils

c_sources+= \
        ${WORKSPACE}/vendor/qcom/opensource/bluetooth/system_bt_ext/btif/src/btif_avk_media_task.c \
        ${WORKSPACE}/vendor/qcom/opensource/bluetooth/system_bt_ext/btif/src/btif_avk.c \
        ${WORKSPACE}/vendor/qcom/opensource/bluetooth/system_bt_ext/btif/src/btif_avk_rc.c \
        ${WORKSPACE}/vendor/qcom/opensource/bluetooth/system_bt_ext/btif/src/btif_avk_media_aac.c \
        ${WORKSPACE}/vendor/qcom/opensource/bluetooth/system_bt_ext/btif/src/btif_avk_media_mp3.c \
        ${WORKSPACE}/vendor/qcom/opensource/bluetooth/system_bt_ext/btif/src/btif_avk_media_aptx.c \
        ${WORKSPACE}/vendor/qcom/opensource/bluetooth/system_bt_ext/btif/co/bta_avk_co.c \
        ${WORKSPACE}/vendor/qcom/opensource/bluetooth/system_bt_ext/udrv/ulinux/uipc_avk.c

common_CFLAGS =  $(AM_CFLAGS) -fPIC -DUSE_AUDIO_TRACK -D_GNU_SOURCE $(Z_LIB)

library_includedir = $(pkgincludedir)
lib_LTLIBRARIES = libbluetoothdefault.la
libbluetoothdefault_la_CC = @CC@
libbluetoothdefault_la_SOURCES = $(c_sources)
libbluetoothdefault_la_CPPFLAGS = $(AM_CPPFLAGS)
libbluetoothdefault_la_CFLAGS = $(common_CFLAGS)
libbluetoothdefault_la_LDFLAGS = $(common_LDFLAGS)
