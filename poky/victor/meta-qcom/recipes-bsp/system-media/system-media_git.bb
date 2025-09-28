inherit autotools pkgconfig

DESCRIPTION = "system media headers"
HOMEPAGE = "http://developer.android.com/"
LICENSE = "Apache-2.0"
LIC_FILES_CHKSUM = "file://${COREBASE}/meta/files/common-licenses/\
${LICENSE};md5=89aea4e17d99a7cacdbeed46a0096b10"

DEPENDS = "system-core tinyalsa expat"

PROVIDES += " audio-route audio-utils audio-effects"

CFLAGS += "-Wno-error -Wno-implicit-function-declaration -Wno-strict-prototypes"

FILESPATH =+ "${WORKSPACE}/frameworks/:"
SRC_URI   = "file://system/media/"
S = "${UNPACKDIR}/system/media"

EXTRA_OECONF += " --with-glib"

PR = "r2"

FILES:${PN}-dbg    = "${libdir}/.debug/lib*.*"
FILES:${PN}        = "${libdir}/lib*.so.* ${libdir}/pkgconfig/*"
FILES:${PN}-dev    = "${libdir}/lib*.so ${libdir}/lib*.la ${includedir}"
INSANE_SKIP:${PN}-dev += "dev-elf"
