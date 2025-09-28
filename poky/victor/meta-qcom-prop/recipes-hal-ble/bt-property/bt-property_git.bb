inherit autotools-brokensep pkgconfig systemd update-rc.d

DESCRIPTION = "Bluetooth Property Daemon"
HOMEPAGE = "http://codeaurora.org/"
LICENSE = "Apache-2.0"

LIC_FILES_CHKSUM = "file://${COREBASE}/meta/files/common-licenses/\
${LICENSE};md5=89aea4e17d99a7cacdbeed46a0096b10"

DEPENDS = "common glib-2.0 liblog"
FILESPATH =+ "${WORKSPACE}:"
SRC_URI = "file://qcom-opensource/bt/property-ops/"
SRC_URI += "file://btproperty.service"

EXTRA_OECONF = " \
                --with-glib \
               "
S = "${UNPACKDIR}/qcom-opensource/bt/property-ops"

CFLAGS:append = " -DUSE_ANDROID_LOGGING -fno-strict-aliasing -fno-tree-vectorize "
LDFLAGS:append = " -llog "

INITSCRIPT_NAME = "start_btproperty"
INITSCRIPT_PARAMS = "start 98 5 . stop 2 0 1 6 ."

do_install:append() {
    #if ${@bb.utils.contains('DISTRO_FEATURES', 'systemd', 'true', 'false', d)}; then
	install -d ${D}${sysconfdir}/initscripts/
	install "${S}/start_btproperty" ${D}${sysconfdir}/initscripts/start_btproperty
	install -d ${D}${sysconfdir}/systemd/system/
	install -m 0644 ${UNPACKDIR}/btproperty.service \
	    -D ${D}${sysconfdir}/systemd/system/btproperty.service
	install -d ${D}${sysconfdir}/systemd/system/multi-user.target.wants/
	ln -sf /etc/systemd/system/btproperty.service \
	    ${D}${sysconfdir}/systemd/system/multi-user.target.wants/btproperty.service
    #else
	#install -d ${D}${sysconfdir}
	#install -d ${D}${sysconfdir}/init.d
	#install "${S}/start_btproperty" ${D}${sysconfdir}/init.d
    #fi
}

FILES:${PN} += "${systemd_unitdir}/system/"
