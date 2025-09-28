DESCRIPTION = "Start up script for finding partitions used in recovery"
HOMEPAGE = "http://codeaurora.org"
LIC_FILES_CHKSUM = "file://${COREBASE}/meta/files/common-licenses/BSD-3-Clause;md5=550794465ba0ec5312d6919e203a55f9"
LICENSE = "BSD-3-Clause"

SRC_URI += "file://${BASEMACHINE}/find_recovery_partitions.sh"
SRC_URI += "file://find-recovery-partitions.service"
S = "${UNPACKDIR}/${BASEMACHINE}"
#UNPACKDIR = "${S}"
PR = "r3"

inherit update-rc.d

INITSCRIPT_NAME = "find_recovery_partitions.sh"
INITSCRIPT_PARAMS = "start 38 S ."
INITSCRIPT_PARAMS_mdm = "start 38 S ."

do_install() {
    install -m 0755 ${S}/find_recovery_partitions.sh -D ${D}${sysconfdir}/init.d/find_recovery_partitions.sh
    if ${@bb.utils.contains('DISTRO_FEATURES', 'systemd', 'true', 'false', d)}; then
              install -d ${D}${systemd_unitdir}/system/
       install -m 0644 ${S}/find-recovery-partitions.service -D ${D}${systemd_unitdir}/system/find-recovery-partitions.service
       install -d ${D}${systemd_unitdir}/system/multi-user.target.wants/
       # enable the service for sysinit.target
       ln -sf ${systemd_unitdir}/system/find-recovery-partitions.service \
            ${D}${systemd_unitdir}/system/multi-user.target.wants/find-recovery-partitions.service
    fi
}

FILES:${PN} += "/lib/"
FILES:${PN} += "${systemd_unitdir}/system/"
