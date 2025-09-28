#
# Include the robot settings
#

# userdata security/stuff
IMAGE_INSTALL += "cryptsetup"
IMAGE_INSTALL += "user-data-locker"
IMAGE_INSTALL += "securemsm-noship"
IMAGE_INSTALL += "vic-christen"

IMAGE_INSTALL += "openssl"
IMAGE_INSTALL += "libatomic"
IMAGE_INSTALL += "libdevmapper"
IMAGE_INSTALL += "libsamplerate0"
IMAGE_INSTALL += "libsndfile1"
IMAGE_INSTALL += "lvm2"
IMAGE_INSTALL += "chrony"

IMAGE_INSTALL += "iptables-persistent"
IMAGE_INSTALL += "ip6tables-persistent"

# this used 100 percent of the CPU at boot, not sure why
# things seem to work fine without it
#IMAGE_INSTALL += "rng-tools"

IMAGE_INSTALL += "alsa-utils"
#IMAGE_INSTALL += "dhcpcd"
IMAGE_INSTALL += "openssl-bin"
IMAGE_INSTALL += "emr-cat"
IMAGE_INSTALL += "wpa-supplicant"
IMAGE_INSTALL += "extra-conf"
IMAGE_INSTALL += "update-engine"
IMAGE_INSTALL += "bash"
#IMAGE_INSTALL += "python3"
IMAGE_INSTALL += "libgomp"

IMAGE_INSTALL += "libunwind"
IMAGE_INSTALL += "liburcu"

# uncomment if you want systemd-analyze and lttng
#IMAGE_INSTALL += "ankiprofile"

# dev-only things
IMAGE_INSTALL += "${@oe.utils.conditional('USER_BUILD', '1', '', 'nano', d)}"
IMAGE_INSTALL += "${@oe.utils.conditional('USER_BUILD', '1', '', 'rsync', d)}"
IMAGE_INSTALL += "${@oe.utils.conditional('USER_BUILD', '1', '', 'htop', d)}"

# all the HAL stuff
IMAGE_INSTALL += "audiohal"
IMAGE_INSTALL += "bt-property"
IMAGE_INSTALL += "libbt-vendor"
IMAGE_INSTALL += "fluoride"
IMAGE_INSTALL += "rmtstorage"
IMAGE_INSTALL += "adsprpc"
IMAGE_INSTALL += "init-audio"
IMAGE_INSTALL += "mm-camera"
IMAGE_INSTALL += "ankibluetoothd"

# for OpenCL and EGL if you want that
#IMAGE_INSTALL += "adreno"

IMAGE_INSTALL += "busybox"

# busybox's ps, netstat, and dmesg are not good enough for me
IMAGE_INSTALL += "${@oe.utils.conditional('USER_BUILD', '1', '', 'procps', d)}"
IMAGE_INSTALL += "${@oe.utils.conditional('USER_BUILD', '1', '', 'net-tools', d)}"
IMAGE_INSTALL += "${@oe.utils.conditional('USER_BUILD', '1', '', 'util-linux-dmesg', d)}"

# want some debug tools?
#IMAGE_INSTALL += "gdb"
#IMAGE_INSTALL += "strace"

IMAGE_INSTALL += "syslog-ng"
IMAGE_INSTALL += "logrotate"
IMAGE_INSTALL += "connman"
IMAGE_INSTALL += "connman-client"
#IMAGE_INSTALL += "kernel-module-ramoops"
