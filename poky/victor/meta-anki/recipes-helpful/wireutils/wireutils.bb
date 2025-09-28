SUMMARY = "wireutils: simple scripts for debugging"
DESCRIPTION = "installs a couple helpful scripts for dealing with vector"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COREBASE}/meta/files/common-licenses/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

FILESEXTRAPATHS:prepend := "${THISDIR}/${PN}:"

SRC_URI = " \
    file://ddn.sh \
    file://vmesg.sh \
    file://reonboard.sh \
    file://temper.sh \
"

S = "${UNPACKDIR}"
#UNPACKDIR = "${S}"

do_install() {
    install -d ${D}${sbindir}

    install -m 0755 ${S}/ddn.sh         ${D}${sbindir}/ddn
    install -m 0755 ${S}/vmesg.sh	${D}${sbindir}/vmesg
    install -m 0755 ${S}/reonboard.sh   ${D}${sbindir}/reonboard
    install -m 0755 ${S}/temper.sh      ${D}${sbindir}/temper
}

FILES:${PN} = "${sbindir}/ddn \
               ${sbindir}/vmesg \
               ${sbindir}/reonboard \
               ${sbindir}/temper"

RDEPENDS:${PN} = "bash"
