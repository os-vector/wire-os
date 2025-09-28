inherit autotools-brokensep pkgconfig

DESCRIPTION = "GPS Loc Platform Library Abstraction"
PR = "r1"
LICENSE = "BSD-3-Clause"
LIC_FILES_CHKSUM = "file://${COREBASE}/meta/files/common-licenses/\
${LICENSE};md5=550794465ba0ec5312d6919e203a55f9"

FILESPATH =+ "${WORKSPACE}:"
SRC_URI = "file://hardware/qcom/gps/utils/platform_lib_abstractions/loc_pla/"
S = "${UNPACKDIR}/hardware/qcom/gps/utils/platform_lib_abstractions/loc_pla"
DEPENDS = "glib-2.0 loc-stub"
EXTRA_OECONF = "--with-glib"

