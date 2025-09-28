inherit module

DESCRIPTION = "Qualcomm Atheros WLAN Host Driver Module"
SECTION = "kernel/modules"
LIC_FILES_CHKSUM = "file://${COREBASE}/meta/files/common-licenses/BSD-3-Clause;md5=550794465ba0ec5312d6919e203a55f9"
LICENSE = "BSD-3-Clause"

FILES:${PN}     += "${nonarch_base_libdir}/modules/${KERNEL_VERSION}/extra/wlan.ko"
do_unpack[deptask] = "do_populate_sysroot"
PR = "r2"

DEPENDS = "virtual/kernel iw"

FILESPATH =+ "${WORKSPACE}:"
SRC_URI = "file://qcom-opensource/wlan/prima"

S = "${UNPACKDIR}/qcom-opensource/wlan/prima"
##UNPACKDIR = "${S}"

EXTRA_OEMAKE += "CONFIG_PRONTO_WLAN=m \
                 KERNEL_BUILD=0"
#EXTRA_OEMAKE += 'CONFIG_PRONTO_WLAN=m WLAN_DIR="${S}"  KERNEL_BUILD=1'

PACKAGES += "kernel-module-wlan"

do_compile () {
    unset CFLAGS CPPFLAGS CXXFLAGS LDFLAGS CC CPP LD
    oe_runmake 'MODPATH="${base_libdir}/modules/wlan/prima"' \
        'KERNEL_SOURCE="${STAGING_KERNEL_DIR}"' \
        'KDIR="${STAGING_KERNEL_DIR}"' \
        'CC="${KERNEL_CC}"' \
        'WLAN_DIR="${S}"'
}

do_install () {
	module_do_install
	install -m 0644 CORE/SVC/external/wlan_nlink_common.h -D ${D}${includedir}/prima/wlan_nlink_common.h
}

# Remove dependency for wrong kernel version
python split_kernel_module_packages:append() {
        if modules:
                metapkg = d.getVar('KERNEL_MODULES_META_PACKAGE', True)
                d.delVar('RDEPENDS:' + metapkg)
                d.delVar('RDEPENDS:kernel-module-wlan')
}

do_module_signing() {
    if [ -f ${STAGING_KERNEL_BUILDDIR}/signing_key.priv ]; then
	${STAGING_KERNEL_DIR}/scripts/sign-file sha512 ${STAGING_KERNEL_BUILDDIR}/signing_key.priv ${STAGING_KERNEL_BUILDDIR}/signing_key.x509 ${PKGDEST}/${PN}/${nonarch_base_libdir}/modules/${KERNEL_VERSION}/extra/wlan.ko
    elif [ -f ${STAGING_KERNEL_BUILDDIR}/certs/signing_key.pem ]; then
        ${STAGING_KERNEL_BUILDDIR}/scripts/sign-file sha512 ${STAGING_KERNEL_BUILDDIR}/certs/signing_key.pem ${STAGING_KERNEL_BUILDDIR}/certs/signing_key.x509 ${PKGDEST}/${PN}/lib/modules/${KERNEL_VERSION}/extra/wlan.ko
    fi
}

addtask module_signing after do_package before do_package_write_ipk
