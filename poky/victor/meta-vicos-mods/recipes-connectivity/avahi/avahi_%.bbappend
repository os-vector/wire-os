FILESEXTRAPATHS:prepend := "${THISDIR}/files:"

SRC_URI += "file://avahi-daemon.conf \
            file://avahi-daemon.service"

# USERADD_PARAM:avahi-daemon = "--system --home /var/run/avahi-daemon \
#                               --no-create-home --shell /bin/false \
#                               --groups inet --user-group avahi"

do_install:append() {
  if ${@bb.utils.contains('DISTRO_FEATURES', 'avahi', 'true', 'false', d)}; then
      install -m 644 ${UNPACKDIR}/avahi-daemon.conf ${D}${sysconfdir}/avahi/avahi-daemon.conf
      rm -rf ${D}/usr/lib/systemd/system/avahi-daemon.service ${D}/usr/lib/systemd/system/multi-user.target.wants ${D}/etc/systemd/system/multi-user.target.wants ${D}/usr/lib/systemd/system/avahi-daemon.socket
      install -m 0644 ${UNPACKDIR}/avahi-daemon.service ${D}/usr/lib/systemd/system/avahi-daemon.service
      install -d ${D}/usr/lib/systemd/system/multi-user.target.requires
      ln -sf /usr/lib/systemd/system/avahi-daemon.service ${D}/usr/lib/systemd/system/multi-user.target.requires/avahi-daemon.service
  fi
}

FILES:${PN} += "/usr/lib/systemd/system/multi-user.target.requires"
