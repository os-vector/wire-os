SUMMARY = "libpvictor"
DESCRIPTION = "Tiny library for communication with the Anki Vector's hardware without Anki code"
HOMEPAGE = "https://github.com/os-vector/libpvictor"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://LICENSE;md5=0eeb98f19a3068438a3d66106c9c9429"

SRCREV = "65339fa6c5406eeda633a3f0d556ee6261ffc49a"
SRC_URI = "git://github.com/os-vector/libpvictor;branch=main;protocol=https"

inherit cmake

RDEPENDS:${PN} += "libatomic"

BB_FETCH_PREMIRRORONLY = "0"
