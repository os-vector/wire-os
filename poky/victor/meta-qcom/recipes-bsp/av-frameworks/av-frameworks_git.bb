inherit autotools pkgconfig

DESCRIPTION = "Android Multimedia Framework"
HOMEPAGE = "http://developer.android.com/"
LICENSE = "Apache-2.0"
LIC_FILES_CHKSUM = "file://${COREBASE}/meta/files/common-licenses/\
${LICENSE};md5=89aea4e17d99a7cacdbeed46a0096b10"

PR = "r1"

DEPENDS += "binder"
DEPENDS += "camera-metadata"
DEPENDS += "libhardware"
DEPENDS += "liblog"

FILESPATH =+ "${WORKSPACE}:"
SRC_URI   = "file://frameworks/av/"
SRC_URI += "file://automtp.sh"

S = "${UNPACKDIR}/frameworks/av"

EXTRA_OECONF += " --with-kernel-headers=${STAGING_KERNEL_DIR}/include/uapi"

FILES:${PN}-dbg    = "${libdir}/.debug/libcamera_client.* ${bindir}/.debug/*"
FILES:${PN}        = "${libdir}/libcamera_client.so.* ${libdir}/pkgconfig/* ${bindir}/mtpserver ${sysconfdir}/udev/scripts/automtp.sh"
FILES:${PN}-dev    = "${libdir}/libcamera_client.so ${libdir}/libcamera_client.la ${includedir}"

CPPFLAGS += "-I${STAGING_INCDIR}/camera"
CPPFLAGS += "-fpermissive"
LDFLAGS += "-llog"

do_install:append() {
   if ${@bb.utils.contains('DISTRO_FEATURES', 'systemd', 'true', 'false', d)}; then
       install -d ${D}${sysconfdir}/udev/scripts/
       install -m 0744 ${WORKDIR}/automtp.sh \
             ${D}${sysconfdir}/udev/scripts/automtp.sh
   fi
}
