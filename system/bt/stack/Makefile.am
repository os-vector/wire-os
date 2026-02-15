# sources and intermediate files are separated

AM_CFLAGS = -Wall \
        -Wundef \
        -Wstrict-prototypes \
        -Wno-trigraphs \
        -std=c99 \
        -DHAS_NO_BDROID_BUILDCFG
if MDM_CHEROKEE
AM_CFLAGS += -DBTA_AV_SPLIT_A2DP_ENABLED
endif

AM_CPPFLAGS = \
        -I.. \
        -I../include \
        -Iinclude \
        -Iavct \
        -Ibtm \
        -Iavrc \
        -Il2cap \
        -Iavdt \
        -Igatt \
        -Igap \
        -Ipan \
        -Ibnep \
        -Ihid \
        -Isdp \
        -Ismp \
        -Isrvc \
        -I../btcore/include \
        -I../vnd/include \
        -I../vnd/ble \
        -I../btif/include \
        -I../hci/include \
        -I../include \
        -I../udrv/include \
        -I../rpc/include \
        -I../hcis \
        -I../ctrlr/include \
        -I../bta/include \
        -I../bta/sys \
        -I../utils/include \
        -I$(WORKSPACE)/vendor/qcom/opensource/bluetooth/system_bt_ext \
        -I${WORKSPACE}/vendor/qcom/opensource/bluetooth/system_bt_ext/bta/ar


c_sources:= \
    ./a2dp/a2d_api.c \
    ./a2dp/a2d_sbc.c \
    ./a2dp/a2d_aptx.c \
    ./a2dp/a2d_aptx_hd.c \
    ./a2dp/a2d_aac.c \
    ./a2dp/a2d_mp3.c \
    ./avrc/avrc_api.c \
    ./avrc/avrc_sdp.c \
    ./avrc/avrc_opt.c \
    ./avrc/avrc_bld_tg.c \
    ./avrc/avrc_bld_ct.c \
    ./avrc/avrc_pars_tg.c \
    ./avrc/avrc_pars_ct.c \
    ./avrc/avrc_utils.c \
    ./hid/hidh_api.c \
    ./hid/hidh_conn.c \
    ./bnep/bnep_main.c \
    ./bnep/bnep_utils.c \
    ./bnep/bnep_api.c \
    ./hcic/hciblecmds.c \
    ./hcic/hcicmds.c \
    ./btm/btm_ble.c \
    ./btm/btm_sec.c \
    ./btm/btm_inq.c \
    ./btm/btm_ble_addr.c \
    ./btm/btm_ble_bgconn.c \
    ./btm/btm_main.c \
    ./btm/btm_dev.c \
    ./btm/btm_ble_gap.c \
    ./btm/btm_ble_adv_filter.c \
    ./btm/btm_ble_multi_adv.c \
    ./btm/btm_ble_batchscan.c \
    ./btm/btm_ble_cont_energy.c \
    ./btm/btm_ble_privacy.c \
    ./btm/btm_acl.c \
    ./btm/btm_sco.c \
    ./btm/btm_pm.c \
    ./btm/btm_devctl.c \
    ./rfcomm/rfc_utils.c \
    ./rfcomm/port_rfc.c \
    ./rfcomm/rfc_l2cap_if.c \
    ./rfcomm/rfc_mx_fsm.c \
    ./rfcomm/port_utils.c \
    ./rfcomm/rfc_port_fsm.c \
    ./rfcomm/rfc_port_if.c \
    ./rfcomm/port_api.c \
    ./rfcomm/rfc_ts_frames.c \
    ./mcap/mca_dact.c \
    ./mcap/mca_dsm.c \
    ./mcap/mca_l2c.c \
    ./mcap/mca_main.c \
    ./mcap/mca_csm.c \
    ./mcap/mca_cact.c \
    ./mcap/mca_api.c \
    ./gatt/gatt_sr.c \
    ./gatt/gatt_cl.c \
    ./gatt/gatt_api.c \
    ./gatt/gatt_auth.c \
    ./gatt/gatt_utils.c \
    ./gatt/gatt_main.c \
    ./gatt/att_protocol.c \
    ./gatt/gatt_attr.c \
    ./gatt/gatt_db.c \
    ./avct/avct_api.c \
    ./avct/avct_l2c.c \
    ./avct/avct_lcb.c \
    ./avct/avct_ccb.c \
    ./avct/avct_lcb_act.c \
    ./smp/smp_main.c \
    ./smp/smp_l2c.c \
    ./smp/smp_cmac.c \
    ./smp/smp_utils.c \
    ./smp/smp_act.c \
    ./smp/smp_keys.c \
    ./smp/smp_api.c \
    ./smp/aes.c \
    ./smp/smp_br_main.c\
    ./smp/p_256_curvepara.c \
    ./smp/p_256_ecc_pp.c \
    ./smp/p_256_multprecision.c \
    ./avdt/avdt_ccb.c \
    ./avdt/avdt_scb_act.c \
    ./avdt/avdt_msg.c \
    ./avdt/avdt_ccb_act.c \
    ./avdt/avdt_api.c \
    ./avdt/avdt_scb.c \
    ./avdt/avdt_ad.c \
    ./avdt/avdt_l2c.c \
    ./sdp/sdp_server.c \
    ./sdp/sdp_main.c \
    ./sdp/sdp_db.c \
    ./sdp/sdp_utils.c \
    ./sdp/sdp_api.c \
    ./sdp/sdp_discovery.c \
    ./pan/pan_main.c \
    ./srvc/srvc_battery.c \
    ./srvc/srvc_dis.c \
    ./srvc/srvc_eng.c \
    ./pan/pan_api.c \
    ./pan/pan_utils.c \
    ./btu/btu_hcif.c \
    ./btu/btu_init.c \
    ./btu/btu_task.c \
    ./l2cap/l2c_fcr.c \
    ./l2cap/l2c_ucd.c \
    ./l2cap/l2c_main.c \
    ./l2cap/l2c_api.c \
    ./l2cap/l2c_utils.c \
    ./l2cap/l2c_csm.c \
    ./l2cap/l2c_link.c \
    ./l2cap/l2c_ble.c \
    ./l2cap/l2cap_client.c \
    ./gap/gap_api.c \
    ./gap/gap_ble.c \
    ./gap/gap_conn.c \
    ./gap/gap_utils.c

if SSR_CLEANUP_SUPPORT
AM_CFLAGS += -DSSR_CLEANUP=TRUE
endif

common_LDFLAGS = -lpthread
common_CFLAGS =  $(AM_CFLAGS) -fPIC -D_GNU_SOURCE

library_includedir = $(pkgincludedir)\
library_include_HEADERS = $(h_sources)
lib_LTLIBRARIES = libbtstack.la
libbtstack_la_CC = @CC@
libbtstack_la_SOURCES = $(c_sources)
libbtstack_la_CPPFLAGS = $(AM_CPPFLAGS)
libbtstack_la_CFLAGS = $(common_CFLAGS)
libbtstack_la_LDFLAGS = $(common_LDFLAGS) -static
