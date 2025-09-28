DESCRIPTION = "Anki Automatic Updates"
LICENSE = "Anki-Inc.-Proprietary"
LIC_FILES_CHKSUM = "file://${COREBASE}/../victor/meta-qcom/files/anki-licenses/\
Anki-Inc.-Proprietary;md5=4b03b8ffef1b70b13d869dbce43e8f09"

DEPENDS += "systemd"

SERVICE_FILE = "update-engine.service"
ONESHOT_SERVICE_FILE = "update-engine-oneshot.service"
TIMER_FILE = "update-engine.timer"

S = "${UNPACKDIR}"
#UNPACKDIR = "${S}"

SRC_URI = "file://${SERVICE_FILE}"
SRC_URI += "file://${ONESHOT_SERVICE_FILE}"
SRC_URI += "file://${TIMER_FILE}"

SYSTEM_DIR = "${D}${systemd_system_unitdir}"

do_install:append () {
   if ${@bb.utils.contains('DISTRO_FEATURES', 'systemd', 'true', 'false', d)}; then
       install -d ${SYSTEM_DIR}/
       for f in ${SERVICE_FILE} ${ONESHOT_SERVICE_FILE} ${TIMER_FILE}; do
	   install -m 0644 ${S}/${f} -D ${SYSTEM_DIR}/${f}
       done
       install -d ${SYSTEM_DIR}/multi-user.target.wants/
       for f in ${ONESHOT_SERVICE_FILE} ${TIMER_FILE}; do
	   ln -sf ${systemd_system_unitdir}/${f} ${SYSTEM_DIR}/multi-user.target.wants/${f}
       done
   fi
}

FILES:${PN} += "${systemd_system_unitdir}"
