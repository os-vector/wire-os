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
endif

AM_CPPFLAGS = -Iinclude \
        -Isys \
        -Idm \
        -Ihh \
        -I../ \
        -I../btcore/include \
        -I../hci/include \
        -I../include \
        -I../stack/include \
        -I../stack/btm \
        -I../udrv/include \
        -I../vnd/include \
        -I../utils/include \
        -I$(WORKSPACE)/vendor/qcom/opensource/bluetooth/system_bt_ext \
        -I${WORKSPACE}/vendor/qcom/opensource/bluetooth/system_bt_ext/bta/include \
        -I${WORKSPACE}/vendor/qcom/opensource/bluetooth/system_bt_ext/bta/ar
c_sources = \
        dm/bta_dm_ci.c \
        dm/bta_dm_act.c \
        dm/bta_dm_pm.c \
        dm/bta_dm_main.c \
        dm/bta_dm_cfg.c \
        dm/bta_dm_api.c \
        dm/bta_dm_sco.c \
        gatt/bta_gattc_api.c \
        gatt/bta_gatts_act.c \
        gatt/bta_gatts_main.c \
        gatt/bta_gattc_utils.c \
        gatt/bta_gatts_api.c \
        gatt/bta_gattc_main.c \
        gatt/bta_gattc_act.c \
        gatt/bta_gattc_cache.c \
        gatt/bta_gatts_utils.c \
        ag/bta_ag_sdp.c \
        ag/bta_ag_sco.c \
        ag/bta_ag_cfg.c \
        ag/bta_ag_main.c \
        ag/bta_ag_api.c \
        ag/bta_ag_rfc.c \
        ag/bta_ag_act.c \
        ag/bta_ag_cmd.c \
        ag/bta_ag_ci.c \
        ag/bta_ag_at.c \
        hf_client/bta_hf_client_act.c \
        hf_client/bta_hf_client_api.c \
        hf_client/bta_hf_client_main.c \
        hf_client/bta_hf_client_rfc.c \
        hf_client/bta_hf_client_at.c \
        hf_client/bta_hf_client_sdp.c \
        hf_client/bta_hf_client_sco.c \
        hf_client/bta_hf_client_cmd.c \
        hh/bta_hh_cfg.c \
        hh/bta_hh_act.c \
        hh/bta_hh_api.c \
        hh/bta_hh_le.c \
        hh/bta_hh_utils.c \
        hh/bta_hh_main.c \
        pan/bta_pan_main.c \
        pan/bta_pan_ci.c \
        pan/bta_pan_act.c \
        pan/bta_pan_api.c \
        av/bta_av_act.c \
        av/bta_av_ci.c \
        av/bta_av_api.c \
        av/bta_av_aact.c \
        av/bta_av_main.c \
        av/bta_av_cfg.c \
        av/bta_av_ssm.c \
        av/bta_av_sbc.c \
        av/bta_av_aac.c \
        ar/bta_ar.c \
        hl/bta_hl_ci.c \
        hl/bta_hl_act.c \
        hl/bta_hl_api.c \
        hl/bta_hl_main.c \
        hl/bta_hl_utils.c \
        hl/bta_hl_sdp.c \
        sdp/bta_sdp_api.c \
        sdp/bta_sdp_act.c \
        sdp/bta_sdp.c \
        sdp/bta_sdp_cfg.c \
        sys/bta_sys_main.c \
        sys/bta_sys_conn.c \
        sys/utl.c \
        jv/bta_jv_act.c \
        jv/bta_jv_cfg.c \
        jv/bta_jv_main.c \
        jv/bta_jv_api.c

c_sources+= \
        ${WORKSPACE}/vendor/qcom/opensource/bluetooth/system_bt_ext/bta/avk/bta_avk_act.c \
        ${WORKSPACE}/vendor/qcom/opensource/bluetooth/system_bt_ext/bta/avk/bta_avk_ci.c \
        ${WORKSPACE}/vendor/qcom/opensource/bluetooth/system_bt_ext/bta/avk/bta_avk_api.c \
        ${WORKSPACE}/vendor/qcom/opensource/bluetooth/system_bt_ext/bta/avk/bta_avk_aact.c \
        ${WORKSPACE}/vendor/qcom/opensource/bluetooth/system_bt_ext/bta/avk/bta_avk_main.c \
        ${WORKSPACE}/vendor/qcom/opensource/bluetooth/system_bt_ext/bta/avk/bta_avk_cfg.c \
        ${WORKSPACE}/vendor/qcom/opensource/bluetooth/system_bt_ext/bta/avk/bta_avk_ssm.c \
        ${WORKSPACE}/vendor/qcom/opensource/bluetooth/system_bt_ext/bta/avk/bta_avk_sbc.c \
        ${WORKSPACE}/vendor/qcom/opensource/bluetooth/system_bt_ext/bta/avk/bta_avk_aac.c \
        ${WORKSPACE}/vendor/qcom/opensource/bluetooth/system_bt_ext/bta/avk/bta_avk_mp3.c\
        ${WORKSPACE}/vendor/qcom/opensource/bluetooth/system_bt_ext/bta/ar/bta_ar_ext.c
common_LDFLAGS = -lpthread  -lrt ../stack/libbtstack.la -lbtif
common_CFLAGS =  $(AM_CFLAGS) -fPIC -D_GNU_SOURCE

library_includedir = $(pkgincludedir)\
library_include_HEADERS = $(h_sources)
lib_LTLIBRARIES = libbtbta.la
libbtbta_la_CC = @CC@
libbtbta_la_SOURCES = $(c_sources)
libbtbta_la_CPPFLAGS =$(AM_CPPFLAGS)
libbtbta_la_CFLAGS = $(common_CFLAGS)
libbtbta_la_LDFLAGS := $(common_LDFLAGS) -static
