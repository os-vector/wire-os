DESCRIPTION = "RNDIS network scripts"
HOMEPAGE = "http://codeaurora.org"
LIC_FILES_CHKSUM = "file://${COREBASE}/meta/files/common-licenses/\
${LICENSE};md5=550794465ba0ec5312d6919e203a55f9"
LICENSE = "BSD-3-Clause"

SRC_URI = "file://usbnet"

S = "${UNPACKDIR}"
#UNPACKDIR = "${S}"

PR = "r7"

inherit update-rc.d

INITSCRIPT_NAME = "usbnet"
INITSCRIPT_PARAMS = "start 43 S 2 3 4 5 S . stop 80 0 1 6 ."


do_install() {
        install -m 0755 ${S}/usbnet -D ${D}${sysconfdir}/init.d/usbnet
}

pkg_postinst-${PN} () {
        [ -n "$D" ] && OPT="-r $D" || OPT="-s"
        # remove all rc.d-links potentially created from alternatives
        update-rc.d $OPT -f usbnet remove
        update-rd.d $OPT usbnet start 43 S 2 3 4 5 S . stop 80 0 1 6 .
}
