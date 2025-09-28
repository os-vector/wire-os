inherit systemd
DESCRIPTION = "Animation process on failure error code service"
LICENSE = "Anki-Inc.-Proprietary"
LIC_FILES_CHKSUM = "file://${COREBASE}/../victor/meta-qcom/files/anki-licenses/\
Anki-Inc.-Proprietary;md5=4b03b8ffef1b70b13d869dbce43e8f09"

FILESPATH =+ "${WORKSPACE}:"

SRC_URI = "file://anki/rampost/ file://animfail.service"

TARGET_CFLAGS += "-Os -Wall -Wno-error -Wno-unused-result -Wno-strict-aliasing -fPIC"


S = "${UNPACKDIR}/anki/rampost"

do_install() {
  install -d ${D}/usr/bin
  install -m 0755 ${S}/animfail ${D}/usr/bin/

  install -d  ${D}${systemd_unitdir}/system/
  install -m 0644 ${UNPACKDIR}/animfail.service -D ${D}${systemd_unitdir}/system/animfail.service
}

FILES:${PN} += "usr/bin/animfail"
FILES:${PN} += "usr/lib/systemd/system"
