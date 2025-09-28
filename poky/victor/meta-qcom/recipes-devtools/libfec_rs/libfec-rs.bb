inherit autotools pkgconfig

DESCRIPTION = "Build Android fec_rs"
HOMEPAGE = "http://developer.android.com/"
LICENSE = "LGPL-2.1"
LIC_FILES_CHKSUM = "file://${COREBASE}/meta/files/common-licenses/\
${LICENSE};md5=1a6d268fd218675ffea8be556788b780"

PR = "r0"

SRC_URI = "${CLO_LA_GIT}/platform/external/fec;protocol=https;nobranch=1;rev=4342555427f5bb0788dfacd1eb07d2876afab733"
SRC_URI += "file://0001-Add-gnu-autotools-make-files-to-build-libfec_rs.patch"

S = "${UNPACKDIR}/git"

BBCLASSEXTEND += "native"

