# WLAN open source Packages
#IMAGE_INSTALL += "qcacld-utils"
#IMAGE_INSTALL += "hostap-daemon-qcacld"
#IMAGE_INSTALL += "wpa-supplicant-qcacld"

#IMAGE_INSTALL += "wcnss"
#IMAGE_INSTALL += "wlan-opensource"

#IMAGE_INSTALL:append_qsap = "qcacld-hl"
#IMAGE_INSTALL:append_qsap = "wlan-services"

#IMAGE_INSTALL:append_drone = "qcacld-hl"
#IMAGE_INSTALL:append_drone = "wlan-services"

#IMAGE_INSTALL:append_robot-som = "qcacld-hl"
#IMAGE_INSTALL:append_robot-som = "wlan-services"

#IMAGE_INSTALL:remove_robot-rome = "qcacld-hl"
#IMAGE_INSTALL:append_robot-rome = "qcacld-hl-rome"

# robot-som is base distro of robot, need to remove what base had first
IMAGE_INSTALL:remove_robot-pronto = "qcacld-hl"
IMAGE_INSTALL:remove_robot-pronto = "wlan-services"
IMAGE_INSTALL:append_robot-pronto = "wcnss"
IMAGE_INSTALL:append_robot-pronto = "wlan-opensource"
