inherit autotools pkgconfig native

DESCRIPTION = "Minimalistic encryption library from Android"
HOMEPAGE = "http://developer.android.com/"
LICENSE = "Apache-2.0"
LIC_FILES_CHKSUM = "file://${COREBASE}/meta/files/common-licenses/\
${LICENSE};md5=89aea4e17d99a7cacdbeed46a0096b10"

PR = "r1"

FILESPATH =+ "${WORKSPACE}/system/core/:"
SRC_URI   = "file://libmincrypt"

S = "${UNPACKDIR}/libmincrypt"

EXTRA_OECONF = " --with-core-includes=${WORKSPACE}/system/core/include"
