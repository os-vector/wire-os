#
# Include the robot settings
#

DEPENDS += "lk2nd"

# userdata security/stuff
IMAGE_INSTALL += "cryptsetup"
IMAGE_INSTALL += "user-data-locker"
#IMAGE_INSTALL += "securemsm-noship"
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
IMAGE_INSTALL += "${@oe.utils.conditional('USER_BUILD', '1', '', 'htop', d)}"
IMAGE_INSTALL += "${@oe.utils.conditional('USER_BUILD', '1', '', 'nano', d)}"
IMAGE_INSTALL += "${@oe.utils.conditional('USER_BUILD', '1', '', 'rsync', d)}"
#IMAGE_INSTALL += "${@oe.utils.conditional('USER_BUILD', '1', '', 'vim-tiny', d)}"

IMAGE_INSTALL += "wirebluetoothd"

IMAGE_INSTALL += "libpcre"

# for OpenCL and EGL if you want that
#IMAGE_INSTALL += "adreno"

IMAGE_INSTALL += "busybox"

# busybox's ps, netstat, and dmesg are not good enough for me
IMAGE_INSTALL += "${@oe.utils.conditional('USER_BUILD', '1', '', 'procps', d)}"
IMAGE_INSTALL += "${@oe.utils.conditional('USER_BUILD', '1', '', 'net-tools', d)}"
IMAGE_INSTALL += "${@oe.utils.conditional('USER_BUILD', '1', '', 'util-linux-dmesg', d)}"

# skittle
IMAGE_INSTALL += "purplpkg"

# want some debug tools?
#IMAGE_INSTALL += "gdb"
IMAGE_INSTALL += "strace"

IMAGE_INSTALL += "syslog-ng"
IMAGE_INSTALL += "logrotate"
IMAGE_INSTALL += "connman"
IMAGE_INSTALL += "connman-client"
#IMAGE_INSTALL += "kernel-module-ramoops"
IMAGE_INSTALL += "util-linux-taskset"
IMAGE_INSTALL += "util-linux-lscpu"

IMAGE_INSTALL += "leproperties"

IMAGE_INSTALL += "i2c-tools"
IMAGE_INSTALL += "v4l-utils"
IMAGE_INSTALL += "bluez-tools"
IMAGE_INSTALL += "bluealsa"

IMAGE_INSTALL:append = " \
    kernel-module-algif-hash \
    kernel-module-algif-skcipher \
    kernel-module-af-alg \
    kernel-module-iptable-nat \
    kernel-module-nf-nat \
    kernel-module-iptable-mangle \
    kernel-module-gc1066 \
    kernel-module-bf2253l \
    kernel-module-v4l2-cci \
    kernel-module-qcom-camss \
    kernel-module-videobuf2-dma-contig \
    kernel-module-videobuf2-memops \
    kernel-module-videobuf2-v4l2 \
    kernel-module-videobuf2-common \
    kernel-module-v4l2-fwnode \
    kernel-module-v4l2-async \
    kernel-module-videodev \
    kernel-module-mc \
    kernel-module-overlay \
    kernel-module-xt-tcpudp \
    kernel-module-xt-conntrack \
    kernel-module-nf-conntrack \
    kernel-module-nf-defrag-ipv6 \
    kernel-module-nf-defrag-ipv4 \
    kernel-module-iptable-filter \
    kernel-module-ip-tables \
    kernel-module-x-tables \
    kernel-module-fuse \
    kernel-module-vector-keymaster \
"

