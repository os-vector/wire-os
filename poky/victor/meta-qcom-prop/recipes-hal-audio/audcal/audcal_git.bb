inherit qcommon qprebuilt qlicense pkgconfig autotools

DESCRIPTION = "Audio Calibration Library"
PR = "r11"

TARGETNAME = "${@d.getVar('BASEMACHINE', True).replace('mdm','').replace('apq','')}"
SRC_DIR = "${WORKSPACE}/audio/mm-audio/audcal/"

S = "${UNPACKDIR}/audio/mm-audio/audcal"

DEPENDS = "glib-2.0 diag common acdbmapper"

EXTRA_OECONF += "--with-sanitized-headers=${STAGING_KERNEL_BUILDDIR}/usr/include \
                 --with-glib \
                 --enable-target=${BASEMACHINE}"

do_install:append() {
    if [ -d ${S}/family-b/acdbdata/${TARGETNAME}/MTP/ ];then
        mkdir -p ${D}${sysconfdir}/
        install -m 0755 ${S}/family-b/acdbdata/${TARGETNAME}/MTP/*  -D ${D}${sysconfdir}
    fi
}