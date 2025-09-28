inherit autotools pkgconfig systemd update-rc.d

DESCRIPTION = "Android system/core components"
HOMEPAGE = "http://developer.android.com/"
LICENSE = "Apache-2.0"
LIC_FILES_CHKSUM = "file://${COREBASE}/meta/files/common-licenses/\
${LICENSE};md5=89aea4e17d99a7cacdbeed46a0096b10"

FILESPATH =+ "${WORKSPACE}:"
SRC_URI = "file://system/core"
S = "${UNPACKDIR}/system/core"

PR = "r19"

#CC = "${WORKSPACE}/old-toolchain/arm/bin/arm-linux-gnueabi-gcc --sysroot=${WORKSPACE}/poky/build/tmp-glibc/work/armv7a-neon-vfpv4-oe-linux-gnueabi/${PN}/${PV}/recipe-sysroot"
#CXX = "${WORKSPACE}/old-toolchain/arm/bin/arm-linux-gnueabi-g++ --sysroot=${WORKSPACE}/poky/build/tmp-glibc/work/armv7a-neon-vfpv4-oe-linux-gnueabi/${PN}/${PV}/recipe-sysroot"
#LD = "${WORKSPACE}/old-toolchain/arm/bin/arm-linux-gnueabi-ld"

do_configure:prepend () {
     export CFLAGS="${CFLAGS} -I${WORKSPACE}/poky/build/tmp-glibc/work/armv7a-neon-vfpv4-oe-linux-gnueabi/system-core/git/recipe-sysroot/usr/include/libunwindandroid -Wno-error -Wno-implicit-function-declaration"
     export CXXFLAGS="${CXXFLAGS} -I${WORKSPACE}/poky/build/tmp-glibc/work/armv7a-neon-vfpv4-oe-linux-gnueabi/system-core/git/recipe-sysroot/usr/include/libunwindandroid -Wno-error -Wno-implicit-function-declaration"
}

DEPENDS += "virtual/kernel openssl glib-2.0 libselinux ext4-utils libcutils libmincrypt libutils libbase libunwindandroid"
DEPENDS:append_qcs605 = " libsync"
DEPENDS:append_sdm845 = " libsync"

PROVIDES += "libsysutils logwrapper adb logd usb"

EXTRA_OECONF = " --with-host-os=${HOST_OS} --with-glib"
EXTRA_OECONF:append = " --with-sanitized-headers=${STAGING_KERNEL_BUILDDIR}/usr/include"
EXTRA_OECONF:append = " --with-logd-logging"
EXTRA_OECONF:append = " --disable-debuggerd"
EXTRA_OECONF:append = " ${@bb.utils.contains('DISTRO_FEATURES', 'nad-stream-update', '--enable-libfsmgr', '', d)}"

#Disable default libsync in system/core for 4.4 above kernels
EXTRA_OECONF:append = "${@oe.utils.version_less_or_equal('PREFERRED_VERSION_linux-msm', '4.4', '', ' --disable-libsync', d)}"

# Disable adb root privileges in USER builds for msm targets
EXTRA_OECONF:append_msm = "${@bb.utils.contains('VARIANT','user',' --disable-adb-root','',d)}"

CPPFLAGS += "-I${STAGING_INCDIR}/ext4_utils"
CPPFLAGS += "-I${STAGING_INCDIR}/libselinux"
CPPFLAGS += "-I${STAGING_INCDIR}/libunwind"
CFLAGS:append = " -D_GNU_SOURCE"

CPPFLAGS:append_apq8053 = " -DTARGET_IS_64_BIT"
CPPFLAGS:append_apq8017 = " -DTARGET_IS_64_BIT"
CPPFLAGS:append_apq8096 = " -DTARGET_IS_64_BIT"
CPPFLAGS:append_apq8098 = " -DTARGET_IS_64_BIT"
CPPFLAGS:remove_apq8053-32 = " -DTARGET_IS_64_BIT"

USBCOMPOSITION         ?= "9025"
USBCOMPOSITION_apq8009 = "9091"
USBCOMPOSITION_apq8053 = "901D"
USBCOMPOSITION_apq8096 = "901D"
USBCOMPOSITION_apq8098 = "901D"
USBCOMPOSITION_qcs605 = "901D"
USBCOMPOSITION_sdm845 = "901D"
USBCOMPOSITION_sdxnightjar = "90DB"
USBCOMPOSITION_sdxpoorwills = "90DB"
USBCOMPOSITION_sdxprairie = "90DB"
USBCOMPOSITION_sdmsteppe = "901D"
USBCOMPOSITION_sa2150p = "90FF"

