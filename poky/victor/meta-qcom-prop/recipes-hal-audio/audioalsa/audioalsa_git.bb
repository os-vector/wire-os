inherit qcommon qlicense qprebuilt

DESCRIPTION = "libaudioalsa Library"
PR = "r5"

DEPENDS = "glib-2.0 virtual/kernel"

SRC_DIR = "${WORKSPACE}/audio/mm-audio/audio-alsa/"
S = "${UNPACKDIR}/audio/mm-audio/audio-alsa"

EXTRA_OECONF += "\
                 --with-sanitized-headers=${STAGING_KERNEL_BUILDDIR}/usr/include \
                 --with-glib"
