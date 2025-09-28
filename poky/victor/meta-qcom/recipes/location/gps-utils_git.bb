inherit autotools-brokensep qcommon pkgconfig

DESCRIPTION = "GPS Utils"
PR = "r1"
LICENSE = "BSD-3-Clause"
LIC_FILES_CHKSUM = "file://${COREBASE}/meta/files/common-licenses/\
${LICENSE};md5=550794465ba0ec5312d6919e203a55f9"

SRC_DIR = "${WORKSPACE}/hardware/qcom/gps/utils/"
S = "${UNPACKDIR}/hardware/qcom/gps/utils"

DEPENDS = "glib-2.0 loc-pla libcutils"
EXTRA_OECONF = "--with-glib"

