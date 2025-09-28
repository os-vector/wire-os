inherit autotools-brokensep pkgconfig

DESCRIPTION = "Izat api hdr"
PR = "r1"
LICENSE = "BSD-3-Clause"
LIC_FILES_CHKSUM = "file://${COREBASE}/meta/files/common-licenses/\
${LICENSE};md5=550794465ba0ec5312d6919e203a55f9"

FILESPATH =+ "${WORKSPACE}:"
SRC_URI = "file://qcom-opensource/location/izat_api/"
S = "${UNPACKDIR}/qcom-opensource/location/izat_api"

do_configure() {
}

do_compile() {
}

do_install() {
    install -d ${D}${includedir}
    install -m 644 ${S}/*.h ${D}${includedir}
}
