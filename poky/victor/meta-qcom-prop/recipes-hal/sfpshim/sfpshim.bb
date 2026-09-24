SUMMARY = "janky softfp call wrappers for float imports of blobs"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

SRC_URI = "file://sfpshim.c \
           file://sfpshim_cxx.cpp \
           file://shimcsu.c \
           file://rename.map \
           file://float-syms.txt \
          "

S = "${UNPACKDIR}"

do_compile() {
    ${CC} ${CFLAGS} -fPIC -c ${S}/sfpshim.c -o sfpshim.o
    ${CXX} ${CXXFLAGS} -fPIC -c ${S}/sfpshim_cxx.cpp -o sfpshim_cxx.o
    ${CXX} ${LDFLAGS} -shared -Wl,-soname,libsfpshim.so -o libsfpshim.so sfpshim.o sfpshim_cxx.o -lm
    ${CC} ${CFLAGS} ${LDFLAGS} -fPIC -shared -Wl,-soname,libshimcsu.so -o libshimcsu.so ${S}/shimcsu.c
}

do_install() {
    install -d ${D}${libdir} ${D}${datadir}/sfpshim
    install -m 0755 libsfpshim.so libshimcsu.so ${D}${libdir}/
    install -m 0644 ${S}/rename.map ${S}/float-syms.txt ${D}${datadir}/sfpshim/
}

SYSROOT_DIRS += "${datadir}/sfpshim"

FILES_SOLIBSDEV = ""
FILES:${PN} += "${libdir}/libsfpshim.so ${libdir}/libshimcsu.so"
FILES:${PN}-dev += "${datadir}/sfpshim"
