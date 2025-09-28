DESCRIPTION = "bspatch tool from Android needed for delta updates"
LICENSE = "BSD-3-Clause"
LIC_FILES_CHKSUM = "file://${COREBASE}/meta/files/common-licenses/\
BSD-3-Clause;md5=550794465ba0ec5312d6919e203a55f9"

DEPENDS = "bzip2"

FILESPATH =+ "${WORKSPACE}:"
SRC_URI = "file://external/bspatch"

S = "${UNPACKDIR}/external/bspatch"

TARGET_CPPFLAGS += "-Iinclude"
TARGET_CXXFLAGS += "-std=c++11 -O3 -Wall -Werror -fPIC"

EXTRA_OEMAKE += "USE_BSDIFF=n PREFIX=${D}/usr"

do_install() {
   mkdir -p ${D}/usr/bin
   install -c -m 755 ${S}/bspatch ${D}/usr/bin
}

FILES:${PN} += "usr/bin/bspatch"
INSANE_SKIP:${PN} += "ldflags"
