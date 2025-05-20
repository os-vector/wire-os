#
# Include the robot settings
#
# Anki stuff
# IMAGE_INSTALL += "animfail"
# IMAGE_INSTALL += "ankibluetoothd"
# IMAGE_INSTALL += "anki-audio-init"
# IMAGE_INSTALL += "anki-crash-log"
IMAGE_INSTALL += "anki-robot-target"
IMAGE_INSTALL += "anki-version"
# IMAGE_INSTALL += "ankiinit"
IMAGE_INSTALL += "ankiwifi"
# IMAGE_INSTALL += "${@base_contains('DISTRO_FEATURES', 'ipv6', 'ip6tables-persistent', '', d)}"
# IMAGE_INSTALL += "iptables-persistent"
# IMAGE_INSTALL += "wipe-all-wifi-configs"
#
# Victor stuff
IMAGE_INSTALL += "vic-anim"
# IMAGE_INSTALL += "vic-christen"
IMAGE_INSTALL += "vic-cloud"
IMAGE_INSTALL += "vic-crashuploader"
IMAGE_INSTALL += "vic-dasmgr"
IMAGE_INSTALL += "vic-engine"
IMAGE_INSTALL += "vic-gateway"
IMAGE_INSTALL += "vic-gateway-cert"
IMAGE_INSTALL += "vic-init"
IMAGE_INSTALL += "vic-log-uploader"
IMAGE_INSTALL += "vic-robot"
IMAGE_INSTALL += "vic-switchboard"
IMAGE_INSTALL += "vic-webserver"
IMAGE_INSTALL += "vic-bootanim"
IMAGE_INSTALL += "victor"

IMAGE_INSTALL += "wired"

IMAGE_INSTALL += "avahi-daemon"
IMAGE_INSTALL += "${@oe.utils.conditional('USER_BUILD', '1', '', 'neofetch', d)}"

#
# Other stuff
IMAGE_INSTALL += "bspatch"
IMAGE_INSTALL += "fake-hwclock"
IMAGE_INSTALL += "rebooter"
# IMAGE_INSTALL += "rng-tools"
IMAGE_INSTALL += "anki-update-engine"
# IMAGE_INSTALL += "alsa-utils"
# IMAGE_INSTALL += "lvm2"
# IMAGE_INSTALL += "cryptsetup"
# IMAGE_INSTALL += "user-data-locker"
IMAGE_INSTALL += "fault-code"

IMAGE_INSTALL += "python3-netclient"
IMAGE_INSTALL += "python3-compression"
IMAGE_INSTALL += "python3-shell"
IMAGE_INSTALL += "python3-crypt"
IMAGE_INSTALL += "python3-pkgutil"
IMAGE_INSTALL += "python3-numbers"
IMAGE_INSTALL += "python3-six"
IMAGE_INSTALL += "python3-protobuf"
IMAGE_INSTALL += "paycheck"

# IMAGE_INSTALL += "lib-robotics-camera"
# IMAGE_INSTALL += "lib-robotics-camera-dev"
# IMAGE_INSTALL += "encoders"
# IMAGE_INSTALL += "bt-property"
# IMAGE_INSTALL += "libbt-vendor"
# IMAGE_INSTALL += "audiohal"
# IMAGE_INSTALL += "fluoride"
# IMAGE_INSTALL += "rb-camera"

# Developer stuff
# IMAGE_INSTALL += "${@base_conditional('USER_BUILD', '1', '', 'reboot-stresser', d)}"
# IMAGE_INSTALL += "${@base_conditional('USER_BUILD', '1', '', 'ankitrace', d)}"
# IMAGE_INSTALL += "${@base_conditional('DEV', '1', 'ankitrace', '', d)}"
IMAGE_INSTALL += "update-os"

# IMAGE_INSTALL += "${@base_conditional('USER_BUILD', '1', '', 'parted', d)}"
