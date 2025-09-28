inherit autotools systemd
DESCRIPTION = "Device specific config"
LICENSE = "ISC"
LIC_FILES_CHKSUM = "file://${COREBASE}/meta/files/common-licenses/${LICENSE};md5=f3b90e78ea0cffb20bf5cca7947a896d"
PR = "r3"

FILESPATH =+ "${WORKSPACE}:"
# Provide a baseline
SRC_URI = "file://mdm-init"
SRC_URI += "file://wlan_daemon.service"

# Update for each machine
S = "${UNPACKDIR}/mdm-init"

do_install:append(){
  if ${@bb.utils.contains('DISTRO_FEATURES', 'systemd', 'true', 'false', d)}; then
      install -d ${D}/etc/initscripts
#      cp ${D}/etc/init.d/wlan ${D}/etc/initscripts/wlan
#      install -d ${D}/etc/systemd/system/
#      install -d ${D}/etc/systemd/system/multi-user.target.wants/
#      install -m 0644 ${WORKDIR}/wlan_daemon.service -D ${D}/etc/systemd/system/wlan_daemon.service
#      ln -sf /etc/systemd/system/wlan_daemon.service ${D}/etc/systemd/system/multi-user.target.wants/wlan_daemon.service

      if ${@bb.utils.contains('COMBINED_FEATURES', 'qti-wifi', 'true', 'false', d)}; then
          mkdir -p ${D}/lib/firmware/wlan/qca_cld/wlan_debug
          ln -sf /lib/firmware/wlan/qca_cld/WCNSS_qcom_cfg.ini ${D}/lib/firmware/wlan/qca_cld/wlan_debug/WCNSS_qcom_cfg.ini
      fi
  else
      install -m 0755 ${S}/wlan_daemon -D ${D}${sysconfdir}/init.d/wlan_daemon
  fi
}
FILES:${PN} += "${userfsdatadir}/misc/wifi/*"
FILES:${PN} += "${base_libdir}/firmware/wlan/qca_cld/*"
FILES:${PN} += "/lib/firmware/wlan/qca_cld/* ${sysconfdir}/init.d/* "
FILES:${PN} += "${@bb.utils.contains('MACHINE_FEATURES', 'wlan-sdio', '/lib/firmware/wlan_sdio', '', d)}"


EXTRA_OECONF += "--enable-target-${BASEMACHINE}=yes"

EXTRA_OECONF += "${@bb.utils.contains("MACHINE_FEATURES", "naples", "--enable-naples-wlan=yes", "", d)}"
EXTRA_OECONF += "${@bb.utils.contains("MACHINE_FEATURES", "pronto", "--enable-pronto-wlan=yes", "", d)}"
