inherit autotools  pkgconfig

DESCRIPTION = "sec-config file for sensors"
LICENSE = "BSD-3-Clause"
LIC_FILES_CHKSUM = "file://${COREBASE}/meta/files/common-licenses/${LICENSE};md5=550794465ba0ec5312d6919e203a55f9"

FILESPATH =+ "${THISDIR}/files:"

SRC_URI   = "file://sec_config"
S = "${UNPACKDIR}"
#UNPACKDIR = "${S}"

do_compile[noexec] = "1"

do_install() {
  install -m 0444 ${S}/sec_config -D ${D}${sysconfdir}/sec_config
}

FILES:${PN} = "${sysconfdir}/*"



