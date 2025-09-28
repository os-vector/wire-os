DESCRIPTION = "ip6tables persistent service"
LICENSE = "Anki-Inc.-Proprietary"
LIC_FILES_CHKSUM = "file://${COREBASE}/../victor/meta-qcom/files/anki-licenses/\
Anki-Inc.-Proprietary;md5=4b03b8ffef1b70b13d869dbce43e8f09"

inherit systemd qperf
                                                                                                    
FILESPATH =+ "${WORKSPACE}:"

#RULES_TAG = "${@base_conditional('USER_BUILD', '1', 'user', 'eng', d)}"
#RULES_TAG .= "${@base_conditional('DEV', '1', 'dev', '', d)}"
#RULES_TAG .= "${@base_conditional('OSKR', '1', 'oskr', '', d)}"

RULES_TAG = "${@bb.utils.contains('USER_BUILD','1','user','eng',d)}${@bb.utils.contains('DEV','1','dev','',d)}${@bb.utils.contains('OSKR','1','oskr','',d)}"

S = "${UNPACKDIR}"
#UNPACKDIR = "${S}"

SRC_URI += "file://ip6tables.service"
SRC_URI += "file://ip6tables-user.rules"
SRC_URI += "file://ip6tables-userdev.rules"
SRC_URI += "file://ip6tables-eng.rules"
SRC_URI += "file://ip6tables-engoskr.rules"
SRC_URI += "file://ip6tables-flush.sh"

SYSTEM_DIR = "${D}${sysconfdir}/systemd/system"

do_install() {
  install -d ${D}/etc/iptables
  install -m 0644 ${UNPACKDIR}/ip6tables-${RULES_TAG}.rules ${D}/etc/iptables/ip6tables.rules
  install -m 0644 ${UNPACKDIR}/ip6tables-flush.sh ${D}/etc/iptables/ip6tables-flush.sh

  if ${@bb.utils.contains('DISTRO_FEATURES', 'systemd', 'true', 'false', d)}; then
    install -d  ${D}${systemd_unitdir}/system/
    install -m 0644 ${UNPACKDIR}/ip6tables.service -D ${D}${systemd_unitdir}/system/ip6tables.service

    install -d ${SYSTEM_DIR}/
    install -d ${SYSTEM_DIR}/multi-user.target.wants/

    ln -sf ${systemd_unitdir}/system/ip6tables.service \
      ${SYSTEM_DIR}/multi-user.target.wants/ip6tables.service
  fi
}

RDEPENDS:${PN} += "iptables"

FILES:${PN} += "/etc/iptables/ip6tables.rules"
FILES:${PN} += "/etc/iptables/ip6tables-flush.sh"
FILES:${PN} += "/usr/lib/systemd/system"
