inherit autotools linux-kernel-base

DESCRIPTION = "Qualcomm Atheros WLAN"
LICENSE = "ISC"
LIC_FILES_CHKSUM = "file://${COREBASE}/meta/files/common-licenses/${LICENSE};md5=f3b90e78ea0cffb20bf5cca7947a896d"

KERNEL_VERSION = "${@get_kernelversion_headers('${STAGING_KERNEL_DIR}')}"

FILES:${PN} += "\
    ${base_libdir}/modules/${KERNEL_VERSION}/kernel/drivers/net/wireless/ar6000.ko \
    "

PR = "r2"

DEPENDS = "virtual/kernel iw"

FILESPATH =+ "${WORKSPACE}:"
#SRC_URI = "file://wlan"

S = "${UNPACKDIR}/wlan/host"

ATH_BUILD_TYPE = "mdm9x15"
ATH_BUS_TYPE = "SDIO"

EXTRA_OEMAKE = "\
    ATH_LINUXPATH=${STAGING_DIR_HOST}${base_libdir}/modules/${KERNEL_VERSION}/build \
    ATH_CROSS_COMPILE_TYPE=${STAGING_BINDIR_TOOLCHAIN}/${HOST_PREFIX} \
    ATH_BUILD_TYPE=${ATH_BUILD_TYPE} \
    ATH_BUS_TYPE=${ATH_BUS_TYPE} \
    ATH_OS_TYPE=linux_3_0 \
    ATH_ARCH_CPU_TYPE=${TARGET_ARCH} \
    ATH_BUS_SUBTYPE=linux_sdio \
    ATH_SOFTMAC_FILE_USED=no \
    ATH_HTC_RAW_INT_ENV=yes \
    ATH_INIT_MODE_DRV_ENABLED=yes \
    ATH_DEBUG_DRIVER=yes \
    ATH_BUILD_TOOLS=no \
    ATH_BUILD_3RDPARTY=no \
    ATH_BUILD_FTM=no \
    "

ATH_IMAGE_DIR = "${S}/.output/${ATH_BUILD_TYPE}-${ATH_BUS_TYPE}/image"
ATH_MODULE_DIR = "${D}${base_libdir}/modules/${KERNEL_VERSION}/kernel/drivers/net/wireless"

do_compile() {
    unset LDFLAGS
    oe_runmake
}

do_install() {
    install -d ${ATH_MODULE_DIR}
    install -m 0644 ${ATH_IMAGE_DIR}/ar6000.ko ${ATH_MODULE_DIR}
}
