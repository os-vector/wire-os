inherit kernel kernel-yocto

DESCRIPTION = "CAF Linux Kernel"
LICENSE = "GPL-2.0-only"
LIC_FILES_CHKSUM = "file://COPYING;md5=d7810fab7487fb0aad327b76f1be7cd7"

python __anonymous () {
  # Override KERNEL_IMAGETYPE_FOR_MAKE variable, which is internal
  # to kernel.bbclass. We override the variable as msm kernel can't
  # support alternate image builds
  if d.getVar("KERNEL_IMAGETYPE", True):
      if d.getVar("INITRAMFS_IMAGE_BUNDLE", True):
          d.setVar("KERNEL_IMAGETYPE_FOR_MAKE", "${KERNEL_IMAGETYPE}")
      else:
          d.setVar("KERNEL_IMAGETYPE_FOR_MAKE", "")
}

DEPENDS += " mkbootimg-native openssl-native kern-tools-native"
RDEPENDS:${KERNEL_PACKAGE_NAME}-base = ""

LDFLAGS_aarch64 = "--hash-style=gnu --as-needed"

DEPENDS:append_aarch64 = " libgcc"
KERNEL_CC:append_aarch64 = " ${TOOLCHAIN_OPTIONS}"
KERNEL_LD:append_aarch64 = " ${TOOLCHAIN_OPTIONS}"

KERNEL_PRIORITY           = "9001"
# Add V=1 to KERNEL_EXTRA_ARGS for verbose
KERNEL_EXTRA_ARGS        += "O=${B} -Wno-attributes -Wno-implicit-function-declaration -w"

FILESPATH =+ "${WORKSPACE}:"
SRC_URI   =  "file://kernel \
	      ${@bb.utils.contains('DISTRO_FEATURES', 'systemd', 'file://systemd.cfg', '', d)} \
              ${@bb.utils.contains('DISTRO_FEATURES', 'virtualization', 'file://virtualization.cfg', '', d)} \
              ${@bb.utils.contains('DISTRO_FEATURES', 'nand-squashfs', 'file://squashfs.cfg', '', d)} \
              ${@bb.utils.contains('DISTRO_FEATURES', 'dm-verity', 'file://verity.cfg', '', d)} \
              ${@bb.utils.contains('DISTRO_FEATURES', 'zram', 'file://zram.cfg', '', d)} \
              ${@bb.utils.contains('DISTRO_FEATURES', 'nad-fde', 'file://fde.cfg', '', d)} \
	     "
SRC_URI:append_sdm845 = "${@bb.utils.contains('DISTRO_FEATURES', 'virtualization', 'file://virtualization_robomaker.cfg', '', d)}"

PACKAGE_ARCH = "${MACHINE_ARCH}"
PACKAGES = "kernel kernel-base kernel-vmlinux kernel-dev kernel-modules kernel-image"

KBRANCH ?= ""
KMETA = "kernel-meta"
KMACHINE ?= "${MACHINE}"
KCONFIG_MODE = "--alldefconfig"
#KBUILD_DEFCONFIG ?= "${KERNEL_CONFIG}"
LINUX_VERSION_EXTENSION = "${@['-perf', ''][d.getVar('VARIANT', True) == ('' or 'debug')]}"

do_kernel_checkout[noexec] = "1"

# extra task for configuration checks
addtask kernel_configcheck after do_configure before do_compile

python do_patch() {
    bb.build.exec_func('patch_do_patch', d)
}

do_compile () {
    oe_runmake CC="${KERNEL_CC}" LD="${KERNEL_LD}" ${KERNEL_EXTRA_ARGS} $use_alternate_initrd
}

do_deploy() {
    if [[ ${KERNEL_IMAGETYPE} != *-dtb ]]; then
        bberror "${PN}: Only appended DTB supported; Change KERNEL_IMAGETYPE to ${KERNEL_IMAGETYPE}-dtb in your kernel config."
        return
    fi

    if [ -f ${D}/${KERNEL_IMAGEDEST}/-${KERNEL_VERSION} ]; then
        mv ${D}/${KERNEL_IMAGEDEST}/-${KERNEL_VERSION} ${D}/${KERNEL_IMAGEDEST}/${KERNEL_IMAGETYPE}-${KERNEL_VERSION}
    fi
}

do_bundle_initramfs:append () {
    if [ ! -z "${INITRAMFS_IMAGE}" -a x"${INITRAMFS_IMAGE_BUNDLE}" = x1 ]; then
        realpath=`readlink -fn ${KERNEL_OUTPUT_DIR}/$imageType.initramfs`
        if [ -n "$realpath" ]; then
            # We need this to let the build system know the initramfs image got updated.
            # Otherwise the mkboot image will used the old one under deply directory.
            ln -sf ${realpath} ${DEPLOY_DIR_IMAGE}
        fi
    fi
}

# Put the zImage in the kernel-dev pkg
#FILES:kernel-dev += "/${KERNEL_IMAGEDEST}/${KERNEL_IMAGETYPE}-${KERNEL_VERSION}"
