inherit systemd
DESCRIPTION = "Anki Crash Log Service"
LICENSE = "Anki-Inc.-Proprietary"
LIC_FILES_CHKSUM = "file://${COREBASE}/../victor/meta-qcom/files/anki-licenses/\
Anki-Inc.-Proprietary;md5=4b03b8ffef1b70b13d869dbce43e8f09"

FILESPATH =+ "${WORKSPACE}:"

SRC_URI += "file://anki/anki-crash-log/"
SRC_URI += "file://anki-crash-log.service"
SRC_URI += "file://anki-crash-log.socket"

S = "${UNPACKDIR}/anki/anki-crash-log"
SYSTEM_DIR = "${D}${sysconfdir}/systemd/system"

do_install() {
  install -d ${D}/usr/bin
  install -m 0755 ${S}/anki-crash-log ${D}/usr/bin/

  if ${@bb.utils.contains('DISTRO_FEATURES', 'systemd', 'true', 'false', d)}; then
    install -d  ${D}${systemd_unitdir}/system/
    install -m 0644 ${UNPACKDIR}/anki-crash-log.service -D ${D}${systemd_unitdir}/system/
    install -m 0644 ${UNPACKDIR}/anki-crash-log.socket -D ${D}${systemd_unitdir}/system/

    install -d ${SYSTEM_DIR}/
    install -d ${SYSTEM_DIR}/sockets.target.wants/

    ln -sf ${systemd_unitdir}/system/anki-crash-log.socket \
      ${SYSTEM_DIR}/sockets.target.wants/
  fi   
}

FILES:${PN} += "/usr/bin/anki-crash-log"
FILES:${PN} += "/usr/lib/systemd/system"

RDEPENDS:${PN} += "bash"
