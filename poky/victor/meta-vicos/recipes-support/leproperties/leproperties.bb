SUMMARY = "leproperties"
DESCRIPTION = "qualcomm setprop/getprop but standalone and simple"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

FILESPATH =+ "${WORKSPACE}:"
SRC_URI = "file://external/leproperties \
	   file://leproperties.service"

S = "${UNPACKDIR}/external/leproperties"

inherit cmake systemd

SYSTEMD_SERVICE:${PN} = "leproperties.service"
SYSTEMD_AUTO_ENABLE:${PN} = "enable"

DEPENDS = ""

do_install() {
    install -d ${D}${bindir}
    install -m 0755 leproperties ${D}${bindir}/leproperties
    install -m 0755 getprop ${D}${bindir}/getprop
    install -m 0755 setprop ${D}${bindir}/setprop

    install -d ${D}${systemd_system_unitdir}
    install -m 0644 ${UNPACKDIR}/leproperties.service \
        ${D}${systemd_system_unitdir}/leproperties.service
}

FILES:${PN} += "${systemd_system_unitdir}/leproperties.service"
