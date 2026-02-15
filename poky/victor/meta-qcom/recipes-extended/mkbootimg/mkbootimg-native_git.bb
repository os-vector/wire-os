inherit native

DESCRIPTION = "Boot image creation tool from Android"
LICENSE = "Apache-2.0"
LIC_FILES_CHKSUM = "file://${COREBASE}/meta/files/common-licenses/\
${LICENSE};md5=89aea4e17d99a7cacdbeed46a0096b10"
HOMEPAGE = "http://android.git.kernel.org/?p=platform/system/core.git"
PROVIDES = "mkbootimg-native"
#S = "${UNPACKDIR}"
S = "${UNPACKDIR}"
SRC_DIR = "${WORKSPACE}/system/core/mkbootimg"
##UNPACKDIR = "${S}"
DEPENDS = "libmincrypt-native"

PR = "r6"

MY_PN = "mkbootimg"

# Handle do_fetch ourselves...  The automated tools don't work nicely with this...
do_compile() {
	install -d ${S}
	cp -rf ${WORKSPACE}/system/core/mkbootimg/* ${S}/
	cp -f ${THISDIR}/files/makefile ${S}/
	oe_runmake
}

EXTRA_OEMAKE = "INCLUDES='-Imincrypt' LIBS='${libdir}/libmincrypt.a'"
do_configure[noexec] = "1"
do_install() {
	install -d ${D}${bindir}
	install ${MY_PN} ${D}${bindir}
}

NATIVE_INSTALL_WORKS = "1"
