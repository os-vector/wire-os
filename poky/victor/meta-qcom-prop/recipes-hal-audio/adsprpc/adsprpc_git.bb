inherit qcommon qlicense qprebuilt systemd

SUMMARY = "adsprpc daemon"

DEPENDS += "system-core"

FILESPATH =+ "${WORKSPACE}/:"
FILESPATH =+ "${PN}/files:"
SRC_URI   = "file://adsprpc"
SRC_URI  += "file://start_adsprpcd"
SRC_URI  += "file://start_mdsprpcd"
SRC_URI  += "file://adsprpcd.service"
SRC_URI  += "file://mdsprpcd.service"

SRC_DIR   = "${WORKSPACE}/adsprpc"

S  = "${UNPACKDIR}/adsprpc"
PR = "r1"

EXTRA_OECONF += "--enable-mdsprpc"

INITSCRIPT_PACKAGES         = "${PN}"

INITSCRIPT_NAME_${PN} = "mdsprpcd"
INITSCRIPT_PARAMS_${PN}       = "start 70 2 3 4 5 S . stop 30 0 1 6 ."

inherit update-rc.d pkgconfig

do_prebuilt_install:append() {
    #install -m 0755 ${UNPACKDIR}/start_${INITSCRIPT_NAME_${PN}} -D ${D}${sysconfdir}/init.d/${INITSCRIPT_NAME_${PN}}

    # # Install systemd unit file
    # install -d ${D}${systemd_unitdir}/system
    # install -m 0644 ${UNPACKDIR}/${INITSCRIPT_NAME_${PN}}.service ${D}${systemd_unitdir}/system

    install -d ${D}/etc/systemd/system/multi-user.target.wants
    ln -sf /etc/systemd/system/mdsprpcd.service ${D}/etc/systemd/system/multi-user.target.wants/mdsprpcd.service
}

FILES:${PN}-dbg  = "${libdir}/.debug/* ${bindir}/.debug/*"
FILES:${PN}      = "${libdir}/libadsp*.so ${libdir}/libadsp*.so.* ${bindir}/adsprpcd"
FILES:${PN}     += "${libdir}/libmdsp*.so ${libdir}/libmdsp*.so.* ${bindir}/mdsprpcd"
FILES:${PN}     += "${sysconfdir}/init.d/adsprpcd ${sysconfdir}/init.d/mdsprpcd ${libdir}/pkgconfig/"
FILES:${PN}     += "usr/lib/systemd/system"
FILES:${PN}     += "etc/systemd/system"
FILES:${PN}-dev  = "${libdir}/*.la ${includedir}"
