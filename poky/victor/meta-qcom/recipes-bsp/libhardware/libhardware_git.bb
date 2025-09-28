inherit autotools pkgconfig

DESCRIPTION = "hardware libhardware headers"
HOMEPAGE = "http://codeaurora.org/"
LICENSE = "Apache-2.0"
LIC_FILES_CHKSUM = "file://${COREBASE}/meta/files/common-licenses/\
${LICENSE};md5=89aea4e17d99a7cacdbeed46a0096b10"

FILESPATH =+ "${WORKSPACE}:"
SRC_URI   = "file://hardware/libhardware/"
S = "${UNPACKDIR}/hardware/libhardware"

PR = "r6"

CFLAGS += "-fno-strict-aliasing -fno-tree-vectorize"
CXXFLAGS += "-fno-strict-aliasing -fno-tree-vectorize"

DEPENDS += "libcutils liblog system-core"

EXTRA_OECONF:append_apq8053 = " --enable-sensors"
EXTRA_OECONF:append_concam = " --enable-camera"
EXTRA_OECONF:append_sdm845 = " --enable-sensors"
EXTRA_OECONF:append_sdm845 = " --enable-camera"
EXTRA_OECONF:append_robot-som = " --enable-camera"
