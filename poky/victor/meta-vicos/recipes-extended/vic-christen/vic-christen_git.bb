inherit autotools pkgconfig

DESCRIPTION = "Victor Christening Tool"
LICENSE = "Anki-Inc.-Proprietary"
LIC_FILES_CHKSUM = "file://${COREBASE}/../victor/meta-qcom/files/anki-licenses/\
Anki-Inc.-Proprietary;md5=4b03b8ffef1b70b13d869dbce43e8f09"

FILESPATH =+ "${WORKSPACE}:"
SRC_URI = "file://anki/vic-christen"

S = "${UNPACKDIR}/anki/vic-christen"

DEPENDS += "libcutils"

INSANE_SKIP:${PN} += " ldflags"
