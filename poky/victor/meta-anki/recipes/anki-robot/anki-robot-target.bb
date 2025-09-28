DESCRIPTION = "anki-robot.target systemd"
LICENSE = "Anki-Inc.-Proprietary"
LIC_FILES_CHKSUM = "file://${COREBASE}/../victor/meta-qcom/files/anki-licenses/\
Anki-Inc.-Proprietary;md5=4b03b8ffef1b70b13d869dbce43e8f09"

SERVICE_FILE = "anki-robot.target"

SRC_URI = "file://${SERVICE_FILE}"
SRC_URI += "file://anki.sudoers"
SRC_URI += "file://multi-user-done.service"
SRC_URI += "file://start-anki.service"

S = "${UNPACKDIR}"
#UNPACKDIR="${S}"

DEPENDS += "vic-init"
DEPENDS += "vic-robot"
DEPENDS += "vic-anim"
DEPENDS += "vic-engine"
DEPENDS += "vic-cloud"
DEPENDS += "vic-webserver"
# DEPENDS += "anki-audio-init"
# DEPENDS += "vic-christen"

inherit systemd

do_install:append () {
    if ${@bb.utils.contains('DISTRO_FEATURES', 'systemd', 'true', 'false', d)}; then
        install -d ${D}${systemd_unitdir}/system/
        install -d ${D}${systemd_unitdir}/system/multi-user.target.wants
        install -m 0644 ${S}/${SERVICE_FILE} -D ${D}${systemd_unitdir}/system/${SERVICE_FILE}
        install -m 0644 ${S}/multi-user-done.service -D ${D}${systemd_unitdir}/system/multi-user-done.service
        install -m 0644 ${S}/start-anki.service -D ${D}${systemd_unitdir}/system/start-anki.service
        install -d ${D}${systemd_unitdir}/system/anki-robot.target.wants/

        # create a symlink named victor.target for cli alias
        ln -sf ${systemd_unitdir}/system/${SERVICE_FILE} \                                           
            ${D}${systemd_unitdir}/system/victor.target

        ln -sf ${systemd_unitdir}/system/multi-user-done.service \
            ${D}${systemd_unitdir}/system/multi-user.target.wants/multi-user-done.service

        ln -sf ${systemd_unitdir}/system/start-anki.service \
            ${D}${systemd_unitdir}/system/multi-user.target.wants/start-anki.service
   fi
   install -m 0750 -d ${D}${sysconfdir}/sudoers.d
   install -m 0644 ${S}/anki.sudoers -D ${D}${sysconfdir}/sudoers.d/anki
}

FILES:${PN} += "${sysconfdir}/sudoers.d/anki"
FILES:${PN} += "${systemd_unitdir}/system/"
SYSTEMD_SERVICE:${PN} = "${SERVICE_FILE}"
