BASEMACHINE ?= "qcs40x"
require conf/machine/include/basemachine.inc

SOC_FAMILY ?= "qcs405"
include conf/machine/include/soc-family.inc

# Pull in security flags
require ${COREBASE}/meta-qti-bsp/conf/distro/include/security_flags.inc

include conf/machine/include/arm/arch-armv8.inc

#usb composition to be used by qcs40x devices.
USBCOMPOSITION      = "901D"

PREFERRED_PROVIDER_virtual/kernel = "linux-msm"
PREFERRED_VERSION_linux-msm = "4.14"

# Set openssl10 as openssl provider.
PREFERRED_PROVIDER_openssl          = "openssl10"
PREFERRED_VERSION_openssl10         = "1.0.2q"
PREFERRED_PROVIDER_openssl-native   = "openssl10-native"

MACHINE_DISPLAY_WIDTH_PIXELS ?= "480"
MACHINE_DISPLAY_HEIGHT_PIXELS ?= "640"

SERIAL_CONSOLE ?= "${@['', '115200 ttyMSM0'][d.getVar('VARIANT', True) == ('' or 'debug')]}"
LLVM_VERSION = "6.0"
DISTRO ?= "msm"

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

#Enable SDK feature
# SDK_IMAGE ?= "1"

PAGE_SIZE ?= '4096'

MACHINE_FEATURES += "kernel26 keyboard"

# fbdev driver
MACHINE_FEATURES += "fbdev"

#Enable persist image/volume.
DISTRO_FEATURES += "persist-image"
# Enable LE Verified Boot
DISTRO_FEATURES:append = " vble"

KERNEL_DTS_NAME = "qcs405-rumi"
KERNEL_BASE = "0x80000000"
KERNEL_TAGS_OFFSET = "0x1e00000"

KERNEL_CMD_PARAMS ?= "noinitrd ro rootwait console=ttyMSM0,115200,n8 no_console_suspend=1 androidboot.hardware=qcom androidboot.console=ttyMSM0 lpm_levels.sleep_disabled=1 service_locator.enable=1 firmware_class.path=/lib/firmware/updates msm_rtb.filter=0x237"

# Userspace needs below given macro to support libion after kernel 4.12.
TARGET_CFLAGS:append = " -DTARGET_ION_ABI_VERSION=2 "
TARGET_CXXFLAGS:append = " -DTARGET_ION_ABI_VERSION=2 "
DISTRO_FEATURES:append = " audio-only-libav "
