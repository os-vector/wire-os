SUMMARY = "lk2nd bootloader for msm8909"
LICENSE = "Anki-Inc.-Proprietary"
LIC_FILES_CHKSUM = "file://${COREBASE}/../victor/meta-qcom/files/anki-licenses/\
Anki-Inc.-Proprietary;md5=4b03b8ffef1b70b13d869dbce43e8f09"

FILESPATH =+ "${WORKSPACE}:"
SRC_URI = "file://lk2nd/"

S = "${UNPACKDIR}/lk2nd"

DEPENDS += " \
    dtc-native \
    tar-native \
    python3-dtc-native \
"

inherit deploy

do_compile() {
    echo ${STAGING_LIBDIR}
    make \
        LD_LIBRARY_PATH="${LD_LIBRARY_PATH}:${STAGING_LIBDIR_NATIVE}" \
	LDFLAGS="--no-warn-mismatch -gc-sections" \
        LIBGCC="${STAGING_LIBDIR}/arm-oe-linux-gnueabihf/15.2.0/libgcc.a" \
        TOOLCHAIN_PREFIX=arm-oe-linux-gnueabi- \
        lk2nd-msm8909
}

do_deploy() {
    install -d ${DEPLOYDIR}
    install -m 0644 \
        ${S}/build-lk2nd-msm8909/lk2nd.img \
        ${DEPLOYDIR}/lk2nd-msm8909.img
}

addtask deploy after do_compile
