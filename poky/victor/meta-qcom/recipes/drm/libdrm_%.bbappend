FILESPATH =+ "${WORKSPACE}:"
SRC_URI   = "file://display/libdrm"
SRCREV = "${AUTOREV}"
S      = "${WORKDIR}/libdrm"

do_install:append() {
cp -rf ${S}/libdrm_macros.h ${D}${includedir}/libdrm/
}
