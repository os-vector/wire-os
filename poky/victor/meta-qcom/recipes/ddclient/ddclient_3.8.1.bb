DESCRIPTION = "DDClient is a perl based dynamic DNS Utility"
HOMEPAGE = "http://sourceforge.net/projects/ddclient/"
SECTION = "console/network"
LICENSE = "GPLv2"
LICENSE_${PN} = "GPLv2"
LIC_FILES_CHKSUM = "file://COPYING;md5=0636e73ff0215e8d672dc4c32c317bb3"
DEPENDS = "perl"

SRC_URI[md5sum] = "7fa417bc65f8f0e6ce78418a4f631988"
SRC_URI[sha256sum] = "77a82668a53fdbed1e05ad6febe6dbefb093e3922afb20b993d4ad9ee868258f"


inherit autotools pkgconfig

S = "${UNPACKDIR}/ddclient-${PV}"

#All the DDClient development is on sourceforge.met
PR = "r6"

SRC_URI = "\
    ${SOURCEFORGE_MIRROR}/ddclient/ddclient-${PV}.tar.bz2 \
    file://ddclient_conf.patch \
    file://ddclient_ipv6.patch \
    file://ddclient_ipv4v6.patch"

RDEPENDS:${PN} = "perl perl-module-strict perl-module-dynaloader perl-module-getopt-long perl-module-vars perl-module-warnings-register perl-module-warnings perl-module-carp perl-module-exporter perl-module-constant perl-module-exporter-heavy perl-module-sys-hostname perl-module-xsloader perl-module-autoloader perl-module-io-select perl-module-io-socket perl-module-io-handle perl-module-symbol perl-module-selectsaver perl-module-io perl-module-socket perl-module-errno perl-module-config perl-module-io-socket-inet perl-module-io-socket-unix perl-module-integer perl-module-overload"

do_compile() {
}

do_install() {
   install -m 0755 ${S}/ddclient -D ${D}${sbindir}/ddclient
   install -m 0644 ${S}/sample-etc_ddclient.conf -D ${D}${sysconfdir}/data/ddclient.conf
}

FILES:${PN} += "${sysconfdir}/data/ddclient.conf"
