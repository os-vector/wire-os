DESCRIPTION = "User Data Locker"
LICENSE = "Anki-Inc.-Proprietary"
LIC_FILES_CHKSUM = "file://${COREBASE}/../victor/meta-qcom/files/anki-licenses/\
Anki-Inc.-Proprietary;md5=4b03b8ffef1b70b13d869dbce43e8f09"

FILESPATH =+ "${WORKSPACE}:"
SRC_URI = "file://anki/wire-data-locker/"

S = "${UNPACKDIR}/anki/wire-data-locker"

do_compile[depends] += "virtual/kernel:do_shared_workdir"

do_install() {
  mkdir -p ${D}/usr/bin
  install -m 0100 ${S}/user-data-locker ${D}/usr/bin/
}

FILES:${PN} = "usr/bin/user-data-locker"
