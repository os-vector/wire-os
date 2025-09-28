inherit autotools pkgconfig native

DESCRIPTION = "Android system/core components for linux"
HOMEPAGE = "http://developer.android.com/"
LICENSE = "Apache-2.0"
LIC_FILES_CHKSUM = "file://${COREBASE}/meta/files/common-licenses/\
${LICENSE};md5=89aea4e17d99a7cacdbeed46a0096b10"

PR = "r0"
DEPENDS += "zlib libcutils-native liblog-native libbase-native libsparse-native"
DEPENDS += "${@bb.utils.contains('DISTRO_FEATURES', 'nad-prod', 'libutils-native', '', d)}"

FILESPATH =+ "${WORKSPACE}:"
SRC_URI   = "file://system/core/"

S = "${UNPACKDIR}/system/core"

EXTRA_OECONF = " --with-host-os=${HOST_OS}"
EXTRA_OECONF += " ${@bb.utils.contains('DISTRO_FEATURES', 'nad-stream-update', '--disable-fastboot', '', d)}"
EXTRA_OECONF += " ${@bb.utils.contains('DISTRO_FEATURES', 'nad-stream-update', '--enable-libfsmgr', '', d)}"

#TODO: Install host bins
do_install:append () {
	install -d ${DEPLOY_DIR}/host/linux/bin
	#install ${D}/usr/bin/adb ${DEPLOY_DIR}/host/linux/bin
	#install ${D}/usr/bin/fastboot ${DEPLOY_DIR}/host/linux/bin
}

