inherit autotools pkgconfig

DESCRIPTION = "Bluetooth OBEX"
LICENSE = "BSD-3-Clause"
HOMEPAGE = "https://www.codeaurora.org/"
LIC_FILES_CHKSUM = "file://${COREBASE}/meta/files/common-licenses/\
${LICENSE};md5=550794465ba0ec5312d6919e203a55f9"

DEPENDS += "glib-2.0 btvendorhal"

CFLAGS:append = " -DUSE_ANDROID_LOGGING "
LDFLAGS:append = " -llog "

FILESPATH =+ "${WORKSPACE}:"
SRC_URI = "file://qcom-opensource/bt/obex_profiles"

S = "${UNPACKDIR}/qcom-opensource/bt/obex_profiles"

EXTRA_OECONF = "--with-glib"
EXTRA_OECONF += "--with-common-includes=${STAGING_INCDIR}"
