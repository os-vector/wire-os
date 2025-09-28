inherit qcommon qprebuilt systemd

DESCRIPTION = "Anki Bluetooth Daemon"
LICENSE = "Anki-Inc.-Proprietary"
LIC_FILES_CHKSUM = "file://${COREBASE}/../victor/meta-qcom/files/anki-licenses/\
Anki-Inc.-Proprietary;md5=4b03b8ffef1b70b13d869dbce43e8f09"

# CPPFLAGS:append = " -DUSE_ANDROID_LOGGING -fno-strict-aliasing -fno-tree-vectorize "
# CFLAGS:append = " -DUSE_ANDROID_LOGGING -fno-strict-aliasing -fno-tree-vectorize "
# LDFLAGS:append = " -llog "

FILESPATH =+ "${WORKSPACE}:"
SRC_URI = "file://ankibluetoothd"
SRC_URI += "file://ankibluetoothd.service"
SRC_URI += "file://smd23.rules"

SRC_DIR   = "${WORKSPACE}/ankibluetoothd"

S = "${UNPACKDIR}/ankibluetoothd"
#UNPACKDIR = "${S}"

DEPENDS += "btvendorhal libhardware bt-property"

do_prebuilt_install:append() {
  install -d ${D}${sysconfdir}/systemd/system/multi-user.target.wants/
  ln -sf /etc/systemd/system/ankibluetoothd.service \
    ${D}${sysconfdir}/systemd/system/multi-user.target.wants/ankibluetoothd.service
}

FILES:${PN} += "${systemd_unitdir}/system/"
FILES:${PN} += "${sysconfdir}/systemd"
FILES:${PN} += "${sysconfdir}/udev/rules.d/smd23.rules"

INSANE_SKIP:${PN} += "rpaths"
INSANE_SKIP:${PN} += "ldflags"
INSANE_SKIP:${PN} += "already-stripped"