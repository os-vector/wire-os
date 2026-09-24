# Camera Open Source packages
include ${BASEMACHINE}/${BASEMACHINE}-anki-robot-image.inc
require include/mdm-bootimg.inc

ROOTFS_POSTPROCESS_COMMAND += ' read_only_robot_rootfs_hook;'
ROOTFS_POSTPROCESS_COMMAND += ' remove_unused_hwdata;'
ROOTFS_POSTPROCESS_COMMAND += ' remove_unused_qcom;'

remove_unused_hwdata () {
	rm -rf ${IMAGE_ROOTFS}${nonarch_libdir}/udev/hwdb.d
	rm -f  ${IMAGE_ROOTFS}${nonarch_libdir}/udev/hwdb.bin
	rm -f  ${IMAGE_ROOTFS}${sysconfdir}/udev/hwdb.bin
	rm -rf ${IMAGE_ROOTFS}${datadir}/keymaps
	rm -rf ${IMAGE_ROOTFS}${datadir}/consolefonts
	rm -rf ${IMAGE_ROOTFS}${datadir}/mime
}

remove_unused_qcom () {
	rm -f ${IMAGE_ROOTFS}${bindir}/qmi_test_*
	rm -f ${IMAGE_ROOTFS}${bindir}/btnvtool
	rm -f ${IMAGE_ROOTFS}${libdir}/libqmi.so*
	rm -f ${IMAGE_ROOTFS}${libdir}/libqmi_client_qmux.so*
	rm -f ${IMAGE_ROOTFS}${libdir}/libqmiidl.so*
	rm -f ${IMAGE_ROOTFS}${libdir}/libqmi_cci.so*
	rm -f ${IMAGE_ROOTFS}${libdir}/libqmi_sap.so*
	rm -f ${IMAGE_ROOTFS}${libdir}/libqmi_common_so.so*
	rm -f ${IMAGE_ROOTFS}${libdir}/libconfigdb.so*
	rm -f ${IMAGE_ROOTFS}${libdir}/libdsutils.so*
	rm -f ${IMAGE_ROOTFS}${libdir}/libxml.so*
	rm -f ${IMAGE_ROOTFS}${bindir}/adsprpcd
	rm -f ${IMAGE_ROOTFS}${libdir}/libadsp_default_listener.so
	rm -f ${IMAGE_ROOTFS}${libdir}/audio.a2dp.default.so
	rm -f ${IMAGE_ROOTFS}${libdir}/libaudioa2dpdefault.so*
	rm -f ${IMAGE_ROOTFS}${bindir}/InstallKeybox
	rm -f ${IMAGE_ROOTFS}${bindir}/hdcp1prov
	rm -f ${IMAGE_ROOTFS}${bindir}/hdcp2p2prov
	rm -f ${IMAGE_ROOTFS}${libdir}/libhdcp1prov.so*
	rm -f ${IMAGE_ROOTFS}${libdir}/libhdcp2p2prov.so*
	rm -f ${IMAGE_ROOTFS}${libdir}/libKeyMaster.so*
	rm -f ${IMAGE_ROOTFS}${libdir}/libdrmfs.so*
	rm -f ${IMAGE_ROOTFS}${libdir}/libdrmtime.so*
	rm -f ${IMAGE_ROOTFS}${libdir}/librpmb.so*
	rm -f ${IMAGE_ROOTFS}${libdir}/libssd.so*
}

# A hook function to support read-only-rootfs IMAGE_FEATURES
read_only_robot_rootfs_hook () {
	if [ -e ${IMAGE_ROOTFS}/etc/default/ssh ]; then
		echo 'SYSCONFDIR=/data/ssh' > ${IMAGE_ROOTFS}/etc/default/ssh
	fi
	echo "ro.anki.product.name=${ANKI_PRODUCT_NAME}" >> ${IMAGE_ROOTFS}/build.prop
}
