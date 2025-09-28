DESCRIPTION = "Provides diag dependent header used by drm provision app for QSEECom"

LICENSE = "Qualcomm-Technologies-Inc.-Proprietary"
LIC_FILES_CHKSUM = "file://${COREBASE}/../victor/meta-qcom/files/qcom-licenses/\
Qualcomm-Technologies-Inc.-Proprietary;md5=92b1d0ceea78229551577d4284669bb8"

PR = "r0"

SRC_URI = "file://${WORKSPACE}/diag"

S = "${UNPACKDIR}/diag"

do_install() {
	mkdir -p ${D}/usr/include
	cp -pPr ${S}/include/msg.h ${D}/usr/include
	cp -pPr ${S}/include/diagcmd.h ${D}/usr/include
	cp -pPr ${S}/include/diagpkt.h ${D}/usr/include
	cp -pPr ${S}/include/msg_pkt_defs.h ${D}/usr/include
	cp -pPr ${S}/include/diag.h ${D}/usr/include
	cp -pPr ${S}/include/msgcfg.h ${D}/usr/include
	cp -pPr ${S}/include/msgtgt.h ${D}/usr/include
	cp -pPr ${S}/include/msg_qsr.h ${D}/usr/include
	cp -pPr ${S}/include/diag_lsm.h ${D}/usr/include
} 
