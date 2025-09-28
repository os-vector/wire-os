DESCRIPTION = "Install default wifi configuration"
LICENSE = "Anki-Inc.-Proprietary"
LIC_FILES_CHKSUM = "file://${COREBASE}/../victor/meta-qcom/files/anki-licenses/\
Anki-Inc.-Proprietary;md5=4b03b8ffef1b70b13d869dbce43e8f09"

SRC_URI += "file://settings"
S = "${UNPACKDIR}"
#UNPACKDIR = "${S}"

do_install() {
  install -d ${D}/var/lib/connman
  install -m 644 ${S}/settings ${D}/var/lib/connman/
}

FILES:${PN} += "/var/lib/connman"
