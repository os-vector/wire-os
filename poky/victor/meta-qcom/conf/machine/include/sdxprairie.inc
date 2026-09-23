BASEMACHINE ?= "sdxprairie"
require conf/machine/include/basemachine.inc

SOC_FAMILY ?= "mdm9x50"
include conf/machine/include/soc-family.inc

# Pull in security flags
require ${COREBASE}/meta-qti-bsp/conf/distro/include/security_flags.inc

include conf/machine/include/tune-cortexa8.inc

PREFERRED_PROVIDER_virtual/kernel = "linux-msm"
PREFERRED_VERSION_linux-msm = "4.14"

MACHINE_DISPLAY_WIDTH_PIXELS ?= "480"
MACHINE_DISPLAY_HEIGHT_PIXELS ?= "640"

SERIAL_CONSOLES ?= '115200;ttyMSM0'

DISTRO ?= "mdm"

DISTRO_FEATURES:append = " persist-volume"

MACHINE_FEATURES += "kernel26 keyboard ipa-offload"

PAGE_SIZE ?= '4096'

# Max supported inodes
EXTRA_IMAGECMD ?= "-N 2048"

#UBINIZE Args
UBINIZE_ARGS ?= "-m 4096 -p 256KiB -s 4096"
#MKUBIFS Args
MKUBIFS_ARGS ?= "-m 4096 -e 253952 -c 2146 -F"
#Volume sizes
SYSTEM_VOLUME_SIZE ?= "110MiB"
CACHE_VOLUME_SIZE ?= "15MiB"
SYSTEMRW_VOLUME_SIZE ?= "6MiB"
PERSIST_VOLUME_SIZE ?= "6MiB"

# Sizes for EXT4 (in bytes)
SYSTEM_SIZE_EXT4 ?= "100000000"
SYSTEMRW_SIZE_EXT4 ?= "8388608"
USERDATA_SIZE_EXT4 ?= "2000000000"
CACHE_SIZE_EXT4 ?= "80000000"
RECOVERYFS_SIZE_EXT4 ?= "100000000"

# Enable LE Verified Boot
DISTRO_FEATURES:append = " vble"

#Disable rpmb listener
DISTRO_FEATURES:append = " no-rpmb-listener-support"

#Disable ssd listener
DISTRO_FEATURES:append = " no-ssd-listener-support"

# Formats of root filesystem images.
IMAGE_FSTYPES += "ubi"

# Userspace needs below given macro to support libion after kernel 4.12.
TARGET_CFLAGS:append = " -DTARGET_ION_ABI_VERSION=2 "
TARGET_CXXFLAGS:append = " -DTARGET_ION_ABI_VERSION=2 "

# readonly-rootfs
DISTRO_FEATURES:append = " ro-rootfs"

# Use systemd init manager.
DISTRO_FEATURES:remove = "sysvinit"
DISTRO_FEATURES:append = " systemd"
DISTRO_FEATURES_OPTED_OUT += "sysvinit"
VIRTUAL-RUNTIME_dev_manager = "udev"
VIRTUAL-RUNTIME_init_manager = "systemd"

# Change Image features for systemd.
IMAGE_DEV_MANAGER = "udev"
IMAGE_INIT_MANAGER = "systemd"
IMAGE_INITSCRIPTS = ""

# Make sure either INITRAMFS_IMAGE_BUNDLE or  EXT_INITRAMFS_IMAGE_BUNDLE
# is enabled and not both are enabled at the same time

#INITRAMFS_IMAGE_BUNDLE = "${@bb.utils.contains('DISTRO_FEATURES', 'initramfs', '1', '', d)}"
EXT_INITRAMFS_IMAGE_BUNDLE = "${@bb.utils.contains('DISTRO_FEATURES', 'initramfs', '1', '', d)}"
INITRAMFS_IMAGE = "${@bb.utils.contains('DISTRO_FEATURES', 'initramfs', 'machine-initramfs', '', d)}"
INITRAMFS_FSTYPES += "${@bb.utils.contains('DISTRO_FEATURES', 'initramfs', 'cpio.gz tar.bz2', '', d)}"
