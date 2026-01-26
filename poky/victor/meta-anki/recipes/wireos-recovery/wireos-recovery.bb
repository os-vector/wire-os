DESCRIPTION = "Silly wire daemon for recovery"
LICENSE = "Anki-Inc.-Proprietary"                                                                   
LIC_FILES_CHKSUM = "file://${COREBASE}/../victor/meta-qcom/files/anki-licenses/\                           
Anki-Inc.-Proprietary;md5=4b03b8ffef1b70b13d869dbce43e8f09"

SERVICE_FILE = "wireos-recovery.service"

SRC_URI = "file://${SERVICE_FILE}"
S = "${UNPACKDIR}"
#UNPACKDIR = "${S}"

inherit systemd

do_install:append () {
   if ${@bb.utils.contains('DISTRO_FEATURES', 'systemd', 'true', 'false', d)}; then
       install -d ${D}${systemd_unitdir}/system/
       install -m 0644 ${S}/${SERVICE_FILE} -D ${D}${systemd_unitdir}/system/${SERVICE_FILE}
   fi
}

FILES:${PN} += "${systemd_unitdir}/system/"
SYSTEMD_SERVICE:${PN} = "${SERVICE_FILE}"

inherit externalsrc

EXTERNALSRC = "${WORKSPACE}/anki/wireos-recovery"

do_clean:append () {
    dir = bb.data.expand("${EXTERNALSRC}", d)
    os.system('cd "%s" && rm -rf build/* && rm vector-gobot/build/*' % dir)
}


run_victor() {
  export -n CCACHE_DISABLE
  export CCACHE_DIR="${HOME}/.ccache"
  env \
    -u AR \
    -u AS \
    -u BUILD_AR \
    -u BUILD_AS \
    -u BUILD_CC \
    -u BUILD_CCLD \
    -u BUILD_CFLAGS \
    -u BUILD_CPP \
    -u BUILD_CPPFLAGS \
    -u BUILD_CXX \
    -u BUILD_CXXFLAGS \
    -u BUILD_FC \
    -u CPPFLAGS \
    -u LC_ALL \
    -u LD \
    -u LDFLAGS \
    -u MAKE \
    -u NM \
    -u OBJCOPY \
    -u OBJDUMP \
    -u PATCH_GET \
    -u PKG_CONFIG_DIR \
    -u PKG_CONFIG_DISABLE_UNINSTALLED \
    -u PKG_CONFIG_LIBDIR \
    -u PKG_CONFIG_PATH \
    -u PKG_CONFIG_SYSROOT_DIR \
    -u PSEUDO_DISABLED \
    -u PSEUDO_UNLOAD \
    -u RANLIB \
    -u STRINGS \
    -u STRIP \
    -u TARGET_CFLAGS \
    -u TARGET_CPPFLAGS \
    -u TARGET_CXXFLAGS \
    -u TARGET_LDFLAGS \
    -u TOPLEVEL \
    -u WORKSPACE \
    -u base_bindir \
    -u base_libdir \
    -u base_prefix \
    -u base_sbindir \
    -u bindir \
    -u datadir \
    -u docdir \
    -u exec_prefix \
    -u includedir \
    -u infodir \
    -u libdir \
    -u libexecdir \
    -u localstatedir \
    -u mandir \
    -u nonarch_base_libdir \
    -u nonarch_libdir \
    -u oldincludedir \
    -u prefix \
    -u sbindir \
    -u servicedir \
    -u sharedstatedir \
    -u sysconfdir \
    -u systemd_system_unitdir \
    -u systemd_unitdir \
    -u systemd_user_unitdir \
    -u userfsdatadir \
    -i PATH=/usr/bin:/bin:/usr/sbin:/sbin HOME=$HOME PWD="${WORKSPACE}/anki/wireos-recovery" \
    "$@"
}

do_compile[pseudo] = "0"
do_compile[network] = "1"

do_compile() {
    cd "${EXTERNALSRC}"
    run_victor make
}

do_install () {
    install -d ${D}/lib
    tar -zxvf ${WORKSPACE}/anki/wireos-recovery/build/anki.tar.gz -C ${D}/
    install -p -m 0755 ${WORKSPACE}/anki/wireos-recovery/build/wireos-recovery ${D}/anki/bin/
    install -p -m 0755 ${WORKSPACE}/anki/wireos-recovery/build/libvector-gobot.so ${D}/lib/
    install -p -m 0755 ${WORKSPACE}/anki/wireos-recovery/build/font.ttf ${D}/anki/bin/
    echo "1" > ${D}/anki/etc/revision
    echo "3.0.1.0" > ${D}/anki/etc/version
}

FILES:${PN} += "anki/"
FILES:${PN} += "lib/libvector-gobot.so"

FILES:${PN}-dev = ""
do_package_qa[noexec] = "1"

INSANE_SKIP:${PN} = " already-stripped ldflags dev-elf"
EXCLUDE_FROM_SHLIBS = "1"
