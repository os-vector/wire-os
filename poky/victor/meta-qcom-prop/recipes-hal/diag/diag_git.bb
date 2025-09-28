inherit qcommon qprebuilt qlicense systemd

DESCRIPTION = "Library and routing applications for diagnostic traffic"
DEPENDS = "common glib-2.0 system-core time-genoff"
RDEPENDS:${PN} = "time-genoff system-core"

PR = "r10"

FILESPATH =+ "${WORKSPACE}/:"
SRC_URI   = "file://diag/"
SRC_URI  += "file://chgrp-diag"
SRC_URI  += "file://chgrp-diag.service"

SRC_DIR = "${WORKSPACE}/diag"
S       = "${UNPACKDIR}/diag"

CFLAGS += "-Wno-error -Wno-implicit-function-declaration"
CXXFLAGS += "-Wno-error -Wno-implicit-function-declaration"
LDFLAGS += "-Wl,--allow-multiple-definition"

EXTRA_OECONF += "--with-glib \
                 --with-common-includes=${STAGING_INCDIR} \
                 --enable-target=${BASEMACHINE}"

do_install:append() {
        if ${@bb.utils.contains('DISTRO_FEATURES', 'systemd', 'true', 'false', d)}; then
          install -m 0755 ${UNPACKDIR}/chgrp-diag -D ${D}${sysconfdir}/initscripts/chgrp-diag
          install -d ${D}/etc/systemd/system/
          install -m 0644 ${UNPACKDIR}/chgrp-diag.service -D ${D}/etc/systemd/system/chgrp-diag.service
          install -d ${D}/etc/systemd/system/multi-user.target.wants/
          # enable the service for multi-user.target
          ln -sf /etc/systemd/chgrp-diag.service \
             ${D}/etc/systemd/system/multi-user.target.wants/chgrp-diag.service
        else
          install -m 0755 ${UNPACKDIR}/chgrp-diag -D ${D}${sysconfdir}/init.d/chgrp-diag    
        fi
}

pkg_postinst:${PN} () {

       if ${@bb.utils.contains('DISTRO_FEATURES', 'systemd', 'false', 'true', d)}; then
        [ -n "$D" ] && OPT="-r $D" || OPT="-s"
        update-rc.d $OPT -f start_diag_qshrink4_daemon remove
        update-rc.d $OPT start_diag_qshrink4_daemon start 15 2 3 4 5 . stop 15 0 6 .
        
        update-rc.d $OPT -f chgrp-diag remove
        update-rc.d $OPT chgrp-diag start 15 2 3 4 5 .
       fi
}

FILES:${PN} += "${systemd_unitdir}/system/"
