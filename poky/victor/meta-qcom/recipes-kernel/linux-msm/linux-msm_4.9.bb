require recipes-kernel/linux-msm/linux-msm.inc

COMPATIBLE_MACHINE = "(apq8009|apq8053|qcs605|sdm845|sdxpoorwills|mdm9650|mdm9607|genericarmv8-64|sa415m)"

KERNEL_IMAGEDEST = "boot"

SRC_DIR   =  "${WORKSPACE}/kernel/msm-4.9"
S         =  "${WORKDIR}/kernel/msm-4.9"
PR = "r5"

DEPENDS += "dtc-native"

SRC_URI += "file://defconfig"

#KERNEL_CC = "${WORKSPACE}/old-toolchain/arm/bin/arm-linux-gnueabihf-gcc"
#KERNEL_LD = "${WORKSPACE}/old-toolchain/arm/bin/arm-linux-gnueabihf-ld"

do_compile () {
    if ${@bb.utils.contains('DISTRO_FEATURES', 'avble', 'true', 'false', d)}; then
        oe_runmake CC="${KERNEL_CC}" LD="${KERNEL_LD}" ${KERNEL_EXTRA_ARGS} $use_alternate_initrd DTC_EXT=${STAGING_DIR_NATIVE}/usr/bin/dtc CONFIG_BUILD_ARM64_DT_OVERLAY=y
    else
        oe_runmake CC="${KERNEL_CC}" LD="${KERNEL_LD}" ${KERNEL_EXTRA_ARGS} $use_alternate_initrd
    fi
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
                scripts/recordmcount \
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
        install -m 0644 ${KERNEL_OUTPUT_DIR}/${KERNEL_IMAGETYPE} ${DEPLOY_DIR_IMAGE}/${KERNEL_IMAGETYPE}
        install -m 0644 vmlinux ${DEPLOY_DIR_IMAGE}

        # Generate kernel headers
        oe_runmake_call -C ${STAGING_KERNEL_DIR} ARCH=${ARCH} CC="${KERNEL_CC}" LD="${KERNEL_LD}" headers_install O=${STAGING_KERNEL_BUILDDIR}
}

do_install:append() {
    rm -rf ${DEPLOY_DIR_IMAGE}/.debug
}


do_shared_workdir[dirs] = "${DEPLOY_DIR_IMAGE}"
KERNEL_VERSION_SANITY_SKIP = "1"
INSANE_SKIP:${PN} += " installed-vs-shipped"
