DESCRIPTION = "Anki Robot Early Boot Self Test and Orange mode authorization"
LICENSE = "Anki-Inc.-Proprietary"
LIC_FILES_CHKSUM = "file://${COREBASE}/../victor/meta-qcom/files/anki-licenses/\
Anki-Inc.-Proprietary;md5=4b03b8ffef1b70b13d869dbce43e8f09"

FILESPATH =+ "${WORKSPACE}:"

SRC_URI = "file://anki/rampost/"

TARGET_CFLAGS += "-Os -Wall -Wno-unused-result -Wno-strict-aliasing -fPIC"
TARGET_CFLAGS += "${@' -DOSKR' if d.getVar('OSKR') == '1' else ''}"

S = "${UNPACKDIR}/anki/rampost"
##UNPACKDIR = "${S}"

do_compile() {
    oe_runmake CC="${CC}" CFLAGS="${TARGET_CFLAGS}" LDFLAGS="${LDFLAGS}"
}

do_install() {
  install -d ${D}/usr/bin
  install -m 0755 ${S}/rampost ${D}/usr/bin/
}

FILES:${PN} += "/usr/bin/rampost"
