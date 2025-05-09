SUMMARY = "Proprietary Qualcomm programs"
DESCRIPTION = "proprietary things copied from a full OTA"
SECTION = "examples"
PR = "r1"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COREBASE}/meta/files/common-licenses/${LICENSE};md5=0835ade698e0bcf8506ecda2f7b4f302"

SRC_URI = "file://qtiroot \
	   file://initscripts \
	   file://services \
	   file://other"

#S = "${WORKDIR}"
S = "${WORKDIR}/sources"
UNPACKDIR = "${S}"

do_install () {
	install -d ${D}/etc/initscripts
	install -d ${D}/lib/systemd/system/multi-user.target.wants
	install -d ${D}/lib/systemd/system/local-fs.target.requires
	install -d ${D}/usr/bin
	install -d ${D}/usr/sbin
	install -d ${D}/data/misc/camera
	install -d ${D}/data/misc/bluetooth
	cp -r ${S}/other/export-gpio ${D}/usr/sbin/export-gpio
	cp -r ${S}/initscripts/* ${D}/etc/initscripts/
	chmod 0777 ${D}/etc/initscripts/*
	cp -r ${S}/qtiroot ${D}/usr/qtiroot
	ln -sf /usr/qtiroot/qtirun ${D}/usr/bin/qtirun
	cp -r ${S}/services/* ${D}/lib/systemd/system/
	ln -sf /lib/systemd/system/anki-audio-init.service ${D}/lib/systemd/system/multi-user.target.wants/
	# this is now installed by the update-engine recipe
	#ln -sf /lib/systemd/system/boot-successful.service ${D}/lib/systemd/system/multi-user.target.wants/
	ln -sf /lib/systemd/system/logd.service ${D}/lib/systemd/system/multi-user.target.wants/
	ln -sf /lib/systemd/system/mdsprpcd.service ${D}/lib/systemd/system/multi-user.target.wants/
	ln -sf /lib/systemd/system/mm-anki-camera.service ${D}/lib/systemd/system/multi-user.target.wants/
	ln -sf /lib/systemd/system/mm-qcamera-daemon.service ${D}/lib/systemd/system/multi-user.target.wants/
	ln -sf /lib/systemd/system/qtid.service ${D}/lib/systemd/system/multi-user.target.wants/
	ln -sf /lib/systemd/system/qti_system_daemon.service ${D}/lib/systemd/system/multi-user.target.wants/
	ln -sf /lib/systemd/system/rmt_storage.service ${D}/lib/systemd/system/multi-user.target.wants/
	ln -sf /lib/systemd/system/init_audio.service ${D}/lib/systemd/system/multi-user.target.wants/
	ln -sf /lib/systemd/system/ankibluetoothd.service ${D}/lib/systemd/system/multi-user.target.wants/
	ln -sf /lib/systemd/system/btproperty.service ${D}/lib/systemd/system/multi-user.target.wants/
	ln -sf /lib/systemd/system/leprop.service ${D}/lib/systemd/system/multi-user.target.wants/
	ln -sf /lib/systemd/system/mount-data.service ${D}/lib/systemd/system/local-fs.target.requires/
	ln -sf /lib/systemd/system/setup-qtiroot.service ${D}/lib/systemd/system/multi-user.target.wants/
	ln -sf /lib/systemd/system/setup-persist.service ${D}/lib/systemd/system/multi-user.target.wants/
}

FILES:${PN} = "/usr/qtiroot \
		/lib/systemd/system \
		/lib/systemd/system/multi-user.target.wants \
		/usr/bin/qtirun \
		/etc/initscripts \
		/data/misc/camera \
		/data/misc/bluetooth \
		/usr/sbin/export-gpio"

# yocto doesn't really allow precompiled binaries
INSANE_SKIP:${PN} = "file-rdeps"
INSANE_SKIP:${PN} += " libdir"
INSANE_SKIP:${PN} += " dev-elf"
INSANE_SKIP:${PN} += " dev-so"
INSANE_SKIP:${PN} += " ldflags"
INSANE_SKIP:${PN} += " installed-vs-shipped"
INHIBIT_PACKAGE_STRIP = "1"
INHIBIT_SYSROOT_STRIP = "1"
SOLIBS = ".so"
FILES:SOLIBSDEV = ""
PRIVATE_LIBS:${PN} = "/usr/qtiroot/.*"
FILES:${PN}-dev = ""
do_package_qa[noexec] = "1"
EXCLUDE_FROM_SHLIBS = "1"

# Prevents do_package failures with:
# debugsources.list: No such file or directory:
INHIBIT_PACKAGE_DEBUG_SPLIT = "1"

QA_SKIP_${PN} += ".*qtiroot.*"
