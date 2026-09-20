SUMMARY = "awesome motd"
DESCRIPTION = "awesome motd"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COREBASE}/meta/files/common-licenses/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

FILESEXTRAPATHS:prepend := "${THISDIR}/${PN}:"

SRC_URI = " \
    file://vector-motd \
    file://99-motd.sh \
    file://10-wireos-login.conf \
"

S = "${UNPACKDIR}"

do_install() {
    install -d ${D}${sbindir}
    install -d ${D}${sysconfdir}/profile.d

    install -m 0755 ${S}/vector-motd ${D}${sbindir}/vector-motd
    install -m 0644 ${S}/99-motd.sh  ${D}${sysconfdir}/profile.d/99-motd.sh

    # drop the sshd "Last login:" line
    install -d ${D}${sysconfdir}/ssh/sshd_config.d
    install -m 0644 ${S}/10-wireos-login.conf ${D}${sysconfdir}/ssh/sshd_config.d/10-wireos-login.conf
}

FILES:${PN} = "${sbindir}/vector-motd \
               ${sysconfdir}/profile.d/99-motd.sh \
               ${sysconfdir}/ssh/sshd_config.d/10-wireos-login.conf"