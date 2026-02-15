inherit autotools pkgconfig

DESCRIPTION = "Build Android libmincrypt"
HOMEPAGE = "http://developer.android.com/"
LICENSE = "Apache-2.0"
LIC_FILES_CHKSUM = "file://${COREBASE}/meta/files/common-licenses/\
${LICENSE};md5=89aea4e17d99a7cacdbeed46a0096b10"

PR = "r0"

FILESPATH =+ "${WORKSPACE}/system/core/:"
SRC_URI   = "file://libmincrypt"

S = "${UNPACKDIR}/libmincrypt"

EXTRA_OECONF = " --with-core-includes=${WORKSPACE}/system/core/include"

