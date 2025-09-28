DESCRIPTION = "neofetch - we all know what it is"
LICENSE = "Anki-Inc.-Proprietary"
LIC_FILES_CHKSUM = "file://${COREBASE}/../victor/meta-qcom/files/anki-licenses/\
Anki-Inc.-Proprietary;md5=4b03b8ffef1b70b13d869dbce43e8f09"

FILESPATH =+ "${WORKSPACE}:"

SRC_URI = " \
      file://neofetch \
      file://vector-ascii \
      "

#S = "${UNPACKDIR}"
S = "${UNPACKDIR}"
#UNPACKDIR = "${S}"

do_install() {
  install -d ${D}/usr/bin
  install -d ${D}${sysconfdir}
  install -m 0755 ${S}/neofetch ${D}/usr/bin/neofetch
  install -m 0755 ${S}/vector-ascii ${D}${sysconfdir}/vector-ascii
}

FILES:${PN} += "/usr/bin/neofetch"
FILES:${PN} += "${sysconfdir}/vector-ascii"

RDEPENDS:${PN} += "bash"
