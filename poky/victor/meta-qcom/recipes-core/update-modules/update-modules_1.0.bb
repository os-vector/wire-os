SECTION = "base"
DESCRIPTION = "Script to manage module configuration files"
LICENSE = "GPLv2"
LIC_FILES_CHKSUM = "file://update-modules;startline=3;endline=5;md5=a907c58943cce6a1032aaec56181d6d6"
PACKAGE_ARCH = "all"
INHIBIT_DEFAULT_DEPS = "1"
RDEPENDS:${PN} = "${@bb.utils.contains("MACHINE_FEATURES", "kernel26",  "module-init-tools-depmod","modutils-depmod",d)} "
PR = "r12"

SRC_URI = "file://update-modules"

pkg_postinst_${PN}() {
if [ "x$D" != "x" ]; then
	exit 1
fi
update-modules
}

do_install() {
	install -d ${D}${sbindir}
	install ${S}/update-modules ${D}${sbindir}
}
