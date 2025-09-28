inherit autotools pkgconfig

DESCRIPTION = "procrank utility for memory analysis"
HOMEPAGE = "http://developer.android.com/"
LICENSE = "Apache-2.0"
LIC_FILES_CHKSUM = "file://${COREBASE}/meta/files/common-licenses/\
${LICENSE};md5=89aea4e17d99a7cacdbeed46a0096b10"

PR = "r1"

DEPENDS = "libpagemap libcutils"

FILESPATH =+ "${WORKSPACE}/system/extras/:"
SRC_URI = "file://procrank"

S = "${UNPACKDIR}/procrank"

CFLAGS = "-I${STAGING_INCDIR}/pagemap"
CFLAGS += "-I${STAGING_INCDIR}/cutils"

CFLAGS += "-include signal.h"
CFLAGS += "-include stdio.h"
CFLAGS += "-include memory.h"
CFLAGS += "-include unistd.h"
CFLAGS += "-D_GNU_SOURCE"
