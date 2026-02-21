# Additional non-open source packages to be put to the image filesystem.
include ${BASEMACHINE}/${BASEMACHINE}-robot-qti-image.inc

# BRC: Set systemd default target to run victor processes
#SYSTEMD_DEFAULT_TARGET="anki-robot.target"

inherit qimage

require internal-image.inc
# Set up for handling the generation of the /usr image
# partition...
require mdm-usr-image.inc

# Set up for handling the generation of the /cache image
# partition...
require mdm-cache-image.inc

# Set up for handling the generation of the /persist image
# partition only for APQ Targets
require apq-persist-image.inc

do_rootfs[nostamp] = "1"
do_build[nostamp]  = "1"

python __anonymous () {
        image = d.getVar('INITRAMFS_IMAGE', True)
        if image:
            d.appendVarFlag('do_makesystem', 'depends', ' ${INITRAMFS_IMAGE}:do_rootfs')
}

# Call function makesystem to generate sparse ext4 image
#addtask makesystem after do_rootfs before do_build

KEYDIR := "${THISDIR}/kernel"
TARGET_SHA_TYPE = "sha256"

do_makesystem:append() {
    #Generating signed boot.img
    cd ${DEPLOY_DIR_IMAGE}
    cp ${MACHINE}-boot.img ${MACHINE}-boot.img.nonsecure

    echo "KEYDIR=${KEYDIR}"
    echo "ls ${KEYDIR}" && ls ${KEYDIR}

    openssl dgst -${TARGET_SHA_TYPE} -binary ${MACHINE}-boot.img.nonsecure > ${MACHINE}-boot.img.${TARGET_SHA_TYPE}
    if ${@bb.utils.contains('DISTRO_FEATURES', 'vble', 'true', 'false', d)}; then
        openssl pkeyutl -sign -in ${MACHINE}-boot.img.${TARGET_SHA_TYPE} -inkey ${KEYDIR}/vble-qti.key -out ${MACHINE}-boot.img.sig -pkeyopt digest:${TARGET_SHA_TYPE} -pkeyopt rsa_padding_mode:pkcs1
    else
        openssl rsautl -sign -in ${MACHINE}-boot.img.${TARGET_SHA_TYPE} -inkey ${KEYDIR}/qti.key -out ${MACHINE}-boot.img.sig
    fi
    dd if=/dev/zero of=${MACHINE}-boot.img.sig.padded bs=2048 count=1
    dd if=${MACHINE}-boot.img.sig of=${MACHINE}-boot.img.sig.padded conv=notrunc
    cat ${MACHINE}-boot.img.nonsecure ${MACHINE}-boot.img.sig.padded > ${MACHINE}-boot.img.secure
    rm -rf ${MACHINE}-boot.img.${TARGET_SHA_TYPE} ${MACHINE}-boot.img.sig ${MACHINE}-boot.img.sig.padded
    #mv -f ${MACHINE}-boot.img.secure ${MACHINE}-boot.img
}
