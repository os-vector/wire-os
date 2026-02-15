SUMMARY = "mainline linux kernel for apq8009 (anki vector)"
#LICENSE = "GPL-2.0-only"
#LIC_FILES_CHKSUM = "file://COPYING;md5=d7810fab7487fb0aad327b76f1be7cd7"

LICENSE = "GPL-2.0-only WITH Linux-syscall-note"

LIC_FILES_CHKSUM = "file://COPYING;md5=6bc538ed5bd9a7fc9398086aedcd7e46"

inherit kernel externalsrc
require recipes-kernel/linux/linux-yocto.inc

PV = "6.19.0"

EXTERNALSRC = "${WORKSPACE}/kernel/linux-vector-mainline"
EXTERNALSRC_BUILD = "${WORKSPACE}/kernel/linux-vector-mainline"

S = "${EXTERNALSRC}"

SRC_URI += "file://defconfig"

KERNEL_DEFCONFIG = "apq8009_defconfig"

KERNEL_DEVICETREE = "qcom/qcom-apq8009-anki-vector.dtb"

COMPATIBLE_MACHINE = "apq8009"
