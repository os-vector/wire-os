inherit qcommon qlicense qprebuilt

DESCRIPTION = "Qualcomm Data DSutils Module"
PR = "r7"

CFLAGS += "-Wno-error -Wno-incompatible-pointer-types -Wno-implicit-function-declaration"
LDFLAGS += "-Wl,--allow-multiple-definition"

DEPENDS = "common diag glib-2.0 virtual/kernel libcutils"

EXTRA_OECONF = "--with-lib-path=${STAGING_LIBDIR} \
                --with-common-includes=${STAGING_INCDIR} \
                --with-glib \
                --with-qxdm \
                --with-sanitized-headers=${STAGING_KERNEL_BUILDDIR}/usr/include \
                --enable-static=no \
                --with-sysroot=${STAGING_LIBDIR}/../.. \
                --enable-target=${BASEMACHINE}"

CFLAGS += "-I${STAGING_INCDIR}/cutils"

S       = "${UNPACKDIR}/data/dsutils"
SRC_DIR = "${WORKSPACE}/data/dsutils"
