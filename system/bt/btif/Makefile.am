# sources and intermediate files are separated

AM_CFLAGS = -Wall \
        -Wundef \
        -Wstrict-prototypes \
        -Wno-trigraphs \
        -std=c99 \
        -DHAS_NO_BDROID_BUILDCFG
if MDM_CHEROKEE
AM_CFLAGS += -DBTA_AV_SPLIT_A2DP_ENABLED
AM_CFLAGS += -DBTA_AV_SPLIT_A2DP_DEF_FREQ_48KHZ
AM_CFLAGS += -DAPTX_48000
endif

AM_CPPFLAGS = \
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
        -I${WORKSPACE}/external/tinyxml2 \
        -I${WORKSPACE}/external/zlib \
        -I${WORKSPACE}/vendor/qcom/opensource/bluetooth/vhal/include \
        -I$(WORKSPACE)/frameworks/av/include \
        -I$(WORKSPACE)/system/media/audio/include \
        -I$(WORKSPACE)/system/media/audio_utils/include \
        -I$(WORKSPACE)/system/core/include

c_sources = \
        src/bluetooth.c \
        src/btif_av.c \
        src/btif_config.c \
        src/btif_core.c \
        src/btif_debug.c \
        src/btif_debug_btsnoop.c \
        src/btif_debug_conn.c \
        src/btif_dm.c \
        src/btif_gatt.c \
        src/btif_gatt_client.c \
        src/btif_gatt_multi_adv_util.c \
        src/btif_gatt_server.c \
        src/btif_gatt_test.c \
        src/btif_gatt_util.c \
        src/btif_hf.c \
        src/btif_hf_client.c \
        src/btif_hh.c \
        src/btif_hl.c \
        src/btif_sdp.c \
        src/btif_media_task.c \
        src/btif_pan.c \
        src/btif_profile_queue.c \
        src/btif_rc.c \
        src/btif_sm.c \
        src/btif_sock.c \
        src/btif_sock_rfc.c \
        src/btif_sock_l2cap.c \
        src/btif_sock_sco.c \
        src/btif_sock_sdp.c \
        src/btif_sock_thread.c \
        src/btif_sdp_server.c \
        src/btif_sock_util.c \
        src/btif_storage.c \
        src/btif_uid.c \
        src/btif_util.c \
        src/stack_manager.c \
        src/btif_stack_log.c \
        src/btif_rfcomm.c \
        src/btif_l2cap.c \
        src/btif_vendor.c \
        src/btif_gap.c \
        src/btif_smp.c \
        co/bta_ag_co.c \
        co/bta_dm_co.c \
        co/bta_av_co.c \
        co/bta_hh_co.c \
        co/bta_hl_co.c \
        co/bta_pan_co.c \
        co/bta_gatts_co.c

common_LDFLAGS = -lpthread  -lrt -lresolv ../bta/libbtbta.la ../osi/libbtosi.la \
                 ../device/libbtdevice.la

common_CFLAGS =  $(AM_CFLAGS) -D_GNU_SOURCE $(Z_LIB) -fPIC

library_includedir = $(pkgincludedir)\
library_include_HEADERS = $(h_sources)
lib_LTLIBRARIES = libbtif.la
libbtif_la_CC = @CC@
libbtif_la_SOURCES = $(c_sources)
libbtif_la_CPPFLAGS = $(AM_CPPFLAGS)
libbtif_la_CFLAGS = $(common_CFLAGS)
libbtif_la_LDFLAGS = $(common_LDFLAGS) -static
