DESCRIPTION = "Start up script for resetting userdata partition to factory state"
HOMEPAGE = "http://codeaurora.org"
LICENSE = "BSD-3-Clause"
LIC_FILES_CHKSUM = "file://${COREBASE}/meta/files/common-licenses/${LICENSE};md5=550794465ba0ec5312d6919e203a55f9"

SRC_URI  = "file://userfs-reset"

S = "${UNPACKDIR}"
#UNPACKDIR = "${S}"

PR = "r1"

inherit systemd

FILES:${PN} += "${systemd_unitdir}/system/"

do_install() {
    if ${@bb.utils.contains('DISTRO_FEATURES', 'systemd', bb.utils.contains('DISTRO_FEATURES', 'userfs-factory-reset', 'true', 'false', d), 'false', d)}
    then
        install -m 0750 ${S}/userfs-reset -D ${D}${bindir}/userfs-reset
    fi
}
