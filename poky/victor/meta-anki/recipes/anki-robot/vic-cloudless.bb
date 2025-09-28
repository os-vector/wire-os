DESCRIPTION = "Victor Cloud Services daemon"
LICENSE = "Anki-Inc.-Proprietary"                                                                   
LIC_FILES_CHKSUM = "file://${COREBASE}/../victor/meta-qcom/files/anki-licenses/\                           
Anki-Inc.-Proprietary;md5=4b03b8ffef1b70b13d869dbce43e8f09"

SERVICE_FILE = "vic-cloud.service"
# GOINSTALLER="go1.15.6.linux-amd64.tar.gz"

SRC_URI = "file://${SERVICE_FILE}"
S = "${UNPACKDIR}"
#UNPACKDIR = "${S}"

inherit systemd

DEPENDS = "pkgconfig-native"

do_install:append () {
   if ${@bb.utils.contains('DISTRO_FEATURES', 'systemd', 'true', 'false', d)}; then
       install -d ${D}${systemd_unitdir}/system/
       install -m 0644 ${S}/${SERVICE_FILE} -D ${D}${systemd_unitdir}/system/${SERVICE_FILE}
   fi
}

FILES:${PN} += "${systemd_unitdir}/system/"
SYSTEMD_SERVICE:${PN} = "${SERVICE_FILE}"

inherit externalsrc

EXTERNALSRC = "${WORKSPACE}/anki/vic-cloudless"

GID_ANKI      = '2901'
GID_CLOUD     = '888'
GID_ANKINET   = '2905'

UID_NET       = "${GID_ANKINET}"
UID_CLOUD     = "${GID_CLOUD}"

do_clean:append() {
    s = d.getVar('S')
    os.system('cd "%s" && rm -rf build/vic-cloud build/vic-gateway build/libvosk.so build/libopus*' % s)
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
    -i PATH=/usr/bin:/bin:/usr/sbin:/sbin HOME=$HOME PWD="${EXTERNALSRC}" \
    "$@"
}

do_compile[pseudo] = "0"

do_compile() {
    # mkdir -p "${GOPATH}"
    # mkdir -p "${GOEXEPATH}"

    # if [ ! -f "${GOEXEPATH}/bin/go" ]; then
    #    wget -P "${WORKDIR}" "https://golang.org/dl/${GOINSTALLER}"
    #    tar zxvf "${WORKDIR}/${GOINSTALLER}" -C "${GOEXEPATH}"
    # fi

    cd "${EXTERNALSRC}"
    # export GOPATH="${GOPATH}"
    # export PATH="${GOEXEPATH}/go/bin:${PATH}"
    # using system Go
    run_victor make
}

do_install() {
    install -d ${D}/anki/bin
    install -d ${D}/anki/lib
    install -d ${D}/anki/data/assets/cozmo_resources/cloudless
    install -d ${D}/etc/sudoers.d
    install -d ${D}/usr/sbin

    install -m 0755 ${WORKSPACE}/anki/vic-cloudless/build/vic-* ${D}/anki/bin/
    install -m 0644 ${WORKSPACE}/anki/vic-cloudless/build/lib* ${D}/anki/lib/
    cp -r ${WORKSPACE}/anki/vic-cloudless/build/en-US ${D}/anki/data/assets/cozmo_resources/cloudless/

    install -m 0440 ${WORKSPACE}/anki/vic-cloudless/extra/cloud.sudoers ${D}/etc/sudoers.d/cloud
    install -m 0755 ${WORKSPACE}/anki/vic-cloudless/extra/setfreq ${D}/usr/sbin/
}

do_package_qa[noexec] = "1"

INSANE_SKIP:${PN} = " already-stripped ldflags dev-elf"
EXCLUDE_FROM_SHLIBS = "1"

FILES:${PN} += "anki/bin"
FILES:${PN} += "anki/bin"
FILES:${PN} += "anki/lib"
FILES:${PN} += "anki/data/assets/cozmo_resources/cloudless"
FILES:${PN} += "usr/sbin"
FILES:${PN} += "etc/sudoers.d"
