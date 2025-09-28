inherit autotools-brokensep pkgconfig

DESCRIPTION = "GPS Loc Net Iface"
PR = "r5"
LICENSE = "BSD-3-Clause"
LIC_FILES_CHKSUM = "file://${COREBASE}/meta/files/common-licenses/\
${LICENSE};md5=550794465ba0ec5312d6919e203a55f9"

FILESPATH =+ "${WORKSPACE}:"
SRC_URI = "file://qcom-opensource/location/loc_net_iface/"
S = "${UNPACKDIR}/qcom-opensource/location/loc_net_iface"
DEPENDS = "glib-2.0 gps-utils qmi qmi-framework loc-pla data data-items loc-hal"
EXTRA_OECONF = "--with-glib"

PACKAGES = "${PN}"
INHIBIT_PACKAGE_DEBUG_SPLIT = "1"
FILES:${PN} = "${libdir}/*"
INSANE_SKIP:${PN} = "dev-so"
