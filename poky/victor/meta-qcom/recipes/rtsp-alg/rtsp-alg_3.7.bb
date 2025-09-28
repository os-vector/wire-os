inherit autotools-brokensep module qperf
DESCRIPTION = "RSTP ALG"
LICENSE = "GPLv2"
LIC_FILES_CHKSUM = "file://nf_nat_rtsp.c;beginline=2;endline=30;md5=1d494e4d4253d1fcdcbfe334f8a4cd0a"

do_unpack[deptask] = "do_populate_sysroot"
PR = "r6"

# This DEPENDS is to serialize kernel module builds
DEPENDS = "alx virtual/kernel"


SRC_URI = "http://github.com/maru-sama/rtsp-linux/archive/${PV}.tar.gz \
		file://Makefile.patch \
"
SRC_URI[md5sum]    = "5cc2be642a0d6ff8817d72d459e76606"
SRC_URI[sha256sum] = "bd14b5f8f0bc8db3db93735b2a7eca2790454c2dc200d95becd283b043b8b94d"


S = "${UNPACKDIR}/rtsp-linux-${PV}"

EXTRA_OEMAKE = "KSOURCE=${STAGING_KERNEL_DIR}"

do_install() {
        install -d ${D}${base_libdir}/modules/${KERNEL_VERSION}/kernel/drivers/net
        install -m 0644 ${S}/nf_nat_rtsp.ko ${D}${base_libdir}/modules/${KERNEL_VERSION}/kernel/drivers/net
        install -m 0644 ${S}/nf_conntrack_rtsp.ko ${D}${base_libdir}/modules/${KERNEL_VERSION}/kernel/drivers/net
}
