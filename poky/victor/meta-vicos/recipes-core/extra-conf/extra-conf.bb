inherit systemd

# this configures some very fundamental things. these should be separated out into their own recipes, probably

SUMMARY = "Extra OS configuration"
DESCRIPTION = "configuring qualcomm things"
PR = "r2"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COREBASE}/meta/files/common-licenses/${LICENSE};md5=0835ade698e0bcf8506ecda2f7b4f302"

SRC_URI = "file://initscripts \
	   file://services \
	   file://other \
           file://rsync"

S = "${UNPACKDIR}"
#UNPACKDIR = "${S}"

do_install () {
	install -d ${D}/etc/initscripts
	install -d ${D}/usr/lib/systemd/system/multi-user.target.wants
	install -d ${D}/usr/lib/systemd/system/local-fs.target.requires
	install -d ${D}/usr/sbin
	install -m 0755 ${S}/other/export-gpio ${D}/usr/sbin/export-gpio
	install -m 0755 ${S}/other/set-timezone ${D}/usr/sbin/set-timezone
        install -m 0755 ${S}/other/btop.conf ${D}/etc/btop.conf
	cp -r ${S}/initscripts/* ${D}/etc/initscripts/
	chmod 0755 ${D}/etc/initscripts/*
	cp -r ${S}/services/* ${D}/usr/lib/systemd/system/
	chmod 0644 ${D}/usr/lib/systemd/system/*
	ln -sf /usr/lib/systemd/system/anki-audio-init.service ${D}/usr/lib/systemd/system/multi-user.target.wants/
	ln -sf /usr/lib/systemd/system/logd.service ${D}/usr/lib/systemd/system/multi-user.target.wants/
	ln -sf /usr/lib/systemd/system/mm-anki-camera.service ${D}/usr/lib/systemd/system/multi-user.target.wants/
	ln -sf /usr/lib/systemd/system/mm-qcamera-daemon.service ${D}/usr/lib/systemd/system/multi-user.target.wants/
	ln -sf /usr/lib/systemd/system/mount-data.service ${D}/usr/lib/systemd/system/local-fs.target.requires/
	ln -sf /usr/lib/systemd/system/partition-links.service ${D}/usr/lib/systemd/system/multi-user.target.wants/
	ln -sf /usr/lib/systemd/system/enable-wifi.service ${D}/usr/lib/systemd/system/multi-user.target.wants/

	install -m 0644 ${S}/rsync/rsyncd-victor.conf ${D}/etc/rsyncd-victor.conf
	install -m 0644 ${S}/rsync/rsyncd.service ${D}/usr/lib/systemd/system/rsyncd.service
}

FILES:${PN} = "	/usr/lib/systemd/system \
		/usr/lib/systemd/system/multi-user.target.wants \
		/etc/initscripts \
		/usr/sbin/export-gpio \
		/usr/sbin/set-timezone \
		/etc/btop.conf \
		/etc/rsyncd-victor.conf"

RDEPENDS:${PN} = "bash"
