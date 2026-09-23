DESCRIPTION = "Small initramfs image based on poky/meta/core-recipes/core-image-minimal-initramfs - directly boots into dm-verity rootfs on emmc."

#PACKAGE_INSTALL = "busybox udev base-passwd ${ROOTFS_BOOTSTRAP_INSTALL} lvm2 initscript-anki rampost"
PACKAGE_INSTALL = "busybox udev base-passwd lvm2 initscript-anki rampost"

# Do not pollute the initrd image with rootfs features
IMAGE_FEATURES = ""

export IMAGE_BASENAME = "core-image-anki-initramfs"
#IMAGE_ROOTFS_DIR = "${WORKDIR}/${IMAGE_BASENAME}-${PV}-${PR}_${PRODUCT}/rootfs"
IMAGE_LINGUAS = ""

LICENSE = "MIT"

IMAGE_FSTYPES = "cpio.gz"
inherit core-image qperf

IMAGE_ROOTFS_SIZE = "8192"

BAD_RECOMMENDATIONS += "busybox-syslog"
