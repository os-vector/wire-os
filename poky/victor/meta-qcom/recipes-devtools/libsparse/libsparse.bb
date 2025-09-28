inherit autotools pkgconfig

DESCRIPTION = "Build Android libsprase"
HOMEPAGE = "http://developer.android.com/"
LICENSE = "Apache-2.0"
LIC_FILES_CHKSUM = "file://${COREBASE}/meta/files/common-licenses/\
${LICENSE};md5=89aea4e17d99a7cacdbeed46a0096b10"

PR = "r0"

DEPENDS += "zlib"

FILESPATH =+ "${WORKSPACE}/system/core/:"
SRC_URI   = "file://libsparse"

S = "${UNPACKDIR}/libsparse"

BBCLASSEXTEND = "native"

EXTRA_OECONF:append_class-native = "  --enable-img-convert-utils"
