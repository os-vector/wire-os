inherit autotools-brokensep

DESCRIPTION = "hardware libhardware headers"
HOMEPAGE = "http://codeaurora.org/"
LICENSE = "Apache-2.0"
LIC_FILES_CHKSUM = "file://${COREBASE}/meta/files/common-licenses/\
${LICENSE};md5=89aea4e17d99a7cacdbeed46a0096b10"

FILESPATH =+ "${WORKSPACE}:"
SRC_URI   = "file://hardware/libhardware/"
S = "${UNPACKDIR}/hardware/libhardware"

PR = "r6"

DEPENDS = "system-core"

CFLAGS:append = "-I${WORKSPACE}/poky/build/tmp-glibc/work/armv7a-neon-vfpv4-oe-linux-gnueabi/libhardware/git/hardware/libhardware/include/"
CXXFLAGS:append = "-I${WORKSPACE}/poky/build/tmp-glibc/work/armv7a-neon-vfpv4-oe-linux-gnueabi/libhardware/git/hardware/libhardware/include/"

#do_compile:prepend() {
# /home/kerigan/projects/wire-os/poky/build/tmp-glibc/work/armv7a-neon-vfpv4-oe-linux-gnueabi/libhardware/git/recipe-sysroot/usr/include
#  /home/kerigan/projects/wire-os/hardware/libhardware/include/hardware/
#	cp -r ${WORKSPACE}/hardware/libhardware/include/hardware ${WORKSPACE}/poky/build/tmp-glibc/work/armv7a-neon-vfpv4-oe-linux-gnueabi/libhardware/git/recipe-sysroot/usr/include/
#}

do_install:append () {
        install -d ${D}${includedir}
        install -m 0644 ${S}/include/hardware/gps.h -D ${D}${includedir}/hardware/gps.h
        install -m 0644 ${S}/include/hardware/hardware.h -D ${D}${includedir}/hardware/hardware.h
        install -m 0644 ${S}/include/hardware/hwcomposer.h -D ${D}${includedir}/hardware/hwcomposer.h
        install -m 0644 ${S}/include/hardware/hwcomposer_defs.h -D ${D}${includedir}/hardware/hwcomposer_defs.h
        install -m 0644 ${S}/include/hardware/gralloc.h -D ${D}${includedir}/hardware/gralloc.h
        install -m 0644 ${S}/include/hardware/fused_location.h -D ${D}${includedir}/hardware/fused_location.h
        install -m 0644 ${S}/include/hardware/camera.h -D ${D}${includedir}/hardware/camera.h
        install -m 0644 ${S}/include/hardware/camera3.h -D ${D}${includedir}/hardware/camera3.h
        install -m 0644 ${S}/include/hardware/camera_common.h -D ${D}${includedir}/hardware/camera_common.h
        install -m 0644 ${S}/include/hardware/fb.h -D ${D}${includedir}/hardware/fb.h
        install -m 0644 ${S}/include/hardware/power.h -D ${D}${includedir}/hardware/power.h
        install -m 0644 ${S}/include/hardware/audio.h -D ${D}${includedir}/hardware/audio.h
        install -m 0644 ${S}/include/hardware/sound_trigger.h -D ${D}${includedir}/hardware/sound_trigger.h
        install -m 0644 ${S}/include/hardware/audio_alsaops.h -D ${D}${includedir}/hardware/audio_alsaops.h
        install -m 0644 ${S}/include/hardware/audio_effect.h -D ${D}${includedir}/hardware/audio_effect.h
        install -m 0644 ${S}/include/hardware/audio_policy.h -D ${D}${includedir}/hardware/audio_policy.h
        install -m 0644 ${S}/modules/gralloc/gralloc_priv.h -D ${D}${includedir}/
        install -m 0644 ${S}/include/hardware/sensors.h -D ${D}${includedir}/hardware/sensors.h
}
