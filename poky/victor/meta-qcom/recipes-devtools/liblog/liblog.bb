inherit autotools-brokensep pkgconfig

DESCRIPTION = "Build Android liblog"
HOMEPAGE = "http://developer.android.com/"
LICENSE = "Apache-2.0"
LIC_FILES_CHKSUM = "file://${COREBASE}/meta/files/common-licenses/\
${LICENSE};md5=89aea4e17d99a7cacdbeed46a0096b10"

PR = "r1"

FILESPATH =+ "${WORKSPACE}/system/core/:"
SRC_URI   = "file://liblog"
SRC_URI  += "file://50-log.rules"

S = "${UNPACKDIR}/liblog"
##UNPACKDIR = "${S}"

BBCLASSEXTEND = "native"

EXTRA_OECONF += " --with-core-includes=${WORKSPACE}/system/core/include"
EXTRA_OECONF += " --disable-static"
EXTRA_OECONF:append_class-target = " --with-logd-logging"

do_install:append() {
    if [ "${CLASSOVERRIDE}" = "class-target" ]; then
       install -m 0644 -D ${UNPACKDIR}/50-log.rules ${D}${sysconfdir}/udev/rules.d/50-log.rules
    fi
}

FILES:${PN}-dbg = "${libdir}/.debug/* ${bindir}/.debug/*"
FILES:${PN}     = "${libdir}/pkgconfig/* ${libdir}/* ${sysconfdir}/*"
FILES:${PN}-dev = "${libdir}/*.so ${libdir}/*.la ${includedir}/*"
