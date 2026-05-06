SUMMARY = "mainline linux kernel for apq8009 (anki vector)"
#LICENSE = "GPL-2.0-only"
#LIC_FILES_CHKSUM = "file://COPYING;md5=d7810fab7487fb0aad327b76f1be7cd7"

LICENSE = "GPL-2.0-only"
LIC_FILES_CHKSUM = "file://COPYING;md5=6bc538ed5bd9a7fc9398086aedcd7e46"

KERNEL_DEFCONFIG = "apq8009_defconfig"
KERNEL_DEVICETREE = "qcom/qcom-apq8009-anki-vector.dtb"
COMPATIBLE_MACHINE = "apq8009"

inherit kernel externalsrc
#require recipes-kernel/linux/linux-yocto.inc

do_configure () {
    oe_runmake_call CC="${KERNEL_CC}" LD="${KERNEL_LD}" -C ${S} ARCH=${ARCH} ${KERNEL_EXTRA_ARGS} ${KERNEL_DEFCONFIG}
}

PV = "7.1.0"

EXTERNALSRC = "${WORKSPACE}/kernel/linux-vector"
EXTERNALSRC_BUILD = "${WORKSPACE}/kernel/linux-vector"

S = "${EXTERNALSRC}"

# gc1066 will be loaded by userland
KERNEL_MODULE_AUTOLOAD:append = " \
    af_alg \
    algif_hash \
    algif_skcipher \
    ip_tables \
    x_tables \
    iptable_filter \
    iptable_mangle \
    iptable_nat \
    nf_nat \
    nf_conntrack \
    nf_defrag_ipv4 \
    nf_defrag_ipv6 \
    xt_tcpudp \
    xt_conntrack \
    overlay \
    fuse \
    qcom_camss \
    videodev \
    mc \
    vector-keymaster \
    videobuf2_common \
    videobuf2_memops \
    videobuf2_dma_contig \
    videobuf2_v4l2 \
    v4l2_fwnode \
    v4l2_async \
    v4l2_cci \
"

