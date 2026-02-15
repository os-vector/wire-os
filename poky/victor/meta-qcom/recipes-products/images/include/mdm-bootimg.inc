# Force a few additional dependencies in the mix so that we get the needed
# recipes to build in the right order so we can make the bootimg file and
# our images...
require ${COREBASE}/../victor/meta-qcom/recipes-products/images/include/mdm-ramdisk.inc
DEPENDS += " \
             virtual/kernel \
             pkgconfig-native \
             gtk-doc-native \
             gettext-native \
             e2fsprogs-native \
             mtd-utils-native"

RDEPENDS += " \
             pkgconfig-native \
             gtk-doc-native \
             gettext-native \
             e2fsprogs-native \
             mtd-utils-native"

IMAGE_LINGUAS = ""

#Exclude packages
PACKAGE_EXCLUDE += "readline"

# Default image output types
IMAGE_FSTYPES ?= "tar.gz ${INITRAMFS_FSTYPES}"

# Use busybox as login manager
IMAGE_LOGIN_MANAGER = "busybox-static"

# Include minimum init and init scripts
IMAGE_DEV_MANAGER ?= "busybox-static-mdev"
IMAGE_INIT_MANAGER ?= "sysvinit sysvinit-pidof"
IMAGE_INITSCRIPTS ?= ""

ROOTFS_POSTPROCESS_COMMAND:append = " gen_buildprop;"
ROOTFS_POSTPROCESS_COMMAND:append = "${@bb.utils.contains('DISTRO_FEATURES','flashless','create_squashfs_rootimg;','', d)}"
EXTRA_IMAGE_FEATURES += "${@bb.utils.contains('DISTRO_FEATURES','ro-rootfs','read-only-rootfs','',d)}"
gen_buildprop() {
    # i'm very sorry for doing this here
   install -d -m 0700 -o root -g root ${IMAGE_ROOTFS}/root
   rm -rf ${IMAGE_ROOTFS}/home/root
   rm -rf ${IMAGE_ROOTFS}/media ${IMAGE_ROOTFS}/srv
#   echo ro.build.version.release=`cat ${IMAGE_ROOTFS}/etc/version ` >> ${IMAGE_ROOTFS}/build.prop
#   echo ro.product.name=${BASEMACHINE}-${DISTRO} >> ${IMAGE_ROOTFS}/build.prop
   echo ${MACHINE} > ${IMAGE_ROOTFS}/target
}

# Generate ramdisk for sdxpoorwills
do_rootfs:append_sdxpoorwills() {
    bb.build.exec_func('do_ramdisk_create',d)
}
# Generate ramdisk for sdxprairie
do_rootfs:append_sdxprairie() {
    bb.build.exec_func('do_ramdisk_create',d)
}
