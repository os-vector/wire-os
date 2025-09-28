inherit autotools

DESCRIPTION = "Tinyalsa Library"
LICENSE = "BSD-3-Clause"
LIC_FILES_CHKSUM = "file://${COREBASE}/meta/files/common-licenses/\
${LICENSE};md5=550794465ba0ec5312d6919e203a55f9"
PR = "r1"

SRCREV = "1369a0ff9979cfbdaa0ca88e4696265655cd198b"
SRC_URI = "git://github.com/tinyalsa/tinyalsa.git;protocol=https;branch=master \
           file://Makefile.am \
           file://configure.ac \
           file://tinyalsa.pc.in \
           file://0001-tinyalsa-Added-avail_min-member.patch \
           file://0001-Add-PCM_FORMAT_INVALID-constant.patch \
           file://0001-pcm-add-support-to-set-silence_size.patch \
           file://0001-tinyalsa-Enable-compilation-with-latest-TinyALSA.patch \
           file://0001-tinyalsa-add-mixer_read-api-to-read-event-informatio.patch \
           file://0001-audio-Add-new-pcm-functions.patch \
           file://0001-tinyalsa-Add-pcm_ioctl-support-for-pcm-driver.patch \
           file://0001-tinyalsa-Fix-tinyplay-runtime-issue.patch \
           file://0001-tinyhostless.patch "

SRC_URI:append_sdxprairie = "file://0001-tinymix_multi.patch \
                             file://0001-tinyplay-lower-threshold-values.patch"

#S = "${UNPACKDIR}"
S = "${UNPACKDIR}"
#UNPACKDIR = "${S}"

EXTRA_OEMAKE = "DEFAULT_INCLUDES=-I${UNPACKDIR}/tinyalsa-git/include/"

DEPENDS = "libcutils"
