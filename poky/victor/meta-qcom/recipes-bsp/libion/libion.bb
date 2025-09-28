inherit autotools-brokensep pkgconfig

DESCRIPTION = "Build Android libion"
HOMEPAGE = "http://developer.android.com/"
LICENSE = "Apache-2.0"
LIC_FILES_CHKSUM = "file://${COREBASE}/meta/files/common-licenses/\
${LICENSE};md5=89aea4e17d99a7cacdbeed46a0096b10"

PR = "r1"

FILESPATH =+ "${WORKSPACE}/system/core/:"
SRC_URI   = "file://libion"

S = "${UNPACKDIR}/libion"
DEPENDS += "virtual/kernel liblog"

EXTRA_OECONF += " --disable-static"
EXTRA_OECONF += "${@bb.utils.contains_any('PREFERRED_VERSION_linux-msm', '3.18 4.4 4.9', '--with-legacyion', '', d)}"
EXTRA_OECONF += "--with-sanitized-headers=${STAGING_KERNEL_BUILDDIR}/usr/include"

PACKAGES += "${PN}-test-bin"

FILES:${PN}     = "${libdir}/pkgconfig/* ${libdir}/* ${sysconfdir}/*"
FILES:${PN}-test-bin = "${base_bindir}/*"
