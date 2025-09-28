inherit autotools-brokensep pkgconfig

DESCRIPTION = "Libselinux"
LICENSE = "PD"
LIC_FILES_CHKSUM = "file://NOTICE;md5=84b4d2c6ef954a2d4081e775a270d0d0"

PR = "r0"

DEPENDS = "libpcre libmincrypt liblog libcutils"

FILESPATH =+ "${WORKSPACE}:"
SRC_URI = "file://external/libselinux/"
S = "${UNPACKDIR}/external/libselinux"

EXTRA_OECONF = " --with-pcre --with-core-includes=${WORKSPACE}/system/core/include"
