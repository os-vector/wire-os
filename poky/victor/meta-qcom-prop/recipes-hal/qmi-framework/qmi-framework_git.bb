inherit qcommon systemd update-rc.d qprebuilt qlicense

DESCRIPTION = "QMI Framework Library"

PR = "r8"

SRC_DIR = "${WORKSPACE}/qmi-framework"
S = "${UNPACKDIR}/qmi-framework"
SRC_URI += "file://irsc_util.service"

DEPENDS = "qmi"
DEPENDS += "glib-2.0"

CFLAGS += "-Wno-implicit-function-declaration"

EXTRA_OECONF = "--with-qmux-libraries=${STAGING_LIBDIR} --with-glib \
               --enable-static=no \
                --with-sysroot=${STAGING_LIBDIR}/../.."

INITSCRIPT_NAME = "init_irsc_util"
INITSCRIPT_PARAMS = "start 29 2 3 4 5 . stop 71 0 1 6 ."

do_install:append() {
    if ${@bb.utils.contains('DISTRO_FEATURES', 'systemd', 'true', 'false', d)}; then
       install -d ${D}${systemd_unitdir}/system/
       install -m 0644 ${UNPACKDIR}/irsc_util.service -D ${D}${systemd_unitdir}/system/irsc_util.service
       install -d ${D}${systemd_unitdir}/system/sysinit.target.wants/
       # enable the service for sysinit.target
       ln -sf ${systemd_unitdir}/system/irsc_util.service \
            ${D}${systemd_unitdir}/system/sysinit.target.wants/irsc_util.service
    else
       install -m 0755 ${S}/irsc_util/start_irsc_util -D ${D}${sysconfdir}/init.d/${INITSCRIPT_NAME}
    fi

}

pkg_postinst:${PN} () {
     if ${@bb.utils.contains('DISTRO_FEATURES', 'systemd', 'false', 'true', d)}; then
      [ -n "$D" ] && OPT="-r $D" || OPT="-s"
      update-rc.d $OPT -f ${INITSCRIPT_NAME} remove
      update-rc.d $OPT ${INITSCRIPT_NAME} ${INITSCRIPT_PARAMS}
     fi
}

FILES:${PN} += "${systemd_unitdir}/system/" 
