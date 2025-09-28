DESCRIPTION = "Start up script for detecting partitions"
HOMEPAGE = "http://codeaurora.org"
LIC_FILES_CHKSUM = "file://${COREBASE}/meta/files/common-licenses/BSD-3-Clause;md5=550794465ba0ec5312d6919e203a55f9"
LICENSE = "BSD-3-Clause"

SRC_URI += "file://${BASEMACHINE}/find_partitions.sh"
SRC_URI += "file://find_partitions.service"

S = "${UNPACKDIR}/${BASEMACHINE}"
#UNPACKDIR = "${S}"

PR = "r4"

inherit systemd
inherit update-rc.d

INITSCRIPT_NAME = "find_partitions.sh"
INITSCRIPT_PARAMS = "start 36 S ."
INITSCRIPT_PARAMS_mdm = "start 30 S ."

FILES:${PN} += "${systemd_unitdir}/system/"

do_install() {
     if ${@bb.utils.contains('DISTRO_FEATURES', 'systemd', 'true', 'false', d)}; then
       install -m 0755 ${S}/find_partitions.sh -D ${D}${sysconfdir}/initscripts/find_partitions.sh
       install -d  ${D}${systemd_unitdir}/system/
       install -m 0644 ${S}/../find_partitions.service -D ${D}${systemd_unitdir}/system/find_partitions.service
       install -d ${D}${systemd_unitdir}/system/sysinit.target.wants/
       # enable the service for sysinit.target
       ln -sf ${systemd_unitdir}/system/find_partitions.service \
            ${D}${systemd_unitdir}/system/sysinit.target.wants/find_partitions.service
    else
       install -m 0755 ${S}/find_partitions.sh -D ${D}${sysconfdir}/init.d/find_partitions.sh
    fi
}

