PR = "r0"

FILESEXTRAPATHS:prepend := "${THISDIR}/files:"

SRC_URI   = "file://${BASEMACHINE}/system.prop"

DESCRIPTION = "Script to populate system properties"

LICENSE = "BSD-3-Clause"
LIC_FILES_CHKSUM = "file://${COREBASE}/meta/files/common-licenses/\
${LICENSE};md5=550794465ba0ec5312d6919e203a55f9"

S = "${UNPACKDIR}"
#UNPACKDIR = "${S}"

do_compile() {
    # Remove empty lines and lines starting with '#'
    sed -e 's/#.*$//' -e '/^$/d' ${S}/${BASEMACHINE}/system.prop >> ${S}/build.prop
}

do_install() {
    install -d ${D}
    install -m 0644 ${S}/build.prop ${D}/build.prop
}

PACKAGES = "${PN}"
FILES:${PN} += "/build.prop"
