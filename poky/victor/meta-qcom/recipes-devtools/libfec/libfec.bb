inherit autotools pkgconfig

DESCRIPTION = "Build Android libfec"
HOMEPAGE = "http://developer.android.com/"
LICENSE = "Apache-2.0"
LIC_FILES_CHKSUM = "file://${COREBASE}/meta/files/common-licenses/\
${LICENSE};md5=89aea4e17d99a7cacdbeed46a0096b10"

PR = "r0"

SRC_URI = "${CLO_LA_GIT}/platform/system/extras;protocol=https;nobranch=1;rev=d73dc7f88ce6f3b9fa141615870e4e2db22a7c23;subpath=libfec"
SRC_URI += "file://Add-autotool-make-files-for-libfec.patch"
SRC_URI += "file://Header-reference-change-to-base-from-android-base.patch"

S = "${UNPACKDIR}/libfec"

DEPENDS += "ext4-utils libcutils libfec-rs libcrypto-utils libsquashfs-utils libbase"
DEPENDS:append_class-target = " system-core"

EXTRA_OECONF += "--with-header-includes=${STAGING_INCDIR}"
EXTRA_OECONF_class-native += "--with-header-includes=${STAGING_INCDIR_NATIVE}"
EXTRA_OECONF_class-native += "--with-coreheader-includes=${WORKSPACE}/system/core/include"

BBCLASSEXTEND += "native"
