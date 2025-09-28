inherit qcommon qlicense qprebuilt
DESCRIPTION = "mm-anki-camera and mm-qcamera-daemon"

SRC_DIR = "${WORKSPACE}/camera/mm-camera"
S = "${UNPACKDIR}/camera/mm-camera"

PR = "1"

INSANE_SKIP:${PN} = "dev-so"
INSANE_SKIP:${PN} += "dev-deps"
INSANE_SKIP:${PN} += "debug-files"
INSANE_SKIP:${PN} += "file-rdeps"
INSANE_SKIP:${PN} += "already-stripped"
INSANE_SKIP:${PN} += "ldflags"

PREBUILT = "1"

FILES:${PN} += "usr/bin"
FILES:${PN} += "usr/lib"

# By default .so libs(which are not versioned) are treated as development libraries
# which are not packaged in the release package but instead in the development package
# (securemsm-noship-dev). The following line explicitly overrides what goes in
# the dev package so that anything remaining can go in the release package
# like the .so libs
FILES:${PN}-dev = "${libdir}/*.la"
