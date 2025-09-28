inherit autotools pkgconfig

DESCRIPTION = "media-hardware headers installation"
HOMEPAGE = "http://developer.android.com/"
LICENSE = "Apache-2.0"
LIC_FILES_CHKSUM = "file://${COREBASE}/meta/files/common-licenses/\
${LICENSE};md5=89aea4e17d99a7cacdbeed46a0096b10"

PR = "r0"

FILESPATH =+ "${WORKSPACE}/frameworks/native/include:"
SRC_URI = "file://media/hardware"
S = "${UNPACKDIR}/media/hardware"

do_compile[noexec] = "1"

do_install (){
    install -d ${D}${includedir}/media/hardware
    install -m 0644 ${S}/*.h ${D}${includedir}/media/hardware/
}

