DESCRIPTION = "Backlight scripts"
HOMEPAGE = "http://codeaurora.org"
LICENSE = "BSD-3-Clause"
LIC_FILES_CHKSUM = "file://${COREBASE}/meta/files/common-licenses/\
${LICENSE};md5=550794465ba0ec5312d6919e203a55f9"

SRC_URI += "file://backlight"

PR = "r5"

inherit update-rc.d

INITSCRIPT_NAME = "backlight"

do_install() {
        install -m 0755 ${UNPACKDIR}/backlight -D ${D}${sysconfdir}/init.d/backlight
}

pkg_postinst-${PN} () {
        update-alternatives --install ${sysconfdir}/init.d/backlight backlight backlight 60
        [ -n "$D" ] && OPT="-r $D" || OPT="-s"
        # remove all rc.d-links potentially created from alternatives
        update-rc.d $OPT -f backlight remove
        update-rd.d $OPT backlight start 25 S 2 3 4 5 S . stop 80 0 1 6 .
}
