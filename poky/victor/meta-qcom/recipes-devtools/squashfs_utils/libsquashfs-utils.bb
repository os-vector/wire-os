inherit autotools pkgconfig

DESCRIPTION = "Build Android libsquashfs utils"
HOMEPAGE = "http://developer.android.com/"
LICENSE = "Apache-2.0"
LIC_FILES_CHKSUM = "file://${COREBASE}/meta/files/common-licenses/\
${LICENSE};md5=89aea4e17d99a7cacdbeed46a0096b10"

PR = "r0"

SRC_URI = "${CLO_LA_GIT}/platform/system/extras;protocol=https;nobranch=1;rev=d73dc7f88ce6f3b9fa141615870e4e2db22a7c23;subpath=squashfs_utils"
SRC_URI += "${CLO_LA_GIT}/platform/external/squashfs-tools;protocol=https;nobranch=1;rev=5b4709ba460869ed66f971115acfa88425a3faf8;subpath=squashfs-tools"
SRC_URI += "file://Add-autotool-make-files-for-libsquashfs_utils.patch"

S = "${UNPACKDIR}/squashfs_utils"

DEPENDS += "libcutils"

EXTRA_OECONF += "--with-squashfstools-includes=${WORKDIR}/squashfs-tools"
EXTRA_OECONF_class-native += "--with-squashfstools-includes=${WORKDIR}/squashfs-tools"

BBCLASSEXTEND += "native"
