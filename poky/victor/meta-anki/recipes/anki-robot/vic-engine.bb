DESCRIPTION = "Victor Engine daemon"
LICENSE = "Anki-Inc.-Proprietary"                                                                   
LIC_FILES_CHKSUM = "file://${COREBASE}/../victor/meta-qcom/files/anki-licenses/\                           
Anki-Inc.-Proprietary;md5=4b03b8ffef1b70b13d869dbce43e8f09"

SERVICE_FILE = "vic-engine.service"

SRC_URI = "file://${SERVICE_FILE}"
SRC_URI += "file://engine.sudoers"
SRC_URI += "file://export-gpio.sh"
S = "${UNPACKDIR}"
#UNPACKDIR = "${S}"

inherit systemd

do_install:append () {
   if ${@bb.utils.contains('DISTRO_FEATURES', 'systemd', 'true', 'false', d)}; then
       install -d ${D}${systemd_unitdir}/system/
       install -m 0644 ${S}/${SERVICE_FILE} -D ${D}${systemd_unitdir}/system/${SERVICE_FILE}
   fi
   install -m 0750 -d ${D}${sysconfdir}/sudoers.d
   install -m 0644 ${S}/engine.sudoers -D ${D}${sysconfdir}/sudoers.d/engine
#    install -d ${D}/sbin
#    install -m 0700 ${WORKDIR}/export-gpio.sh ${D}/sbin/export-gpio
}

FILES:${PN} += "${sysconfdir}/sudoers.d/engine"
FILES:${PN} += "${systemd_unitdir}/system/"
SYSTEMD_SERVICE:${PN} = "${SERVICE_FILE}"
