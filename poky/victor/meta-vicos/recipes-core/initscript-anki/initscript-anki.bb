SUMMARY = "Small init script"
LICENSE = "Anki-Inc.-Proprietary"
LIC_FILES_CHKSUM = "file://${COREBASE}/../victor/meta-qcom/files/anki-licenses/\
Anki-Inc.-Proprietary;md5=4b03b8ffef1b70b13d869dbce43e8f09"
RDEPENDS:${PN} = "rampost busybox"
SRC_URI = "file://rampost-init.sh file://rampost.service file://syscon.dfu"

S = "${UNPACKDIR}"

inherit systemd

SYSTEMD_SERVICE:${PN} = "rampost.service"
SYSTEMD_AUTO_ENABLE = "enable"

do_install() {
        install -d ${D}${bindir}
        install -m 0755 ${S}/rampost-init.sh ${D}${bindir}/rampost-init.sh
        install -d ${D}${sysconfdir}
        install -m 0644 ${S}/syscon.dfu ${D}${sysconfdir}/syscon.dfu
        install -d ${D}${systemd_system_unitdir}
        install -m 0644 ${S}/rampost.service ${D}${systemd_system_unitdir}/rampost.service
}

FILES:${PN} += "${bindir}/rampost-init.sh ${sysconfdir}/syscon.dfu ${systemd_system_unitdir}/rampost.service"
