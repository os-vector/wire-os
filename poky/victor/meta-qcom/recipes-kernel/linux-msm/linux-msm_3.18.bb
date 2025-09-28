require recipes-kernel/linux-msm/linux-msm.inc

COMPATIBLE_MACHINE = "(mdm9607|mdm9650|apq8009|apq8096|apq8053|apq8017|msm8909w|sdx20|apq8009-robot)"

#KERNEL_DEVICETREE = "qcom/msm8909-anki.dtb"

KERNEL_IMAGEDEST_apq8096 = "boot"

SRC_DIR   =  "${WORKSPACE}/kernel/msm-3.18"
S         =  "${UNPACKDIR}/kernel/msm-3.18"
PR = "r6"

SRC_URI += "file://defconfig"

DEPENDS:apq8096 += "dtc-native"

#FILES:kernel-dev += "/${KERNEL_IMAGEDEST}/${KERNEL_IMAGETYPE}-${KERNEL_VERSION}"

# KERNEL_CC = "${WORKSPACE}/old-toolchain/arm/bin/arm-linux-gnueabi-gcc"
# KERNEL_LD = "${WORKSPACE}/old-toolchain/arm/bin/arm-linux-gnueabi-ld"

CFLAGS += "-fno-strict-aliasing -fno-tree-vectorize"

do_configure () {
    oe_runmake_call CC="${KERNEL_CC}" LD="${KERNEL_LD}" -C ${S} ARCH=${ARCH} ${KERNEL_EXTRA_ARGS} ${KERNEL_CONFIG}
}

do_compile () {
    unset LDFLAGS
    oe_runmake CC="${KERNEL_CC}" LD="${KERNEL_LD}" ${KERNEL_EXTRA_ARGS} $use_alternate_initrd
}

do_shared_workdir:append () {
        cp Makefile $kerneldir/
        cp -fR usr $kerneldir/

        cp include/config/auto.conf $kerneldir/include/config/auto.conf

        if [ -d arch/${ARCH}/include ]; then
                mkdir -p $kerneldir/arch/${ARCH}/include/
                cp -fR arch/${ARCH}/include/* $kerneldir/arch/${ARCH}/include/
        fi

        if [ -d arch/${ARCH}/boot ]; then
                mkdir -p $kerneldir/arch/${ARCH}/boot/
                cp -fR arch/${ARCH}/boot/* $kerneldir/arch/${ARCH}/boot/
        fi

        if [ -d scripts ]; then
            for i in \
                scripts/basic/bin2c \
                scripts/basic/fixdep \
                scripts/conmakehash \
                scripts/dtc/dtc \
                scripts/genksyms/genksyms \
                scripts/kallsyms \
                scripts/kconfig/conf \
                scripts/mod/mk_elfconfig \
                scripts/mod/modpost \
                scripts/sign-file \
                scripts/sortextable;
            do
                if [ -e $i ]; then
                    mkdir -p $kerneldir/`dirname $i`
                    cp $i $kerneldir/$i
                fi
            done
        fi

        cp ${STAGING_KERNEL_DIR}/scripts/gen_initramfs_list.sh $kerneldir/scripts/

        # Copy vmlinux and zImage into deplydir for boot.img creation
        install -m 0644 ${KERNEL_OUTPUT_DIR}/${KERNEL_IMAGETYPE} ${DEPLOY_DIR_IMAGE}/${KERNEL_IMAGETYPE}-${MACHINE}.bin
        install -m 0644 vmlinux ${DEPLOY_DIR_IMAGE}

        # Generate kernel headers
        oe_runmake_call -C ${STAGING_KERNEL_DIR} ARCH=${ARCH} CC="${KERNEL_CC}" LD="${KERNEL_LD}" headers_install O=${STAGING_KERNEL_BUILDDIR}
}

do_install:append() {
    find ${DEPLOY_DIR_IMAGE} -type d -name '.debug' -exec rm -rf {} +
}

do_shared_workdir[dirs] = "${DEPLOY_DIR_IMAGE}"
KERNEL_VERSION_SANITY_SKIP = "1"
INSANE_SKIP:${PN} += " installed-vs-shipped"
INSANE_SKIP:${PN} += " debug-files"
do_package_qa[noexec] = "1"
