inherit autotools pkgconfig

DESCRIPTION = "libpagemap"
HOMEPAGE = "http://developer.android.com/"
LICENSE = "Apache-2.0"
LIC_FILES_CHKSUM = "file://${COREBASE}/meta/files/common-licenses/\
${LICENSE};md5=89aea4e17d99a7cacdbeed46a0096b10"

PR = "r1"

FILESPATH =+ "${WORKSPACE}/system/extras/:"
SRC_URI = "file://libpagemap"

S = "${UNPACKDIR}/libpagemap"
