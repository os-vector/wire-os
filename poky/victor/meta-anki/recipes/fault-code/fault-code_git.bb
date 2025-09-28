inherit systemd
DESCRIPTION = "Animation process on failure error code service"
LICENSE = "Anki-Inc.-Proprietary"
LIC_FILES_CHKSUM = "file://${COREBASE}/../victor/meta-qcom/files/anki-licenses/\
Anki-Inc.-Proprietary;md5=4b03b8ffef1b70b13d869dbce43e8f09"

FILESPATH =+ "${WORKSPACE}:"

SRC_URI = "file://anki/fault-code/ file://fault-code.service file://fault-code.socket"

S = "${UNPACKDIR}/anki/fault-code"

#UNPACKDIR = "${WORKDIR}/sources"
SYSTEM_DIR = "${D}${sysconfdir}/systemd/system"

do_install() {
  install -d ${D}/usr/bin
  install -m 0755 ${S}/fault-code-handler ${D}/usr/bin
  install -m 0755 ${S}/fault-code-clear ${D}/usr/bin

  if ${@bb.utils.contains('DISTRO_FEATURES', 'systemd', 'true', 'false', d)}; then
    install -d  ${D}${systemd_unitdir}/system/
    install -m 0644 ${UNPACKDIR}/fault-code.service -D ${D}${systemd_unitdir}/system/fault-code.service
    install -m 0644 ${UNPACKDIR}/fault-code.socket -D ${D}${systemd_unitdir}/system/fault-code.socket

    install -d ${SYSTEM_DIR}/
    install -d ${SYSTEM_DIR}/sockets.target.wants/

    ln -sf ${systemd_unitdir}/system/fault-code.socket \
      ${SYSTEM_DIR}/sockets.target.wants/fault-code.socket
  fi   
}

FILES:${PN} += "/usr/bin/fault-code-handler"
FILES:${PN} += "/usr/bin/fault-code-clear"
FILES:${PN} += "/usr/lib/systemd/system"

RDEPENDS:${PN} += "bash"
