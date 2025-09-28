inherit autotools qcommon qprebuilt qlicense

DESCRIPTION = "rmt_storage server module"
PR = "r11"

FILESPATH =+ "${WORKSPACE}:"
SRC_URI  = "file://remotefs/"
SRC_URI += "file://rmt_storage.sh"
SRC_URI += "file://rmt_storage.service"

SRC_DIR = "${WORKSPACE}/remotefs"

S = "${UNPACKDIR}/remotefs"

DEPENDS += "glib-2.0 qmi qmi-framework virtual/kernel libcutils"

EXTRA_OECONF:append = " --with-glib"
EXTRA_OECONF:append = " --with-sanitized-headers=${STAGING_KERNEL_BUILDDIR}/usr/include"

CFLAGS += "-I${STAGING_INCDIR}/cutils"

do_install() {
   install -d ${D}/usr/sbin
   install -m 0755 ${S}/rmt_storage -D ${D}/usr/sbin/rmt_storage
   if ${@bb.utils.contains('DISTRO_FEATURES', 'systemd', 'true', 'false', d)}; then
       install -d ${D}${systemd_unitdir}/system/
       install -m 0644 ${UNPACKDIR}/rmt_storage.service -D ${D}${systemd_unitdir}/system/rmt_storage.service
       install -d ${D}${systemd_unitdir}/system/multi-user.target.wants/
       install -d ${D}${systemd_unitdir}/system/ffbm.target.wants/
       # enable the service for multi-user.target
       ln -sf ${systemd_unitdir}/system/rmt_storage.service \
            ${D}${systemd_unitdir}/system/multi-user.target.wants/rmt_storage.service
       ln -sf ${systemd_unitdir}/system/rmt_storage.service \
            ${D}${systemd_unitdir}/system/ffbm.target.wants/rmt_storage.service
   fi
}

FILES:${PN} += "${systemd_unitdir}/system/ \
                usr/sbin/rmt_storage"
