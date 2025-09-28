inherit qcommon qprebuilt qlicense

DESCRIPTION = "acdb rtac Library"
PR = "r3"

DEPENDS = "glib-2.0 audioalsa acdbmapper audcal"

CFLAGS += "-Wno-implicit-function-declaration -Wno-incompatible-pointer-types"
CXXFLAGS += "-Wno-implicit-function-declaration -Wno-incompatible-pointer-types"

SRC_DIR = "${WORKSPACE}/audio/mm-audio/audio-acdb-util/acdb-rtac/"
S = "${UNPACKDIR}/audio/mm-audio/audio-acdb-util/acdb-rtac"

EXTRA_OECONF += "--with-sanitized-headers=${STAGING_KERNEL_BUILDDIR}/usr/include \
                 --with-glib \
                 --enable-target=${BASEMACHINE}"
