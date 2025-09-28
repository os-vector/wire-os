DESCRIPTION = "python script to apply delta updates"
LICENSE = "Apache-2.0"
LIC_FILES_CHKSUM = "file://${COREBASE}/meta/files/common-licenses/\
Apache-2.0;md5=89aea4e17d99a7cacdbeed46a0096b10"

DEPENDS = "python3-pkgutil"
DEPENDS += "python3-numbers"
DEPENDS = "bspatch"

FILESPATH =+ "${WORKSPACE}:"
SRC_URI = "file://external/paycheck"

S = "${UNPACKDIR}/external/paycheck"

do_compile() {
}

do_install() {
   mkdir -p ${D}/usr/bin
   install -c -m 755 ${S}/paycheck.py ${D}/usr/bin
   install -c -m 755 ${S}/update_metadata_pb2.py ${D}/usr/bin
   mkdir -p ${D}/usr/bin/update_payload
   install -c -m 755 ${S}/update_payload/* ${D}/usr/bin/update_payload
}

FILES:${PN} += "usr/bin/paycheck.py"
FILES:${PN} += "usr/bin/update_metadata_pb2.py"
FILES:${PN} += "usr/bin/update_payload"
