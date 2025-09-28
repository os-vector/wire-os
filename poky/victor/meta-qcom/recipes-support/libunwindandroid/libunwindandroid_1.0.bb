inherit autotools-brokensep pkgconfig

DESCRIPTION = "Libunwind"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COREBASE}/meta/files/common-licenses/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"
DEPENDS = "libatomic-ops"

LDFLAGS += "-Wl,--allow-multiple-definition"

PR = "r0"
FILESPATH =+ "${WORKSPACE}:"
SRC_URI = "file://external/libunwind/"
S = "${UNPACKDIR}/external/libunwind"
