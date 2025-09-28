#
# Include the robot settings
#
# Anki stuff
IMAGE_INSTALL += "animfail"
IMAGE_INSTALL += "anki-crash-log"
IMAGE_INSTALL += "anki-robot-target"
IMAGE_INSTALL += "anki-version"
IMAGE_INSTALL += "ankiwifi"
IMAGE_INSTALL += "wipe-all-wifi-configs"

# Victor stuff
IMAGE_INSTALL += "vic-anim"
IMAGE_INSTALL += "${@oe.utils.conditional('CLOUDLESS', '1', 'vic-cloudless', 'vic-cloud', d)}"
IMAGE_INSTALL += "vic-crashuploader"
IMAGE_INSTALL += "vic-dasmgr"
IMAGE_INSTALL += "vic-engine"
IMAGE_INSTALL += "vic-gateway-cert"
IMAGE_INSTALL += "vic-init"
IMAGE_INSTALL += "vic-log-uploader"
IMAGE_INSTALL += "vic-robot"
IMAGE_INSTALL += "vic-switchboard"
IMAGE_INSTALL += "vic-webserver"
IMAGE_INSTALL += "vic-bootanim"
IMAGE_INSTALL += "victor"

# Wire things
IMAGE_INSTALL += "wired"
IMAGE_INSTALL += "libpvictor"

IMAGE_INSTALL += "avahi-daemon"
IMAGE_INSTALL += "${@oe.utils.conditional('USER_BUILD', '1', '', 'neofetch', d)}"
IMAGE_INSTALL += "${@oe.utils.conditional('USER_BUILD', '1', '', 'btop', d)}"

# Other stuff
#IMAGE_INSTALL += "bspatch"
IMAGE_INSTALL += "fake-hwclock"
IMAGE_INSTALL += "rebooter"
IMAGE_INSTALL += "${@oe.utils.conditional('USER_BUILD', '1', '', 'update-os', d)}"
IMAGE_INSTALL += "anki-update-engine"
IMAGE_INSTALL += "fault-code"

#IMAGE_INSTALL += "python3-netclient"
#IMAGE_INSTALL += "python3-compression"
#IMAGE_INSTALL += "python3-shell"
#IMAGE_INSTALL += "python3-crypt"
#IMAGE_INSTALL += "python3-pkgutil"
#IMAGE_INSTALL += "python3-numbers"
#IMAGE_INSTALL += "python3-six"
IMAGE_INSTALL += "sqlite3"
IMAGE_INSTALL += "bzip2"

#IMAGE_INSTALL += "python3-protobuf"
#IMAGE_INSTALL += "paycheck"

IMAGE_INSTALL += "${@oe.utils.conditional('USER_BUILD', '1', '', 'wireutils', d)}"

