inherit autotools pkgconfig

DESCRIPTION = "usbaudio"
LICENSE = "BSD-3-Clause"
LIC_FILES_CHKSUM = "file://${COREBASE}/meta/files/common-licenses/\
${LICENSE};md5=550794465ba0ec5312d6919e203a55f9"

FILESPATH =+ "${WORKSPACE}:"

SRC_URI   = "file://hardware/libhardware/modules/usbaudio"
S = "${UNPACKDIR}/hardware/libhardware/modules/usbaudio"

PR = "r0"

DEPENDS = "tinyalsa system-media libhardware"

FILES:${PN} += "${libdir}/*.so"
INSANE_SKIP:${PN} = "dev-deps"
