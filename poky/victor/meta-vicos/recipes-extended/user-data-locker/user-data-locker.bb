DESCRIPTION = "User Data Locker"
LICENSE = "Anki-Inc.-Proprietary"
LIC_FILES_CHKSUM = "file://${COREBASE}/../victor/meta-qcom/files/anki-licenses/\
Anki-Inc.-Proprietary;md5=4b03b8ffef1b70b13d869dbce43e8f09"

DEPENDS += "system-core"
DEPENDS += "liblog"

FILESPATH =+ "${WORKSPACE}:"
SRC_URI = "file://anki/user-data-locker/"

S = "${UNPACKDIR}/anki/user-data-locker"

do_compile[depends] += "virtual/kernel:do_shared_workdir"

TARGET_CXXFLAGS += "-I${STAGING_KERNEL_BUILDDIR}/usr/include"
TARGET_CXXFLAGS += "-I${WORKSPACE}/security/securemsm/keymaster"
TARGET_CXXFLAGS += "-I${WORKSPACE}/hardware/qcom/keymaster"

CXXFLAGS += "-fno-strict-aliasing -fno-tree-vectorize"
CFLAGS += "-fno-strict-aliasing -fno-tree-vectorize"

do_install() {
  mkdir -p ${D}/usr/bin
  install -m 0100 ${S}/user-data-locker ${D}/usr/bin/
}

FILES:${PN} = "usr/bin/user-data-locker"
