DESCRIPTION = "Anki script to wipe all wifi configs from robot"
LICENSE = "Anki-Inc.-Proprietary"
LIC_FILES_CHKSUM = "file://${COREBASE}/../victor/meta-qcom/files/anki-licenses/\
Anki-Inc.-Proprietary;md5=4b03b8ffef1b70b13d869dbce43e8f09"


SRC_URI += "file://wipe-all-wifi-configs"
SRC_URI += "file://wipe-all-wifi-configs.sudoers"

S = "${UNPACKDIR}"
#UNPACKDIR = "${S}"

do_install:append() {
   install -d ${D}/usr/sbin
   install -m 0700 ${S}/wipe-all-wifi-configs ${D}/usr/sbin/wipe-all-wifi-configs
   install -m 0750 -d ${D}${sysconfdir}/sudoers.d
   install -m 0644 ${S}/wipe-all-wifi-configs.sudoers -D ${D}${sysconfdir}/sudoers.d/wipe-all-wifi-configs
}

FILES:${PN} += "${systemd_unitdir}/system/"
FILES:${PN} += "${sysconfdir}/sudoers.d/wipe-all-wifi-configs"
FILES:${PN} += "usr/sbin"

