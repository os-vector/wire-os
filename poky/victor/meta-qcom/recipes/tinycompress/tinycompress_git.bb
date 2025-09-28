inherit autotools
SUMMARY = "ALSA sound library for compress format"
LICENSE = "BSD & LGPLv2.1"
LIC_FILES_CHKSUM = "file://COPYING;md5=7b60fb27ed2ff685a5c5f41b8b59cca6"

PVR = "v1.1.0"

SRCREV = "e605f5684997565ba50cf9ad57df2a7980b5e327"
SRC_URI = "git://github.com/alsa-project/tinycompress.git;protocol=https;branch=master \
   file://0001-Tinycompress-Update-tinycompress-with-bug-fixes-enha.patch \
   file://0001-Tinycompress-Add-compress_set_next_track_param-funct.patch \
   file://0001-Tinycompress-Add-get_metadata-and-set_metadata-API-s.patch"

EXTRA_OECONF:append = " --with-sanitized-headers=${STAGING_KERNEL_BUILDDIR}/usr/include"

#S = "${UNPACKDIR}/git"

DEPENDS = "virtual/kernel"

do_configure[depends] += "virtual/kernel:do_shared_workdir"
