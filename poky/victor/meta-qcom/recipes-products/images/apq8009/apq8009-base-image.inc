# Rootfs creation.
#DEPENDS += "lk"

# Formats of root filesystem images.
IMAGE_FSTYPES += "ext4"

IMAGE_INSTALL += "packagegroup-core-boot"

IMAGE_INSTALL += "run-postinsts"
IMAGE_INSTALL += "${CORE_IMAGE_EXTRA_INSTALL}"

# utils and scripts needed for SELinux
IMAGE_INSTALL += "${@bb.utils.contains('DISTRO_FEATURES','selinux','packagegroup-core-selinux','',d)}"
IMAGE_INSTALL += "${@bb.utils.contains('DISTRO_FEATURES','selinux','selinux-config','',d)}"
IMAGE_INSTALL += "${@bb.utils.contains('DISTRO_FEATURES','selinux','refpolicy-mls','',d)}"
IMAGE_INSTALL += "${@bb.utils.contains('DISTRO_FEATURES','selinux','start-scripts-misc-daemon','',d)}"

# The MSM Linux minimal boot image files.


IMAGE_INSTALL += "base-files"
IMAGE_INSTALL += "base-passwd"

IMAGE_INSTALL += "glib-2.0"

IMAGE_INSTALL += "libnl"
#IMAGE_INSTALL += "libcutils"
#IMAGE_INSTALL += "liblog"

IMAGE_INSTALL += "libxml2"

#IMAGE_INSTALL += "start-scripts-find-partitions"
#IMAGE_INSTALL += "start-scripts-firmware-links"
#IMAGE_INSTALL += "${@oe.utils.conditional('USER_BUILD', '1', '', 'system-core-adbd', d)}"
#IMAGE_INSTALL += "system-core-logd"
#IMAGE_INSTALL += "system-core-leprop"
#IMAGE_INSTALL += "system-core-usb"
#IMAGE_INSTALL += "system-core-post-boot"
#IMAGE_INSTALL += "system-conf"
#IMAGE_INSTALL += "system-prop"
IMAGE_INSTALL += "iw"
IMAGE_INSTALL += "volatile-binds"

# data
IMAGE_INSTALL += "iproute2"
IMAGE_INSTALL += "iptables"
#IMAGE_INSTALL += "dnsmasq"
IMAGE_INSTALL += "ebtables"
IMAGE_INSTALL += "ethtool"
#IMAGE_INSTALL += "dhcpcd"
IMAGE_INSTALL += "tcpdump"

# drones
IMAGE_INSTALL += "nettle"
IMAGE_INSTALL += "gnutls"
IMAGE_INSTALL += "curl"
IMAGE_INSTALL += "libwebp"

# wlan
#IMAGE_INSTALL += "wcnss"
#IMAGE_INSTALL += "wlan-opensource"


#IMAGE_PREPROCESS_COMMAND += "echo ${MACHINE} >> ${IMAGE_ROOTFS}/target;"

# Don't install GDB and related pacakges ever.
#DEPENDS += "${@oe.utils.conditional('USER_BUILD', '1', '', 'gdb-cross-arm', d)}"
#IMAGE_INSTALL += "${@oe.utils.conditional('USER_BUILD', '1', '', 'gdb', d)}"
#IMAGE_INSTALL += "${@oe.utils.conditional('USER_BUILD', '1', '', 'gdbserver', d)}"

# Below is to generate sparse ext4 system image (OE by default supports raw ext4 images)
SELINUX_FILE_CONTEXTS = "${IMAGE_ROOTFS}/etc/selinux/mls/contexts/files/file_contexts"
IMAGE_EXT4_SELINUX_OPTIONS = "${@bb.utils.contains('DISTRO_FEATURES','selinux',' -S ${SELINUX_FILE_CONTEXTS} ','',d)}"

DEPENDS += "cryptsetup-native"

CONFIG_FILE = "${COREBASE}/../victor/meta-qcom/files/config_files/anki_user_config"
VICTOR_CONFIG_FILE = "${DEPLOY_DIR_IMAGE}/victor_canned_fs_config"

