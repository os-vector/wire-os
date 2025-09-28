inherit module qperf

DESCRIPTION = "Neutrino Ethernet driver"
LICENSE = "MIT-style"
LIC_FILES_CHKSUM = "file://DWC_ETH_QOS_dev.c;\
startline=1;endline=71;md5=647bc30ad1427f25bad8967a98d3aecb"

FILES:${PN}     += "${base_libdir}/modules/${KERNEL_VERSION}/"
FILES:${PN}     += "${sysconfdir}/init.d/neutrino_pcie_start_stop_le"
FILES:${PN}     += "${sysconfdir}/init.d/setup_avtp_routing_le"
RPROVIDES_${PN} += "kernel-module-dwc-eth-qos"

do_unpack[deptask] = "do_populate_sysroot"
PR = "r0"

# This DEPENDS is to serialize kernel module builds
DEPENDS = "rtsp-alg"

FILESPATH =+ "${WORKSPACE}:"
SRC_URI = "file://qcom-opensource/ethernet/neutrino-pcie/driver"
SRC_URI += "file://neutrino_pcie_start_stop_le"
SRC_URI += "file://setup_avtp_routing_le"

S = "${UNPACKDIR}/qcom-opensource/ethernet/neutrino-pcie/driver"

EXTRA_OEMAKE += "LBITS=32"
EXTRA_OEMAKE += "DWC_ETH_QOS_DISABLE_PLT_INIT=1"
EXTRA_OEMAKE += "DWC_ETH_QOS_ENABLE_ETHTOOL=1"
EXTRA_OEMAKE += "${@bb.utils.contains('MACHINE_FEATURES', 'ipa-offload', 'CONFIG_IPA_OFFLOAD=1', '', d)}"

do_install() {
    module_do_install
    install -d ${D}${sysconfdir}/init.d
    install -m 0755 ${WORKDIR}/neutrino_pcie_start_stop_le ${D}${sysconfdir}/init.d
    install -m 0755 ${WORKDIR}/setup_avtp_routing_le ${D}${sysconfdir}/init.d
}

pkg_postinst_${PN} () {
    [ -n "$D" ] && OPT="-r $D" || OPT="-s"
    update-rc.d $OPT -f neutrino_pcie_start_stop_le remove
    update-rc.d $OPT neutrino_pcie_start_stop_le start 37 S . stop 63 0 1 6 .
    update-rc.d $OPT -f setup_avtp_routing_le remove
    update-rc.d $OPT setup_avtp_routing_le start 91 5 . stop 9 0 1 6 .
}

do_module_signing() {
    if [ -f ${STAGING_KERNEL_BUILDDIR}/signing_key.priv ]; then
        bbnote "Signing ${PN} module"
        ${STAGING_KERNEL_DIR}/scripts/sign-file sha512 ${STAGING_KERNEL_BUILDDIR}/signing_key.priv \
		${STAGING_KERNEL_BUILDDIR}/signing_key.x509 \
		${PKGDEST}/${PROVIDES_NAME}/lib/modules/$${KERNEL_VERSION}/extra/DWC_ETH_QOS.ko
    else
        bbnote "${PN} module is not being signed"
    fi
}

addtask module_signing after do_package before do_package_write_ipk
