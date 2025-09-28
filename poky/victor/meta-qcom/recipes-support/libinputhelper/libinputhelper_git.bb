inherit autotools pkgconfig

DESCRIPTION = "Wrapper library for libinput"
HOMEPAGE = "http://us.codeaurora.org/"
LICENSE = "BSD-3-Clause"
LIC_FILES_CHKSUM = "file://${COREBASE}/meta/files/common-licenses/\
${LICENSE};md5=550794465ba0ec5312d6919e203a55f9"

PR = "r1"

FILESPATH =+ "${WORKSPACE}/frameworks/:"
SRC_URI = "file://input_helper"

S = "${UNPACKDIR}/input_helper"

DEPENDS = "libinput libevdev"

CFLAGS += "-I${STAGING_INCDIR}/libevdev-1.0"
