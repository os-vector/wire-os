FILESEXTRAPATHS:prepend := "${THISDIR}/${PN}:"

do_compile:append:class-native() {
    echo "building e2fsdroid (android contrib tool)"

    ${CC} \
        ${CFLAGS} \
        ${LDFLAGS} \
        ../sources/e2fsprogs-${PV}/contrib/android/e2fsdroid.c \
        -I. \
        -Ilib \
        -Ilib/ext2fs \
        -o e2fsdroid \
        lib/ext2fs/libext2fs.a \
        lib/e2p/libe2p.a \
        lib/libuuid/libuuid.a \
        lib/libcom_err/libcom_err.a
}

do_install:append:class-native() {
    install -d ${D}${bindir}
    install -m 0755 e2fsdroid ${D}${bindir}/e2fsdroid
}
