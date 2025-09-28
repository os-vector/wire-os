inherit qcommon qlicense qprebuilt

DESCRIPTION = "acdb-fts Library"
PR = "r0"

SRC_DIR = "${WORKSPACE}/audio/mm-audio/audio-acdb-util/acdb-fts/"
S = "${UNPACKDIR}/audio/mm-audio/audio-acdb-util/acdb-fts"

DEPENDS = "audcal"

do_install:append () {
  cp ${D}/${libdir}/libacdb_fts.so ${D}/${libdir}/libacdb-fts.so
}

FILES:${PN}-dbg  = "${libdir}/.debug/*"
FILES:${PN}      = "${libdir}/*.so ${libdir}/*.so.* ${sysconfdir}/* ${libdir}/pkgconfig/*"
FILES:${PN}-dev  = "${libdir}/*.la ${includedir}"
