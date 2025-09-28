SUMMARY = "Client for Wi-Fi Protected Access (WPA)"
HOMEPAGE = "http://w1.fi/wpa_supplicant/"
BUGTRACKER = "http://w1.fi/security/"
SECTION = "network"
LICENSE = "BSD"
LIC_FILES_CHKSUM = "file://COPYING;md5=292eece3f2ebbaa25608eed8464018a3 \
                    file://README;beginline=1;endline=56;md5=3f01d778be8f953962388307ee38ed2b \
                    file://wpa_supplicant/wpa_supplicant.c;beginline=1;endline=12;md5=4061612fc5715696134e3baf933e8aba"
DEPENDS = "dbus libnl"
RRECOMMENDS:${PN} = "wpa-supplicant-passphrase wpa-supplicant-cli"

PACKAGECONFIG ?= "openssl"
PACKAGECONFIG[gnutls] = ",,gnutls libgcrypt"
PACKAGECONFIG[openssl] = ",,openssl"

inherit pkgconfig systemd

BUSNAME:pn-wpa-supplicant = "fi.w1.wpa_supplicant1"
SYSTEMD_SERVICE:${PN} = "wpa_supplicant.service wpa_supplicant-nl80211@.service wpa_supplicant-wired@.service"
SYSTEMD_AUTO_ENABLE = "disable"

SRC_URI = "http://w1.fi/releases/wpa_supplicant-${PV}.tar.gz  \
           file://defconfig \
           file://wpa-supplicant.sh \
           file://wpa_supplicant.conf \
           file://wpa_supplicant.conf-sane \
           file://99_wpa_supplicant \
           file://key-replay-cve-multiple1.patch \
           file://key-replay-cve-multiple2.patch \
           file://key-replay-cve-multiple3.patch \
           file://key-replay-cve-multiple4.patch \
           file://key-replay-cve-multiple5.patch \
           file://key-replay-cve-multiple6.patch \
           file://key-replay-cve-multiple7.patch \
           file://key-replay-cve-multiple8.patch \
          "
SRC_URI[md5sum] = "091569eb4440b7d7f2b4276dbfc03c3c"
SRC_URI[sha256sum] = "b4936d34c4e6cdd44954beba74296d964bc2c9668ecaa5255e499636fe2b1450"

CVE_PRODUCT = "wpa_supplicant"

S = "${UNPACKDIR}/wpa_supplicant-${PV}"

PACKAGES:prepend = "wpa-supplicant-passphrase wpa-supplicant-cli "
FILES:wpa-supplicant-passphrase = "${bindir}/wpa_passphrase"
FILES:wpa-supplicant-cli = "${sbindir}/wpa_cli"
FILES:${PN} += "${datadir}/dbus-1/system-services/*"
CONFFILES:${PN} += "${sysconfdir}/wpa_supplicant.conf"

#CC = "${WORKSPACE}/old-toolchain/arm/bin/arm-linux-gnueabi-gcc"
#CXX = "${WORKSPACE}/old-toolchain/arm/bin/arm-linux-gnueabi-g++"
#LD = "${WORKSPACE}/old-toolchain/arm/bin/arm-linux-gnueabi-ld"

#CC = "${WORKSPACE}/old-toolchain/arm/bin/arm-linux-gnueabi-gcc --sysroot=${WORKSPACE}/poky/build/tmp-glibc/work/armv7a-neon-vfpv4-oe-linux-gnueabi/${PN}/${PV}/recipe-sysroot"
#CXX = "${WORKSPACE}/old-toolchain/arm/bin/arm-linux-gnueabi-g++ --sysroot=${WORKSPACE}/poky/build/tmp-glibc/work/armv7a-neon-vfpv4-oe-linux-gnueabi/${PN}/${PV}/recipe-sysroot"
#LD = "${WORKSPACE}/old-toolchain/arm/bin/arm-linux-gnueabi-ld --sysroot=${WORKSPACE}/poky/build/tmp-glibc/work/armv7a-neon-vfpv4-oe-linux-gnueabi/${PN}/${PV}/recipe-sysroot"

do_configure:prepend () {
    export CFLAGS="$(echo $CFLAGS | sed 's/-fcanon-prefix-map=[^ ]*//g' | sed 's/-fcanon-prefix-map//g' | sed 's/-fmacro-prefix-map=[^ ]*//g' | sed 's/-fdebug-prefix-map=[^ ]*//g' | sed 's/-ffile-prefix-map=[^ ]*//g')"
    export CXXFLAGS="$(echo $CXXFLAGS | sed 's/-fcanon-prefix-map=[^ ]*//g' | sed 's/-fcanon-prefix-map//g' | sed 's/-fmacro-prefix-map=[^ ]*//g' | sed 's/-fdebug-prefix-map=[^ ]*//g' | sed 's/-ffile-prefix-map=[^ ]*//g')"
    export LDFLAGS="$(echo $LDFLAGS | sed 's/-fcanon-prefix-map=[^ ]*//g' | sed 's/-fcanon-prefix-map//g' | sed 's/-fmacro-prefix-map=[^ ]*//g' | sed 's/-fdebug-prefix-map=[^ ]*//g' | sed 's/-ffile-prefix-map=[^ ]*//g')"
}


# this is really bad
#do_compile:prepend() {
#    ln -sf ${STAGING_LIBDIR}/libnl-3.so ${STAGING_LIBDIR}/libnl.so
#    ln -sf ${STAGING_LIBDIR}/libnl-3.a  ${STAGING_LIBDIR}/libnl.a
#}


