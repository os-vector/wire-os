inherit autotools-brokensep

DESCRIPTION = "hardware btvendorhal headers"
HOMEPAGE = "http://codeaurora.org/"
LICENSE = "Apache-2.0"
LIC_FILES_CHKSUM = "file://${COREBASE}/meta/files/common-licenses/\
${LICENSE};md5=89aea4e17d99a7cacdbeed46a0096b10"

FILESPATH =+ "${WORKSPACE}:"
SRC_URI   = "file://vendor/qcom/opensource/bluetooth/"
S = "${UNPACKDIR}/vendor/qcom/opensource/bluetooth"

PR = "r1"

CFLAGS += "-fno-strict-aliasing -fno-tree-vectorize"

DEPENDS = "system-core"
