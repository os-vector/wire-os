#############################################################################
#@COMMENT: This distribution configuration defines an OE Linux based
#@COMMENT: distribution. The matching buildable image target (machine-image)
#@COMMENT: basically consists of: libc, busybox, udev, sysv init, and a few
#@COMMENT: init scripts for running up the system on qti chipsets.
#############################################################################

# DISTRO FEATURE SELECTION
MICRO_GOLD = "ld-is-gold"

USE_DEVFS = "1"

# Use Debian naming scheme for library (.so) files
INHERIT += "recipe_sanity"

# TOOLCHAIN
#PREFERRED_VERSION_autoconf = "2.68"
ARM_INSTRUCTION_SET       ?= "arm"
FULL_OPTIMIZATION          = "-O2 -fomit-frame-pointer -fno-tree-vectorize -fno-strict-aliasing"
FULL_OPTIMIZATION          += " -Wno-error=maybe-uninitialized "

# These flags are taken from android build tree and can be used by recipes if needed.

OPTIONAL_CFLAGS = "-fstack-protector -fdata-sections -funwind-tables -Wa,--noexecstack  -Wformat  -Werror=format-security -D_FORTIFY_SOURCE=2  -fno-short-enums -no-canonical-prefixes -Wno-error=return-type"
OPTIONAL_CFLAGS += " -fmessage-length=0 -DNDEBUG -UDEBUG -Wno-error=unused-result -Wno-error=attributes -Wno-error=uninitialized -Wno-error=unused-variable -Wno-error=unused-parameter -Wno-error=builtin-macro-redefined -Wno-error=type-limits -Wno-error=unused-but-set-variable "
THUMB_FLAGS = " -Os -mthumb "
OPTIONAL_LDFLAGS = "-Wl,-z,noexecstack -Wl,-z,relro -Wl,-z,now -Wl,--warn-shared-textrel -Wl,--fatal-warnings "

# NLS
USE_NLS = "no"
USE_NLS_glib-2.0 = "yes"
USE_NLS_glib-2.0-native = "yes"
USE_NLS_gcc-cross = "no"

# Disable binary locale generation
ENABLE_BINARY_LOCALE_GENERATION = "0"

#Allow library symlinks to exist alongside soname files
PACKAGE_SNAP_LIB_SYMLINKS = "0"

# Don't install ldconfig and associated gubbins
USE_LDCONFIG = "0"
LDCONFIGDEPEND = ""
COMMERCIAL_LICENSE_DEPENDEES = ""

# Add the yaffs2 imaging commands to the filesystem support for Bitbake as
# OE-Core doesn't support them directly...
#IMAGE_CMD_yaffs2 = "mkyaffs2image ${EXTRA_IMAGECMD} ${IMAGE_ROOTFS} ${DEPLOY_DIR_IMAGE}/${IMAGE_NAME}.rootfs.yaffs2; chmod 644 ${DEPLOY_DIR_IMAGE}/${IMAGE_NAME}.rootfs.yaffs2"

# fastboot imaging commands
#IMAGE_DEPENDS_fastboot = "mkbootimg-native virtual/kernel"
#IMAGE_CMD_fastboot = "${STAGING_BINDIR_NATIVE}/mkbootimg --kernel ${STAGING_DIR_TARGET}/kernel/arch/arm/boot/Image --ramdisk /dev/null --ramdisk_offset $(awk --non-decimal-data '/ _end/ {end="0x" $1} /_stext/ {beg="0x" $1} END {size1=end-beg+4096; size=and(size1,compl(4095)); printf("%#x",size)}' ${STAGING_DIR_TARGET}/boot/System.map-${MACHINE_KERNEL_VERSION}) --cmdline 'root=${MACHINE_ROOTDEV} rw init=/sbin/init --verbose loglevel=7 rootwait console=${MACHINE_CONSOLE} no_console_suspend=1 androidboot.hardware=qcom log_buf_len=262144' --base ${MACHINE_KERNEL_BASE} ${EXTRA_IMAGECMD} --pagesize ${MACHINE_FLASH_PAGE_SIZE} --output ${DEPLOY_DIR_IMAGE}/${IMAGE_NAME}.rootfs.fastboot"

# Anki Version Info
QCOM_RELEASE = "r00014.1"
