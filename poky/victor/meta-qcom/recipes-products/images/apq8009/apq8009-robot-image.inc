#
# Include the robot settings
#

# Get base product settings
require apq8009-base-image.inc

# These two lines below is a workaround to remove init-mss,
# since packagegroup-ss-mgr just include reboot-daemon and init-mss
IMAGE_INSTALL:remove = "packagegroup-ss-mgr"

# Add additional components

# backtrace unwinding is now done in-anki-binary
#IMAGE_INSTALL += "system-core-debuggerd"

IMAGE_INSTALL += "e2fsprogs"
IMAGE_INSTALL += "start-scripts-userfs-reset"
IMAGE_INSTALL += "chrony"
IMAGE_INSTALL += "chronyc"
IMAGE_INSTALL += "${@oe.utils.conditional('USER_BUILD', '1', '', 'openssh', d)}"

IMAGE_INSTALL += "${@oe.utils.conditional('DEV', '1', 'openssh', '', d)}"

# does not build with our kernel
IMAGE_INSTALL:remove = "perf"
#IMAGE_INSTALL += "connman connman-client connman-tools"
IMAGE_INSTALL += "iw"
IMAGE_INSTALL += "${@oe.utils.conditional('PRODUCT','robot-rome', 'sigma-dut', '', d)}"
IMAGE_INSTALL += "${@oe.utils.conditional('PRODUCT','robot-rome', 'libcap', '', d)}"
IMAGE_INSTALL += "${@oe.utils.conditional('PRODUCT','robot-rome', 'attr', '', d)}"
IMAGE_INSTALL += "mtd-utils-ubifs"
IMAGE_INSTALL += "blkdiscard"
IMAGE_INSTALL += "tzdata"
IMAGE_INSTALL += "tzdata-africa"
IMAGE_INSTALL += "tzdata-americas"
IMAGE_INSTALL += "tzdata-antarctica"
IMAGE_INSTALL += "tzdata-arctic"
IMAGE_INSTALL += "tzdata-asia"
IMAGE_INSTALL += "tzdata-atlantic"
IMAGE_INSTALL += "tzdata-australia"
IMAGE_INSTALL += "tzdata-europe"
IMAGE_INSTALL += "tzdata-misc"
IMAGE_INSTALL += "tzdata-pacific"
IMAGE_INSTALL += "tzdata-posix"
IMAGE_INSTALL += "tzdata-right"
IMAGE_INSTALL += "${@oe.utils.conditional('USER_BUILD', '1', '', 'jq', d)}"
IMAGE_INSTALL += "ca-certificates"
IMAGE_INSTALL += "${@oe.utils.conditional('USER_BUILD', '1', '', 'attr', d)}"
IMAGE_INSTALL += "${@oe.utils.conditional('USER_BUILD', '1', '', 'libcap-bin', d)}"
IMAGE_INSTALL += "sudo"
