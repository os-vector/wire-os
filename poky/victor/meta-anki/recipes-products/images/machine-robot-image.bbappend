# Camera Open Source packages
include ${BASEMACHINE}/${BASEMACHINE}-anki-robot-image.inc
require include/mdm-bootimg.inc

ROOTFS_POSTPROCESS_COMMAND += ' read_only_robot_rootfs_hook;'
ROOTFS_POSTPROCESS_COMMAND += ' remove_unused_hwdata;'

remove_unused_hwdata () {
	rm -rf ${IMAGE_ROOTFS}${nonarch_libdir}/udev/hwdb.d
	rm -f  ${IMAGE_ROOTFS}${nonarch_libdir}/udev/hwdb.bin
	rm -f  ${IMAGE_ROOTFS}${sysconfdir}/udev/hwdb.bin
	rm -rf ${IMAGE_ROOTFS}${datadir}/keymaps
	rm -rf ${IMAGE_ROOTFS}${datadir}/consolefonts
	rm -rf ${IMAGE_ROOTFS}${datadir}/mime
}

# A hook function to support read-only-rootfs IMAGE_FEATURES
read_only_robot_rootfs_hook () {
	if [ -e ${IMAGE_ROOTFS}/etc/default/ssh ]; then
		echo 'SYSCONFDIR=/data/ssh' > ${IMAGE_ROOTFS}/etc/default/ssh
	fi
	echo "ro.anki.product.name=${ANKI_PRODUCT_NAME}" >> ${IMAGE_ROOTFS}/build.prop
}
