DESCRIPTION = "Start up script for firmware links"
HOMEPAGE = "http://codeaurora.org"
LIC_FILES_CHKSUM = "file://${COREBASE}/meta/files/common-licenses/BSD-3-Clause;md5=550794465ba0ec5312d6919e203a55f9"
LICENSE = "BSD-3-Clause"

SRC_URI += "file://${BASEMACHINE}/firmware-links.sh"
SRC_URI += "file://firmware-links.service"

S = "${UNPACKDIR}/${BASEMACHINE}"
#UNPACKDIR = "${S}"
PR = "r5"

inherit systemd

do_install() {
    if ${@bb.utils.contains('DISTRO_FEATURES', 'ro-rootfs', 'false', 'true', d)}; then
        if ${@bb.utils.contains('DISTRO_FEATURES', 'systemd', 'true', 'false', d)}; then
           install -m 0755 ${S}/firmware-links.sh -D ${D}${sysconfdir}/initscripts/firmware-links.sh
           install -d ${D}${systemd_unitdir}/system/
           install -m 0644 ${S}/firmware-links.service -D ${D}${systemd_unitdir}/system/firmware-links.service
           install -d ${D}${systemd_unitdir}/system/sysinit.target.wants/
           # enable the service for sysinit.target
           ln -sf ${systemd_unitdir}/system/firmware-links.service \
                ${D}${systemd_unitdir}/system/sysinit.target.wants/firmware-links.service
        else
           install -m 0755 ${S}/firmware-links.sh -D ${D}${sysconfdir}/init.d/firmware-links.sh
        fi
    fi
}

# Beyond msm-3.18 /lib/firmware/image is no longer a valid PIL search path.
# Use /lib/firmware/updates instead till userspace is capable of firmware load.
do_install:append() {
    install -d ${D}/lib/firmware
    if ${@oe.utils.version_less_or_equal('PREFERRED_VERSION_linux-msm', '3.18', 'true', 'false', d)}; then
        ln -s /firmware/image ${D}/lib/firmware/image
    else
        ln -s /firmware/image ${D}/lib/firmware/updates
    fi
}

do_install:append_apq8017() {
    ln -s /firmware/image/btfw32.tlv ${D}/lib/firmware/
    ln -s /firmware/image/btnv32.bin ${D}/lib/firmware/
}

pkg_postinst_${PN} () {
    if ${@bb.utils.contains('DISTRO_FEATURES', 'ro-rootfs', 'false', 'true', d)}; then
        if ${@bb.utils.contains('DISTRO_FEATURES', 'systemd', 'false', 'true', d)}; then
             update-alternatives --install ${sysconfdir}/init.d/firmware-links.sh firmware-links.sh firmware-links.sh 60
             [ -n "$D" ] && OPT="-r $D" || OPT="-s"
             # remove all rc.d-links potentially created from alternatives
             update-rc.d $OPT -f firmware-links.sh remove
             update-rc.d $OPT firmware-links.sh multiuser
        fi
    fi
}

FILES:${PN} += "/lib/*"
FILES:${PN} += "${systemd_unitdir}/system/"

INSANE_SKIP:${PN} += " usrmerge"
INSANE_SKIP:${PN}-dbg += " usrmerge"
