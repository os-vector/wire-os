DESCRIPTION = "Anki developer script to update OS on robot"
LICENSE = "Anki-Inc.-Proprietary"
LIC_FILES_CHKSUM = "file://${COREBASE}/../victor/meta-qcom/files/anki-licenses/\
Anki-Inc.-Proprietary;md5=4b03b8ffef1b70b13d869dbce43e8f09"

SRC_URI += "file://update-os.sh"

S = "${UNPACKDIR}"
#UNPACKDIR = "${S}"

do_install:append() {
   install -d ${D}/usr/sbin
   install -m 0700 ${UNPACKDIR}/update-os.sh ${D}/usr/sbin/update-os
}

FILES:${PN} = "usr/sbin/update-os"
