inherit autotools pkgconfig

DESCRIPTION = "Build Filesystem Encryption Decryption Enabler-library utilising underlying crypto-hardware"
HOMEPAGE = "https://git.codelinaro.org/clo/la/device/qcom/common"

LICENSE = "BSD-3-Clause"
LIC_FILES_CHKSUM = "file://${COREBASE}/meta/files/common-licenses/${LICENSE};md5=550794465ba0ec5312d6919e203a55f9"
PR = "r0"

SRC_URI = "${CLO_LA_GIT}/device/qcom/common;protocol=https;nobranch=1;rev=de9a4014a349d23cce6acec077aa648a4a07bcc9;subpath=cryptfs_hw"
SRC_URI += "file://0001-cryptfs_hw-Replace-LOGE-with-SLOGE.patch"

S = "${UNPACKDIR}/cryptfs_hw"

DEPENDS += "libcutils libhardware system-core libselinux libscrypt"

EXTRA_OECONF += "--with-emmc-use-ICE"
