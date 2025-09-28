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
IMAGE_INSTALL += "libcutils"
IMAGE_INSTALL += "liblog"

IMAGE_INSTALL += "libxml2"

IMAGE_INSTALL += "start-scripts-find-partitions"
IMAGE_INSTALL += "start-scripts-firmware-links"
IMAGE_INSTALL += "${@oe.utils.conditional('USER_BUILD', '1', '', 'system-core-adbd', d)}"
IMAGE_INSTALL += "system-core-logd"
IMAGE_INSTALL += "system-core-leprop"
IMAGE_INSTALL += "system-core-usb"
IMAGE_INSTALL += "system-core-post-boot"
IMAGE_INSTALL += "system-conf"
IMAGE_INSTALL += "system-prop"
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
IMAGE_INSTALL += "wcnss"
IMAGE_INSTALL += "wlan-opensource"


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
    which make_ext4fs

    RAW_FILE=${DEPLOY_DIR_IMAGE}/${MACHINE}-raw-sysfs.ext4
    SPARSE_FILE=${DEPLOY_DIR_IMAGE}/${MACHINE}-sysfs.ext4
    VERITYSETUP_OUTPUT=${DEPLOY_DIR_IMAGE}/${MACHINE}-veritysetup.txt

    BLOCK_SIZE=4096
    DATA_BLOCKS=$( expr ${SYSTEM_SIZE_EXT4} / $BLOCK_SIZE )

    cp ${CONFIG_FILE} ./final_user_config
    if [[ -e ${VICTOR_CONFIG_FILE} ]]; then
       echo "Appending contents of ${VICTOR_CONFIG_FILE} to our final canned fs config"
       cat ${VICTOR_CONFIG_FILE} >> ./final_user_config
    fi

    echo "making the sparse ext4 file system"
    make_ext4fs ${IMAGE_EXT4_EXTRA_OPTIONS} -a / -C ./final_user_config ${IMAGE_EXT4_SELINUX_OPTIONS} -l ${SYSTEM_SIZE_EXT4} ${RAW_FILE} ${IMAGE_ROOTFS}
    make_ext4fs -l ${SYSTEMRW_SIZE_EXT4} ${DEPLOY_DIR_IMAGE}/${MACHINE}-systemrw.ext4

    veritysetup --data-block-size=${BLOCK_SIZE} --data-blocks=${DATA_BLOCKS} --hash-offset=${SYSTEM_SIZE_EXT4} format ${RAW_FILE} ${RAW_FILE} >${VERITYSETUP_OUTPUT}
    #img2simg ${RAW_FILE} ${SPARSE_FILE}


    echo "=== Generate Boot Image containing Kernel, DTB, initramfs etc... ===="

    mkdir -p ${DEPLOY_DIR_IMAGE}

    VERITYSETUP_OUTPUT=${DEPLOY_DIR_IMAGE}/apq8009-robot-veritysetup.txt

    HASH_TYPE=$(sed -n -e '/Hash type:/ s/.*:\s*// p' $VERITYSETUP_OUTPUT)
    HASH_ALGORITHM=$(sed -n -e '/Hash algorithm:/ s/.*:\s*// p' $VERITYSETUP_OUTPUT)
    DATA_BLOCKS=$(sed -n -e '/Data blocks:/ s/.*:\s*// p' $VERITYSETUP_OUTPUT)
    DATA_BLOCK_SIZE=$(sed -n -e '/Data block size:/ s/.*:\s*// p' $VERITYSETUP_OUTPUT | cut -d' ' -f1)
    #DATA_BLOCK_SIZE=$(sed -n -e '/Data block size:/ s/.*:\s*// p' $VERITYSETUP_OUTPUT)
    #HASH_BLOCK_SIZE=$(sed -n -e '/Hash block size:/ s/.*:\s*// p' $VERITYSETUP_OUTPUT)
    HASH_BLOCK_SIZE=$(sed -n -e '/Hash block size:/ s/.*:\s*\([0-9]*\).*/\1/p' $VERITYSETUP_OUTPUT)
    SALT=$(sed -n -e '/Salt:/ s/.*:\s*// p' $VERITYSETUP_OUTPUT)
    ROOT_HASH=$(sed -n -e '/Root hash:/ s/.*:\s*// p' $VERITYSETUP_OUTPUT)

    #FS_SIZE_IN_512_BLOCKS=$(expr \( $DATA_BLOCKS \* $DATA_BLOCK_SIZE / 512 \) )
    FS_SIZE_IN_512_BLOCKS=$(expr "$DATA_BLOCKS" \* "$DATA_BLOCK_SIZE" / 512)
    #FS_SIZE_IN_512_BLOCKS=$(( DATA_BLOCKS * DATA_BLOCK_SIZE / 512 ))
    #HASH_OFFSET=$(( $DATA_BLOCKS + 1 ))
    HASH_OFFSET=$(expr "$DATA_BLOCKS" + 1)

    ## Where does the magic number `179` below come from?
    ## -> see kernel/msm-3.18/Documentation/devices.txt
    ## -> All MMC block devices get the major device number 179
    ## -> The minor number corresponds to the partition number (counting from 1)
    ## -> We assume the boot partition is always on the first MMC block device
 
    if [ "${FACTORY}" == "1" ] || [ "${USER_BUILD}" == "1" ]; then
        # Enable dm-verity for any FACTORY build or if we are making a USER build
        KERNEL_CMD_PARAMS="${@ d.getVar("KERNEL_CMD_PARAMS") } dm-verity.dev_wait=1 dm=\"system none ro,0 $FS_SIZE_IN_512_BLOCKS verity $HASH_TYPE 179:##SYSTEM_BOOT_PART## 179:##SYSTEM_BOOT_PART## $DATA_BLOCK_SIZE $HASH_BLOCK_SIZE $DATA_BLOCKS $HASH_OFFSET $HASH_ALGORITHM $ROOT_HASH $SALT\" root=/dev/dm-0"
    else
        # Disable dm-verity for Developer builds
        KERNEL_CMD_PARAMS="${@ d.getVar("KERNEL_CMD_PARAMS") }"
    fi

    RAMDISK="/dev/null"
    if [ ! -z "${INITRAMFS_IMAGE}" ]; then
	    RAMDISK="${DEPLOY_DIR_IMAGE}/${INITRAMFS_IMAGE}-${MACHINE}.rootfs.cpio.gz"
    fi

    ## Save kernel cmd line for debugging purposes
    echo "KERNEL_CMD_PARAMS=$KERNEL_CMD_PARAMS" > ${DEPLOY_DIR_IMAGE}/kernel_params.txt


    ## NOTE: don't use ${KERNEL_CMD_PARAMS} below - it will be expanded to the same what ${@ d.getVar("KERNEL_CMD_PARAMS") } returns
    ##       we are modifying a local shell variable here, that can only be dereferenced by using $KERNEL_CMD_PARAMS
    ##       c.f. https://stackoverflow.com/questions/48213497/how-do-i-update-a-datastore-variable-from-inside-a-bash-variable
    # Make bootimage
    ${STAGING_BINDIR_NATIVE}/mkbootimg --kernel ${DEPLOY_DIR_IMAGE}/${KERNEL_IMAGETYPE}-${MACHINE}.bin \
        --ramdisk "$RAMDISK" \
        --cmdline "$KERNEL_CMD_PARAMS" \
        --pagesize ${PAGE_SIZE} \
        --base ${KERNEL_BASE} \
        --ramdisk_offset 0x0 \
        ${extra_mkbootimg_params} --output ${DEPLOY_DIR_IMAGE}/${MACHINE}-boot.img
}
