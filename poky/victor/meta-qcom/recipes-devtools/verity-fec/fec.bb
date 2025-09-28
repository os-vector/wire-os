inherit autotools pkgconfig

DESCRIPTION = "Build Android fec binary for host"
HOMEPAGE = "http://developer.android.com/"
LICENSE = "Apache-2.0"
LIC_FILES_CHKSUM = "file://${COREBASE}/meta/files/common-licenses/\
${LICENSE};md5=89aea4e17d99a7cacdbeed46a0096b10"

PR = "r0"

SRC_URI = "${CLO_LA_GIT}/platform/system/extras;protocol=https;nobranch=1;rev=d73dc7f88ce6f3b9fa141615870e4e2db22a7c23;subpath=verity/fec;destsuffix=fec"

SRC_URI += "file://GNUAutotoolsProper.patch"
SRC_URI += "file://Including-file.h-from-LE-base-instead-of-Android-s-a.patch"

S = "${UNPACKDIR}/fec"

DEPENDS += "ext4-utils libsparse libfec-rs libcrypto-utils libbase zlib libfec libsquashfs-utils"

CPPFLAGS += "-I${WORKSPACE}/system/core/include"

EXTRA_OECONF_class-native += "--with-header-includes=${WORKSPACE}/system/core/include"

BBCLASSEXTEND += "native"
