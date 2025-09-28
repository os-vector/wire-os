inherit autotools pkgconfig

DESCRIPTION = "Build Android libcutils"
HOMEPAGE = "http://developer.android.com/"
LICENSE = "Apache-2.0"
LIC_FILES_CHKSUM = "file://${COREBASE}/meta/files/common-licenses/\
${LICENSE};md5=89aea4e17d99a7cacdbeed46a0096b10"

PR = "r1"

DEPENDS += "liblog"

BBCLASSEXTEND = "native"

FILESPATH =+ "${WORKSPACE}/system/core/:"
SRC_URI   = "file://libcutils"

S = "${UNPACKDIR}/libcutils"

# mdsprpcd complains about execstack
CFLAGS += "-O2 -fPIC"
LDFLAGS += "-Wl,-z,noexecstack"

EXTRA_OECONF += " --with-core-includes=${WORKSPACE}/system/core/include"
EXTRA_OECONF += " --with-host-os=${HOST_OS}"
EXTRA_OECONF += " --disable-static"
#EXTRA_OECONF += " --enable-leproperties"
EXTRA_OECONF += " LE_PROPERTIES_ENABLED=true"

FILES:${PN}-dbg    = "${libdir}/.debug/libcutils.*"
FILES:${PN}        = "${libdir}/libcutils.so.* ${libdir}/pkgconfig/*"
FILES:${PN}-dev    = "${libdir}/libcutils.so ${libdir}/libcutils.la ${includedir}"
