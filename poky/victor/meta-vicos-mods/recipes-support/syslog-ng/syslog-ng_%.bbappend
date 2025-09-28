FILESEXTRAPATHS:prepend := "${THISDIR}/files:"
SRC_URI += " file://syslog-ng.conf.systemd"

do_install:append() {
    install -d ${D}/${sysconfdir}/${BPN}
    install -d ${D}/${sysconfdir}/init.d
    install -m 755 ${UNPACKDIR}/initscript ${D}/${sysconfdir}/init.d/syslog
    install -d ${D}/${sysconfdir}/default/volatiles/
    install -m 755 ${UNPACKDIR}/volatiles.03_syslog-ng ${D}/${sysconfdir}/default/volatiles/03_syslog-ng
    install -d ${D}/${localstatedir}/lib/${BPN}
    # Remove /var/run as it is created on startup
    rm -rf ${D}${localstatedir}/run

    # Remove loggen
    rm -rf = ${D}${libdir}/${BPN}/loggen
    rm -rf = ${D}${bindir}/loggen

    # support for systemd
    if ${@bb.utils.contains('DISTRO_FEATURES','systemd','true','false',d)}; then
        install ${UNPACKDIR}/syslog-ng.conf.systemd ${D}${sysconfdir}/${BPN}/${BPN}.conf

        #install -d ${D}${systemd_unitdir}/system/
        #install -m 0644 ${UNPACKDIR}/${BPN}@.service ${D}${systemd_unitdir}/system/${BPN}@.service
        #install -m 0644 ${S}/contrib/systemd/${BPN}@default ${D}${sysconfdir}/default/${BPN}@default

        #sed -i -e 's,@SBINDIR@,${sbindir},g' ${D}${systemd_unitdir}/system/${BPN}@.service ${D}${sysconfdir}/default/${BPN}@default
        #sed -i -e 's,@LOCALSTATEDIR@,${localstatedir},g' ${D}${systemd_unitdir}/system/${BPN}@.service ${D}${sysconfdir}/default/${BPN}@default
        #sed -i -e 's,@BASEBINDIR@,${base_bindir},g' ${D}${systemd_unitdir}/system/${BPN}@.service ${D}${sysconfdir}/default/${BPN}@default

        #install -d ${D}${systemd_unitdir}/system/multi-user.target.wants
        #ln -sf ../${BPN}@.service ${D}${systemd_unitdir}/system/multi-user.target.wants/${BPN}@default.service
    else
        install ${UNPACKDIR}/syslog-ng.conf.sysvinit ${D}${sysconfdir}/${BPN}/${BPN}.conf
    fi
}