do_configure () {
	${MAKE} -C wpa_supplicant clean
	#sed -e '/^CONFIG_TLS=/d' <wpa_supplicant/defconfig >wpa_supplicant/.config
	install -m 0777 ${UNPACKDIR}/defconfig wpa_supplicant/.config
	#sed -i 's/#CONFIG_LIBNL32=y/CONFIG_LIBNL32=y/g' wpa_supplicant/.config
	echo "CFLAGS +=\"-I${STAGING_INCDIR}/libnl3\"" >> wpa_supplicant/.config
	echo "DRV_CFLAGS +=\"-I${STAGING_INCDIR}/libnl3\"" >> wpa_supplicant/.config
	
	if echo "${PACKAGECONFIG}" | grep -qw "openssl"; then
        	ssl=openssl
	elif echo "${PACKAGECONFIG}" | grep -qw "gnutls"; then
        	ssl=gnutls
	fi
	if [ -n "$ssl" ]; then
        	sed -i "s/%ssl%/$ssl/" wpa_supplicant/.config
	fi

	# For rebuild
	rm -f wpa_supplicant/*.d wpa_supplicant/dbus/*.d
}

export EXTRA_CFLAGS = "${CFLAGS}"
export BINDIR = "${sbindir}"

do_compile () {
	unset CFLAGS CPPFLAGS CXXFLAGS
        export EXTRA_CFLAGS="$(echo $EXTRA_CFLAGS | sed 's/-fcanon-prefix-map=[^ ]*//g' | sed 's/-fcanon-prefix-map//g' | sed 's/-fmacro-prefix-map=[^ ]*//g' | sed 's/-fdebug-prefix-map=[^ ]*//g' | sed 's/-ffile-prefix-map=[^ ]*//g')"
        export CXXFLAGS="$(echo $CXXFLAGS | sed 's/-fcanon-prefix-map=[^ ]*//g' | sed 's/-fcanon-prefix-map//g' | sed 's/-fmacro-prefix-map=[^ ]*//g' | sed 's/-fdebug-prefix-map=[^ ]*//g' | sed 's/-ffile-prefix-map=[^ ]*//g')"
        export LDFLAGS="$(echo $LDFLAGS | sed 's/-fcanon-prefix-map=[^ ]*//g' | sed 's/-fcanon-prefix-map//g' | sed 's/-fmacro-prefix-map=[^ ]*//g' | sed 's/-fdebug-prefix-map=[^ ]*//g' | sed 's/-ffile-prefix-map=[^ ]*//g')"
	sed -e "s:CFLAGS\ =.*:& \$(EXTRA_CFLAGS):g" -i ${S}/src/lib.rules
	oe_runmake -C wpa_supplicant
}

do_install () {
	install -d ${D}${sbindir}
	install -m 755 wpa_supplicant/wpa_supplicant ${D}${sbindir}
	install -m 755 wpa_supplicant/wpa_cli        ${D}${sbindir}

	install -d ${D}${bindir}
	install -m 755 wpa_supplicant/wpa_passphrase ${D}${bindir}

	install -d ${D}${docdir}/wpa_supplicant
	install -m 644 wpa_supplicant/README ${UNPACKDIR}/wpa_supplicant.conf ${D}${docdir}/wpa_supplicant

	install -d ${D}${sysconfdir}
	install -m 600 ${UNPACKDIR}/wpa_supplicant.conf-sane ${D}${sysconfdir}/wpa_supplicant.conf

	install -d ${D}${sysconfdir}/network/if-pre-up.d/
	install -d ${D}${sysconfdir}/network/if-post-down.d/
	install -d ${D}${sysconfdir}/network/if-down.d/
	install -m 755 ${UNPACKDIR}/wpa-supplicant.sh ${D}${sysconfdir}/network/if-pre-up.d/wpa-supplicant
	cd ${D}${sysconfdir}/network/ && \
	ln -sf ../if-pre-up.d/wpa-supplicant if-post-down.d/wpa-supplicant

	install -d ${D}/${sysconfdir}/dbus-1/system.d
	install -m 644 ${S}/wpa_supplicant/dbus/dbus-wpa_supplicant.conf ${D}/${sysconfdir}/dbus-1/system.d
	install -d ${D}/${datadir}/dbus-1/system-services
	install -m 644 ${S}/wpa_supplicant/dbus/*.service ${D}/${datadir}/dbus-1/system-services

	if ${@bb.utils.contains('DISTRO_FEATURES','systemd','true','false',d)}; then
		install -d ${D}/${systemd_unitdir}/system
		install -m 644 ${S}/wpa_supplicant/systemd/*.service ${D}/${systemd_unitdir}/system
#		sed -i 's|BusName=|BusName=fi.w1.wpa_supplicant1|g' ${D}/${systemd_unitdir}/system/wpa_supplicant.service
#                sed -i 's|Alias=dbus-|Alias=dbus-fi.w1.wpa_supplicant1|g' ${D}/${systemd_unitdir}/system/wpa_supplicant.service
	fi

	install -d ${D}/etc/default/volatiles
	install -m 0644 ${UNPACKDIR}/99_wpa_supplicant ${D}/etc/default/volatiles
}

pkg_postinst:wpa-supplicant () {
	# If we're offline, we don't need to do this.
	if [ "x$D" = "x" ]; then
		killall -q -HUP dbus-daemon || true
	fi

}
