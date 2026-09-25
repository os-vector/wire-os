FILESEXTRAPATHS:prepend := "${THISDIR}/files:"

SRC_URI += "file://avahi-daemon.conf \
            file://avahi-daemon.service \
            file://ankivector.service \
            file://vhostname \
            file://vhostname-publish \
            file://vhostname-publish.service \
            file://0002-static-hosts-no-reverse.patch"

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
      install -m 0644 ${UNPACKDIR}/ankivector.service ${D}${sysconfdir}/avahi/services/ankivector.service
      rm -f ${D}${sysconfdir}/avahi/hosts
      ln -sf /run/avahi/hosts ${D}${sysconfdir}/avahi/hosts
      install -d ${D}${sbindir}
      install -m 0755 ${UNPACKDIR}/vhostname ${D}${sbindir}/vhostname
      install -m 0755 ${UNPACKDIR}/vhostname-publish ${D}${sbindir}/vhostname-publish
      install -m 0644 ${UNPACKDIR}/vhostname-publish.service ${D}/usr/lib/systemd/system/vhostname-publish.service
  fi
}

FILES:${PN} += "/usr/lib/systemd/system/multi-user.target.requires"
FILES:avahi-daemon:append = " ${sbindir}/vhostname ${sbindir}/vhostname-publish"
SYSTEMD_SERVICE:${PN}-daemon:append = " vhostname-publish.service"