do_install:append() {
   install -m 0755 ${S}/adb/launch_adbd -D ${D}${sysconfdir}/launch_adbd
   install -b -m 0644 /dev/null ${D}${sysconfdir}/adb_devid
   install -d ${D}${sysconfdir}/usb/
   install -d ${D}${base_sbindir}
   install -b -m 0644 /dev/null ${D}${sysconfdir}/usb/boot_hsusb_comp
   install -b -m 0644 /dev/null ${D}${sysconfdir}/usb/boot_hsic_comp
   echo ${USBCOMPOSITION} > ${D}${sysconfdir}/usb/boot_hsusb_comp
   install -m 0755 ${S}/usb/usb_composition -D ${D}${base_sbindir}/
   install -d ${D}${base_sbindir}/usb/compositions/
   install -m 0755 ${S}/usb/compositions/* -D ${D}${base_sbindir}/usb/compositions/
   install -m 0755 ${S}/usb/target -D ${D}${base_sbindir}/usb/
   install -d ${D}${base_sbindir}/usb/debuger/
   install -m 0755 ${S}/usb/debuger/debugFiles -D ${D}${base_sbindir}/usb/debuger/
   install -m 0755 ${S}/usb/debuger/help -D ${D}${base_sbindir}/usb/debuger/
   install -m 0755 ${S}/usb/debuger/usb_debug -D ${D}${base_sbindir}/
   install -b -m 0644 /dev/null -D ${D}${sysconfdir}/build.prop
   if ${@bb.utils.contains('DISTRO_FEATURES', 'systemd', 'true', 'false', d)}; then
      install -m 0750 ${S}/adb/start_adbd -D ${D}${sysconfdir}/initscripts/adbd
      install -m 0755 ${S}/usb/start_usb -D ${D}${sysconfdir}/initscripts/usb
      install -m 0750 ${S}/rootdir/etc/init.qcom.post_boot.sh -D ${D}${sysconfdir}/initscripts/init_post_boot
      #install -m 0750 ${S}/rootdir/etc/init.qti.debug.sh -D ${D}${sysconfdir}/initscripts/init_qti_debug
      install -d ${D}${systemd_unitdir}/system/
      install -d ${D}${systemd_unitdir}/system/multi-user.target.wants/
      install -d ${D}${systemd_unitdir}/system/ffbm.target.wants/
      install -m 0644 ${S}/adb/adbd.service -D ${D}${systemd_unitdir}/system/adbd.service
      ln -sf ${systemd_unitdir}/system/adbd.service ${D}${systemd_unitdir}/system/multi-user.target.wants/adbd.service
      ln -sf ${systemd_unitdir}/system/adbd.service ${D}${systemd_unitdir}/system/ffbm.target.wants/adbd.service
      install -m 0644 ${S}/usb/usb.service -D ${D}${systemd_unitdir}/system/usb.service
      ln -sf ${systemd_unitdir}/system/usb.service ${D}${systemd_unitdir}/system/multi-user.target.wants/usb.service
      ln -sf ${systemd_unitdir}/system/usb.service ${D}${systemd_unitdir}/system/ffbm.target.wants/usb.service
      if [ -f "${S}/usb/usbd.service" ]; then
      install -m 0644 ${S}/usb/usbd.service -D ${D}${systemd_unitdir}/system/usbd.service
      ln -sf ${systemd_unitdir}/system/usbd.service ${D}${systemd_unitdir}/system/multi-user.target.wants/usbd.service
      ln -sf ${systemd_unitdir}/system/usbd.service ${D}${systemd_unitdir}/system/ffbm.target.wants/usbd.service
      fi
      install -m 0644 ${S}/rootdir/etc/init_post_boot.service -D ${D}${systemd_unitdir}/system/init_post_boot.service
      ln -sf ${systemd_unitdir}/system/init_post_boot.service \
          ${D}${systemd_unitdir}/system/multi-user.target.wants/init_post_boot.service
      ln -sf ${systemd_unitdir}/system/init_post_boot.service \
          ${D}${systemd_unitdir}/system/ffbm.target.wants/init_post_boot.service
      install -m 0644 ${S}/debuggerd/init_debuggerd.service -D ${D}${systemd_unitdir}/system/init_debuggerd.service
      ln -sf ${systemd_unitdir}/system/init_debuggerd.service \
          ${D}${systemd_unitdir}/system/multi-user.target.wants/init_debuggerd.service
      ln -sf ${systemd_unitdir}/system/init_debuggerd.service \
          ${D}${systemd_unitdir}/system/ffbm.target.wants/init_debuggerd.service
      install -m 0644 ${S}/leproperties/leprop.service -D ${D}${systemd_unitdir}/system/leprop.service
      ln -sf ${systemd_unitdir}/system/leprop.service \
          ${D}${systemd_unitdir}/system/multi-user.target.wants/leprop.service
      ln -sf ${systemd_unitdir}/system/leprop.service \
          ${D}${systemd_unitdir}/system/ffbm.target.wants/leprop.service
   else
      install -m 0755 ${S}/adb/start_adbd -D ${D}${sysconfdir}/init.d/adbd
      install -m 0755 ${S}/usb/start_usb -D ${D}${sysconfdir}/init.d/usb
      install -m 0755 ${S}/rootdir/etc/init.qcom.post_boot.sh -D ${D}${sysconfdir}/init.d/init_post_boot
   fi
}

# Install logd specific files into rootfs
do_install:append() {
#   if ${@bb.utils.contains('DISTRO_FEATURES', 'systemd', 'true', 'false', d)}; then
#      install -m 0644 ${S}/logd/logd.path -D ${D}${systemd_unitdir}/system/logd.path
#      install -m 0644 ${S}/logd/earlyinit-logd.service -D ${D}${systemd_unitdir}/system/earlyinit-logd.service
#      install -m 0644 ${S}/logd/logd.service -D ${D}${systemd_unitdir}/system/logd.service
#
#      ln -sf ${systemd_unitdir}/system/logd.path ${D}${systemd_unitdir}/system/multi-user.target.wants/logd.path
#      ln -sf ${systemd_unitdir}/system/earlyinit-logd.service \
#             ${D}${systemd_unitdir}/system/multi-user.target.wants/earlyinit-logd.service
#
#      ln -sf ${systemd_unitdir}/system/logd.path ${D}${systemd_unitdir}/system/ffbm.target.wants/logd.path
#      ln -sf ${systemd_unitdir}/system/earlyinit-logd.service \
#             ${D}${systemd_unitdir}/system/ffbm.target.wants/earlyinit-logd.service
#   else
    install -m 0750 ${S}/logd/start_logd -D ${D}${sysconfdir}/init.d/logd
#   fi
}

do_install:append_mdm() {
   if ${@bb.utils.contains('DISTRO_FEATURES', 'systemd', 'true', 'false', d)}; then
      install -d ${D}${systemd_unitdir}/system/local-fs.target.wants/
      rm -rf ${D}${systemd_unitdir}/system/multi-user.target.wants/adbd.service
      rm -rf ${D}${systemd_unitdir}/system/multi-user.target.wants/usb.service
      rm -rf  ${D}${systemd_unitdir}/system/ffbm.target.wants/usbd.service
      ln -sf ${systemd_unitdir}/system/adbd.service ${D}${systemd_unitdir}/system/local-fs.target.wants/adbd.service
      ln -sf ${systemd_unitdir}/system/usb.service ${D}${systemd_unitdir}/system/local-fs.target.wants/usb.service
      ln -sf ${systemd_unitdir}/system/usbd.service ${D}${systemd_unitdir}/system/local-fs.target.wants/usbd.service
   fi
}

do_install:append_apq8009() {
   if ${@bb.utils.contains('DISTRO_FEATURES', 'systemd', 'false', 'true', d)}; then
    install -m 0755 ${S}/debuggerd/start_debuggerd -D ${D}${sysconfdir}/init.d/init_debuggerd
   else
    install -m 0755 ${S}/debuggerd/start_debuggerd -D ${D}${sysconfdir}/initscripts/init_debuggerd
   fi
}

#Install rules specific to apq8053 target
do_install:append_apq8053(){

  DEBUGGERD_NO_INSTALL=${@bb.utils.contains('USER_BUILD','1','no-debuggerd','',d)}
  if [ "x$DEBUGGERD_NO_INSTALL" == "x" ]; then
    if ${@bb.utils.contains('DISTRO_FEATURES', 'systemd', 'false', 'true', d)}; then
      install -m 0755 ${S}/debuggerd/start_debuggerd64 -D ${D}${sysconfdir}/init.d/init_debuggerd
    else
      install -m 0755 ${S}/debuggerd/start_debuggerd64 -D ${D}${sysconfdir}/initscripts/init_debuggerd
    fi
  fi
}

do_install:append_apq8096() {
  if ${@bb.utils.contains('DISTRO_FEATURES', 'systemd', 'false', 'true', d)}; then
    install -m 0755 ${S}/debuggerd/start_debuggerd64 -D ${D}${sysconfdir}/init.d/init_debuggerd
  else
    install -m 0755 ${S}/debuggerd/start_debuggerd64 -D ${D}${sysconfdir}/initscripts/init_debuggerd
  fi
}
do_install:append_apq8017(){
  if ${@bb.utils.contains('DISTRO_FEATURES', 'systemd', 'false', 'true', d)}; then
   install -m 0755 ${S}/debuggerd/start_debuggerd64 -D ${D}${sysconfdir}/init.d/init_debuggerd
  else
   install -m 0755 ${S}/debuggerd/start_debuggerd64 -D ${D}${sysconfdir}/initscripts/init_debuggerd
  fi
}

do_install:append_apq8098(){
  if ${@bb.utils.contains('DISTRO_FEATURES', 'systemd', 'false', 'true', d)}; then
   install -m 0755 ${S}/debuggerd/start_debuggerd64 -D ${D}${sysconfdir}/init.d/init_debuggerd
  else
   install -m 0755 ${S}/debuggerd/start_debuggerd64 -D ${D}${sysconfdir}/initscripts/init_debuggerd
  fi
}
INITSCRIPT_PACKAGES =+ "${PN}-usb"
INITSCRIPT_NAME_${PN}-usb = "usb"
INITSCRIPT_PARAMS_${PN}-usb = "start 30 2 3 4 5 ."
INITSCRIPT_PARAMS_${PN}-usb_mdm = "start 30 S ."

INITSCRIPT_PACKAGES =+ "${PN}-debuggerd"
INITSCRIPT_NAME_${PN}-debuggerd = "init_debuggerd"
INITSCRIPT_PARAMS_${PN}-debuggerd = "start 31 2 3 4 5 ."
INITSCRIPT_PARAMS_${PN}-debuggerd += "stop 38 6 ."

INITSCRIPT_PACKAGES =+ "${PN}-logd"
INITSCRIPT_NAME_${PN}-logd = "logd"
INITSCRIPT_PARAMS_${PN}-logd = "start 10  2 3 4 5 ."
INITSCRIPT_PARAMS_${PN}-logd += "stop 39  6 ."

INITSCRIPT_PACKAGES:remove_apq8053 = "${PN}-logd"
INITSCRIPT_PACKAGES:remove_apq8053-32 = "${PN}-logd"

INITSCRIPT_PACKAGES =+ "${PN}-post-boot"
INITSCRIPT_NAME_${PN}-post-boot = "init_post_boot"
INITSCRIPT_PARAMS_${PN}-post-boot = "start 90 2 3 4 5 ."

PACKAGES =+ "${PN}-adbd-dbg ${PN}-adbd ${PN}-adbd-dev"
FILES:${PN}-adbd-dbg = "${base_sbindir}/.debug/adbd ${libdir}/.debug/libadbd.*"
FILES:${PN}-adbd     = "${base_sbindir}/adbd ${sysconfdir}/init.d/adbd ${libdir}/libadbd.so.* /sbin/adbd"
FILES:${PN}-adbd    += "${systemd_unitdir}/system/adbd.service ${systemd_unitdir}/system/multi-user.target.wants/adbd.service ${systemd_unitdir}/system/local-fs.target.wants/adbd.service ${systemd_unitdir}/system/ffbm.target.wants/adbd.service ${sysconfdir}/launch_adbd ${sysconfdir}/initscripts/adbd ${sysconfdir}/adb_devid"
FILES:${PN}-adbd-dev = "${libdir}/libadbd.so ${libdir}/libadbd.la"

PACKAGES =+ "${PN}-usb-dbg ${PN}-usb"
FILES:${PN}-usb-dbg  = "${bindir}/.debug/usb_composition_switch ${bindir}/.debug/usbd"
FILES:${PN}-usb      = "${sysconfdir}/init.d/usb ${bindir}/usbd ${base_sbindir}/usb_composition ${bindir}/usb_composition_switch ${base_sbindir}/usb/compositions/*"
FILES:${PN}-usb     += "${sysconfdir}/usb/*"
FILES:${PN}-usb     += "${base_sbindir}/usb/* ${base_sbindir}/usb_debug ${base_sbindir}/usb/debuger/*"
FILES:${PN}-usb     += "${systemd_unitdir}/system/usb.service ${systemd_unitdir}/system/multi-user.target.wants/usb.service ${systemd_unitdir}/system/local-fs.target.wants/usb.service ${systemd_unitdir}/system/ffbm.target.wants/usb.service ${sysconfdir}/initscripts/usb ${systemd_unitdir}/system/usbd.service ${systemd_unitdir}/system/multi-user.target.wants/usbd.service ${systemd_unitdir}/system/local-fs.target.wants/usbd.service ${systemd_unitdir}/system/ffbm.target.wants/usbd.service"

PACKAGES =+ "${PN}-post-boot"
FILES:${PN}-post-boot  = "${sysconfdir}/init.d/init_post_boot"
FILES:${PN}-post-boot += "${systemd_unitdir}/system/init_post_boot.service ${systemd_unitdir}/system/multi-user.target.wants/init_post_boot.service ${systemd_unitdir}/system/ffbm.target.wants/init_post_boot.service ${sysconfdir}/initscripts/init_post_boot ${sysconfdir}/initscripts/init_qti_debug"
INSANE_SKIP:${PN}-post-boot = "file-rdeps"

PACKAGES =+ "${PN}-logd-dbg ${PN}-logd"
FILES:${PN}-logd-dbg  = "${base_sbindir}/.debug/logd"
FILES:${PN}-logd      = "${sysconfdir}/init.d/logd ${base_sbindir}/logd /sbin/logd"
FILES:${PN}-logd     += "${systemd_unitdir}/system/earlyinit-logd.service ${systemd_unitdir}/system/logd.path ${systemd_unitdir}/system/logd.service"
FILES:${PN}-logd     += "${systemd_unitdir}/system/multi-user.target.wants/earlyinit-logd.service ${systemd_unitdir}/system/multi-user.target.wants/logd.path"
FILES:${PN}-logd     += "${systemd_unitdir}/system/ffbm.target.wants/earlyinit-logd.service ${systemd_unitdir}/system/ffbm.target.wants/logd.path"

PACKAGES =+ "${PN}-debuggerd-dbg ${PN}-debuggerd"
FILES:${PN}-debuggerd-dbg  = "${base_sbindir}/.debug/debuggerd ${base_sbindir}/.debug/debuggerd64 "
FILES:${PN}-debuggerd      = "${sysconfdir}/init.d/init_debuggerd ${sysconfdir}/initscripts/init_debuggerd ${base_sbindir}/debuggerd ${base_sbindir}/debuggerd64"
FILES:${PN}-debuggerd     += "${systemd_unitdir}/system/init_debuggerd.service ${systemd_unitdir}/system/multi-user.target.wants/init_debuggerd.service ${systemd_unitdir}/system/ffbm.target.wants/init_debuggerd.service"

PACKAGES =+ "${PN}-leprop-dbg ${PN}-leprop"
FILES:${PN}-leprop-dbg  = "${base_sbindir}/.debug/leprop-service ${bindir}/.debug/getprop ${bindir}/.debug/setprop"
FILES:${PN}-leprop      = "${base_sbindir}/leprop-service ${bindir}/getprop ${bindir}/setprop ${sysconfdir}/proptrigger.sh ${sysconfdir}/proptrigger.conf /sbin/leprop-service"
FILES:${PN}-leprop     += "${systemd_unitdir}/system/leprop.service ${systemd_unitdir}/system/multi-user.target.wants/leprop.service ${systemd_unitdir}/system/ffbm.target.wants/leprop.service ${sysconfdir}/build.prop"

FILES:${PN}-dbg  = "${bindir}/.debug/* ${libdir}/.debug/*"
FILES:${PN}      = "${bindir}/* ${libdir}/pkgconfig/* ${libdir}/*.so.* "
FILES:${PN}-dev  = "${libdir}/*.so ${libdir}/*.la ${includedir}*"

INSANE_SKIP:${PN} += " usrmerge"
INSANE_SKIP:${PN}-adbd += " usrmerge"
INSANE_SKIP:${PN}-leprop += " usrmerge"
INSANE_SKIP:${PN}-logd += " usrmerge"
INSANE_SKIP:${PN}-leprop-dbg += " usrmerge"
