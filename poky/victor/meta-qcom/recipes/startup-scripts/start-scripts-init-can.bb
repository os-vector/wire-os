DESCRIPTION = "Start up script for setting up the can link"
HOMEPAGE = "http://codeaurora.org"
LIC_FILES_CHKSUM = "file://${COREBASE}/meta/files/common-licenses/BSD-3-Clause;md5=550794465ba0ec5312d6919e203a55f9"
LICENSE = "BSD-3-Clause"
inherit update-rc.d

SRC_URI += "file://init-can.sh"
#S = "${UNPACKDIR}"
S = "${UNPACKDIR}"
#UNPACKDIR = "${S}"
SRC_DIR = "${THISDIR}"

PR = "r2"

INITSCRIPT_NAME = "init-can.sh"
INITSCRIPT_PARAMS = "start 45 5 . stop 3 0 1 6 ."

do_install() {
    install -m 0755 ${WORKDIR}/${INITSCRIPT_NAME} -D ${D}${sysconfdir}/init.d/${INITSCRIPT_NAME}
}
