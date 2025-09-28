inherit autotools pkgconfig

DESCRIPTION = "Build Android libsync"
HOMEPAGE = "http://developer.android.com/"
LICENSE = "Apache-2.0"
LIC_FILES_CHKSUM = "file://${COREBASE}/meta/files/common-licenses/\
${LICENSE};md5=89aea4e17d99a7cacdbeed46a0096b10"

PR = "r1"

FILESEXTRAPATHS:prepend := "${THISDIR}/${BPN}:"
SRC_URI = "${CLO_LA_GIT}/platform/system/core/;protocol=https;nobranch=1;rev=8fbe56b11ee7c1f8c87e9b71d89caa306c6cdebb;destsuffix=libsync;subpath=libsync;name=libsync"

SRC_URI += "file://configure.ac;subdir=${BPN}"
SRC_URI += "file://Makefile.am;subdir=${BPN}"
SRC_URI += "file://libsync.pc.in;subdir=${BPN}"

S = "${UNPACKDIR}/libsync"

SRC_URI[libsync.md5sum] = "9b8362e45b5a8256b733ccd8546674b4"
SRC_URI[libsync.sha256sum] = "f4a1c0e077f76b8c509887647c6e979e0e473e54b43a79e3ed156ead92e2742b"

DEPENDS += "liblog"
DEPENDS += "glib-2.0"

EXTRA_OECONF = "--with-glib"

PACKAGES =+ "${PN}-test-bin"

FILES:${PN}-test-bin = "${base_bindir}/*"
