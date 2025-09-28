inherit qcommon qlicense qprebuilt
DESCRIPTION = "OpenCL for our silly Adreno"

SRC_DIR = "${WORKSPACE}/adreno"
S = "${UNPACKDIR}/adreno"

PR = "1"

INSANE_SKIP:${PN} = "dev-so"
INSANE_SKIP:${PN} += "dev-deps"
INSANE_SKIP:${PN} += "debug-files"
INSANE_SKIP:${PN} += "file-rdeps"
INSANE_SKIP:${PN} += "already-stripped"
INSANE_SKIP:${PN} += "ldflags"

PREBUILT = "1"

FILES:${PN} += "usr/lib"

# By default .so libs(which are not versioned) are treated as development libraries
# which are not packaged in the release package but instead in the development package
# (securemsm-noship-dev). The following line explicitly overrides what goes in
# the dev package so that anything remaining can go in the release package
# like the .so libs
FILES:${PN}-dev = "${libdir}/*.la"
