inherit autotools pkgconfig

DESCRIPTION = "Build Android crypto utils"
HOMEPAGE = "http://developer.android.com/"
LICENSE = "Apache-2.0"
LIC_FILES_CHKSUM = "file://${COREBASE}/meta/files/common-licenses/\
${LICENSE};md5=89aea4e17d99a7cacdbeed46a0096b10"

PR = "r0"

SRC_URI = "${CLO_LA_GIT}/platform/system/core;protocol=https;nobranch=1;rev=fb09a4583339baba8eff9ec52f30710572c9632c;subpath=libcrypto_utils"
SRC_URI += "file://Add-autotool-make-files-for-libcrypto_utils.patch"
SRC_URI += "file://0001-openssl-1.1.1-compatibility.patch"

S = "${UNPACKDIR}/libcrypto_utils"

EXTRA_OECONF_class-native = "--with-header-includes=${S}/include"

DEPENDS += "openssl"

BBCLASSEXTEND += "native"
