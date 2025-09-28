inherit qcommon qlicense qprebuilt

DESCRIPTION = "acdb mapper Library"
PR = "r2"
DEPENDS = "glib-2.0 audioalsa"

SRC_DIR = "${WORKSPACE}/audio/mm-audio/audio-acdb-util/acdb-mapper/"
S = "${UNPACKDIR}/audio/mm-audio/audio-acdb-util/acdb-mapper"

EXTRA_OECONF += "--with-sanitized-headers=${STAGING_KERNEL_HEADERS} \
                 --with-glib"
