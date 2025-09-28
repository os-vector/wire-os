inherit autotools-brokensep

DESCRIPTION = "Safe integer operation library for C"

LICENSE = "ISC"
LIC_FILES_CHKSUM = "file://${COREBASE}/meta/files/common-licenses/\
${LICENSE};md5=f3b90e78ea0cffb20bf5cca7947a896d"

FILESPATH =+ "${WORKSPACE}:"

BBCLASSEXTEND = " ${@bb.utils.contains('DISTRO_FEATURES', 'nad-stream-update', 'native', '', d)}"

SRC_URI   = "file://external/safe-iop/"
SRC_URI  += "file://autotools.patch"

S = "${UNPACKDIR}/external/safe-iop"

PR = "r0"
