inherit systemd
DESCRIPTION = "Generate x509 cert for vic-gateway"
LICENSE = "Anki-Inc.-Proprietary"
LIC_FILES_CHKSUM = "file://${COREBASE}/../victor/meta-qcom/files/anki-licenses/\
Anki-Inc.-Proprietary;md5=4b03b8ffef1b70b13d869dbce43e8f09"

SRC_URI += "file://vic-gateway-cert.service"
SRC_URI += "file://vic-gateway-cert"
SRC_URI += "file://vic-gateway-cert.conf.in"
S = "${UNPACKDIR}"
#UNPACKDIR = "${S}"

inherit systemd

do_install() {
  install -d ${D}/usr/sbin
  install -m 0755 ${S}/vic-gateway-cert ${D}/usr/sbin/vic-gateway-cert
  install -d ${D}${sysconfdir}/systemd/system/
  install -m 0644 ${S}/vic-gateway-cert.service \
    -D ${D}${sysconfdir}/systemd/system/vic-gateway-cert.service 
  install -m 0644 ${S}/vic-gateway-cert.conf.in \
    ${D}/etc/vic-gateway-cert.conf.in
}

FILES:${PN} += "/usr/sbin/vic-gateway-cert"
FILES:${PN} += "/etc/vic-gateway-cert.conf.in"
SYSTEMD_SERVICE:${PN} = "vic-gateway-cert.service" 
