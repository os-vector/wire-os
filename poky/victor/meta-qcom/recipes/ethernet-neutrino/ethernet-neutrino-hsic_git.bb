inherit module autotools-brokensep qperf

DESCRIPTION = "Neutrino HSIC driver"
LICENSE = "MIT-style"
LIC_FILES_CHKSUM = "file://DWC_ETH_QOS_yapphdr.h;\
startline=1;endline=70;md5=ed991597877111486701bb66868b0676"

FILES:${PN}     += "${base_libdir}/modules/${KERNEL_VERSION}/"
FILES:${PN}     += "${sysconfdir}/init.d/neutrino_hsic_start_stop_le"
FILES:${PN}     += "${sysconfdir}/init.d/setup_avtp_routing_le"
RPROVIDES_${PN} += "kernel-module-ntn-hsic"

do_unpack[deptask] = "do_populate_sysroot"
PR = "r0-${KERNEL_VERSION}"

# This DEPENDS is to serialize kernel module builds
DEPENDS = "rtsp-alg"

FILESPATH =+ "${WORKSPACE}:"
SRC_URI = "file://qcom-opensource/ethernet/neutrino-hsic/driver/"
SRC_URI += "file://neutrino_hsic_start_stop_le"
SRC_URI += "file://setup_avtp_routing_le"

S = "${UNPACKDIR}/qcom-opensource/ethernet/neutrino-hsic/driver"

EXTRA_OEMAKE =+ "LBITS=32"
#EXTRA_OEMAKE =+ "DWC_ETH_QOS_DISABLE_PLT_INIT=1"
#EXTRA_OEMAKE =+ "DWC_ETH_QOS_ENABLE_ETHTOOL=1"
#EXTRA_OEMAKE =+ "CONFIG_IPA_OFFLOAD=1"
EXTRA_OEMAKE =+ "KERNEL_SRC=${STAGING_KERNEL_DIR}"

do_install() {
    module_do_install
    install -d ${D}${sysconfdir}/init.d
    install -m 0755 ${WORKDIR}/neutrino_hsic_start_stop_le ${D}${sysconfdir}/init.d
    install -m 0755 ${WORKDIR}/setup_avtp_routing_le ${D}${sysconfdir}/init.d
}

pkg_postinst_${PN} () {
    [ -n "$D" ] && OPT="-r $D" || OPT="-s"
    update-rc.d $OPT -f neutrino_hsic_start_stop_le remove
    update-rc.d $OPT neutrino_hsic_start_stop_le start 37 S . stop 63 0 1 6 .
    update-rc.d $OPT -f setup_avtp_routing_le remove
    update-rc.d $OPT setup_avtp_routing_le start 91 5 . stop 9 0 1 6 .
}

do_module_signing() {
    if [ -f ${STAGING_KERNEL_BUILDDIR}/signing_key.priv ]; then
        bbnote "Signing ${PN} module"
        ${STAGING_KERNEL_DIR}/scripts/sign-file sha512 ${STAGING_KERNEL_BUILDDIR}/signing_key.priv \
		${STAGING_KERNEL_BUILDDIR}/signing_key.x509 \
		${PKGDEST}/${PROVIDES_NAME}/lib/modules/$${KERNEL_VERSION}/extra/NTN_HSIC.ko
    else
        bbnote "${PN} module is not being signed"
    fi
}

addtask module_signing after do_package before do_package_write_ipk
