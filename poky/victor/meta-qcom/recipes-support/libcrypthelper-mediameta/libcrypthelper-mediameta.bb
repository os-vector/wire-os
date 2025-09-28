inherit autotools pkgconfig

DESCRIPTION = "Build crypthelper-mediameta, a helper library\
               to provide mapping between encryption meta and\
               encryptable block devices"

LICENSE = "BSD-3-Clause"
LIC_FILES_CHKSUM = "file://${COREBASE}/meta/files/common-licenses/\
${LICENSE};md5=550794465ba0ec5312d6919e203a55f9"

PR = "r0"

SRC_URI   = "file://crypthelper-mediameta"
SRC_URI  += "file://media-encryption.conf"

S = "${UNPACKDIR}/crypthelper-mediameta"

CFLAGS += "-I${S}/libs"

do_install:append() {
    install -m 0644 ${WORKDIR}/media-encryption.conf -D ${D}/${sysconfdir}/conf/media-encryption.conf
}

do_install:append_sdm845 () {
    sed -i "s/footer/fdemeta/g" ${D}/${sysconfdir}/conf/media-encryption.conf
}

PACKAGES =+ "${PN}-lib"
FILES:${PN}-lib   =  "${sysconfdir}/conf/*"
FILES:${PN}-lib  +=  "${libdir}/libcrypthelper_mediameta.so.*  ${libdir}/pkgconfig/*"
