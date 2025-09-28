DESCRIPTION = "Common headers"
HOMEPAGE = "http://support.cdmatech.com"
LICENSE = "Qualcomm-Technologies-Inc.-Proprietary"
LIC_FILES_CHKSUM = "file://${COREBASE}/../victor/meta-qcom/files/qcom-licenses/\
${LICENSE};md5=92b1d0ceea78229551577d4284669bb8"

PR = "r8"

FILESPATH =+ "${WORKSPACE}:"
SRC_DIR = "${WORKSPACE}/android_compat/common/inc/"
SRC_URI = "file://${@d.getVar('SRC_DIR', True).replace('${WORKSPACE}/', '')}"

S = "${UNPACKDIR}/android_compat/common/inc"

do_install () {
        install -d ${D}${includedir}
        install -m 0644 ${S}/armasm.h ${D}${includedir}/
        install -m 0644 ${S}/comdef.h ${D}${includedir}/
        install -m 0644 ${S}/customer.h ${D}${includedir}/
        install -m 0644 ${S}/stringl.h ${D}${includedir}/
        install -m 0644 ${S}/target.h ${D}${includedir}/
        install -m 0644 ${S}/common_log.h ${D}${includedir}/
        install -m 0644 ${S}/rex.h ${D}${includedir}/
        install -m 0644 ${S}/msm_ipc.h ${D}${includedir}/
        install -m 0644 ${S}/qsocket.h ${D}${includedir}/
        install -m 0644 ${S}/qsocket_ipcr.h ${D}${includedir}/
}

FILES:${PN} += "${includedir}"
FILES:${PN}-dev = ""