IMAGE_ROOTFS_EXTRA_SPACE = "307200"

addtask makesystem after do_rootfs
#addtask makesystem
do_image_ext4[depends] += "${PN}:do_makesystem"

do_makesystem() {
    echo "================ Creating system ================="
    echo "IMAGE_ROOTFS=" ${IMAGE_ROOTFS}
    if [[ "${DISTRO_FEATURES}" =~ "selinux" ]]; then
        echo "SELINUX_FILE_CONTEXTS=" ${SELINUX_FILE_CONTEXTS}
    fi
    echo "SYSTEM_SIZE_EXT4="  ${SYSTEM_SIZE_EXT4}
    echo "DEPLOY_DIR_IMAGE=" ${DEPLOY_DIR_IMAGE}
    echo "MACHINE=" ${MACHINE}
    echo "SYSTEMRW_SIZE_EXT4=" ${SYSTEMRW_SIZE_EXT4}
    echo "CONFIG_FILE=" ${CONFIG_FILE}
    echo "VICTOR_CONFIG_FILE=" ${VICTOR_CONFIG_FILE}

    RAW_FILE=${DEPLOY_DIR_IMAGE}/${MACHINE}-system.ext4
    #RW_FILE=${DEPLOY_DIR_IMAGE}/${MACHINE}-systemrw.ext4

    BLOCK_SIZE=4096
    BLOCKS=$(expr ${SYSTEM_SIZE_EXT4} / ${BLOCK_SIZE})

    mkdir -p ${DEPLOY_DIR_IMAGE}

    cp ${CONFIG_FILE} ./final_user_config
    if [[ -e ${VICTOR_CONFIG_FILE} ]]; then
        echo "Appending contents of ${VICTOR_CONFIG_FILE} to our final canned fs config"
        cat ${VICTOR_CONFIG_FILE} >> ./final_user_config
    fi

    echo "creating empty ext4 filesystem"

    mke2fs \
        -t ext4 \
        -b ${BLOCK_SIZE} \
        -L system \
        -O ^has_journal \
        -E lazy_itable_init=0,lazy_journal_init=0 \
        ${RAW_FILE} \
        ${BLOCKS}

    e2fsdroid \
        -f ${IMAGE_ROOTFS} \
        -C ./final_user_config \
        -a / \
        ${RAW_FILE}

    echo "=== Generate Boot Image containing Kernel, DTB, initramfs etc... ===="

    KERNEL_CMD_PARAMS="${@ d.getVar("KERNEL_CMD_PARAMS") }"

    RAMDISK="/dev/null"

    echo "KERNEL_CMD_PARAMS=$KERNEL_CMD_PARAMS" > ${DEPLOY_DIR_IMAGE}/kernel_params.txt

    ## NOTE: don't use ${KERNEL_CMD_PARAMS} below - it will be expanded to whatever
    ##       ${@ d.getVar("KERNEL_CMD_PARAMS") } returns
    ##       we are modifying a local shell variable here
    ##       see: https://stackoverflow.com/questions/48213497/how-do-i-update-a-datastore-variable-from-inside-a-bash-variable

    cat ${DEPLOY_DIR_IMAGE}/${KERNEL_IMAGETYPE}-${MACHINE}.bin ${DEPLOY_DIR_IMAGE}/qcom-apq8009-anki-vector.dtb > ${DEPLOY_DIR_IMAGE}/${KERNEL_IMAGETYPE}-dtb-${MACHINE}.bin

    ${STAGING_BINDIR_NATIVE}/mkbootimg \
        --kernel ${DEPLOY_DIR_IMAGE}/${KERNEL_IMAGETYPE}-dtb-${MACHINE}.bin \
        --ramdisk "$RAMDISK" \
        --cmdline "$KERNEL_CMD_PARAMS" \
        --pagesize ${PAGE_SIZE} \
        --base ${KERNEL_BASE} \
        --ramdisk_offset 0x0 \
        ${extra_mkbootimg_params} \
        --output ${DEPLOY_DIR_IMAGE}/${MACHINE}-boot.img
}

