DESCRIPTION = "fake-hwclock tool to keep the time since RTC does not work"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://LICENSE;md5=a109bd21357ed8a1fa56e8879764d28d"

DEPENDS = "bash"

FILESPATH =+ "${WORKSPACE}:"
SRC_URI = "file://external/fake-hwclock"

S = "${UNPACKDIR}/external/fake-hwclock"
SYSTEM_DIR = "${D}${sysconfdir}/systemd/system"

do_compile() {
}

do_install() {
   mkdir -p ${D}/usr/sbin
   cp ${S}/fake-hwclock ${D}/usr/sbin/
   chmod 0755 ${D}/usr/sbin/fake-hwclock
   if ${@bb.utils.contains('DISTRO_FEATURES', 'systemd', 'true', 'false', d)}; then
      install -d ${SYSTEM_DIR}/
      install -d ${SYSTEM_DIR}/multi-user.target.wants/
      for f in fake-hwclock-earlyboot.service \
               fake-hwclock.service \
               fake-hwclock-tick.service \
               fake-hwclock-tick.timer;
      do
         install -m 0644 ${S}/${f} -D ${SYSTEM_DIR}/${f}
         ln -sf /etc/systemd/system/${f} ${SYSTEM_DIR}/multi-user.target.wants/${f}
      done

      install -d ${SYSTEM_DIR}/sockets.target.wants/
      install -m 0644 ${S}/fake-hwclock-cmd.socket -D ${SYSTEM_DIR}/fake-hwclock-cmd.socket
      install -m 0644 ${S}/fake-hwclock-cmd.service -D ${SYSTEM_DIR}/fake-hwclock-cmd.service
      ln -sf /etc/systemd/system/fake-hwclock-cmd.socket ${SYSTEM_DIR}/sockets.target.wants/fake-hwclock-cmd.socket
  fi
}

FILES:${PN} += "usr/sbin"
FILES:${PN} += " ${systemd_unitdir}/system/"
