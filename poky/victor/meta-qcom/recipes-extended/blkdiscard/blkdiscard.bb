DESCRIPTION = "blkdiscard utility forked from https://git.kernel.org/pub/scm/utils/util-linux/util-linux.git/tree/sys-utils/blkdiscard.c"
LICENSE = "GPLv2+"
LIC_FILES_CHKSUM = "file://${COREBASE}/../victor/meta-qcom/recipes-extended/blkdiscard/files/blkdiscard.c;beginline=7;endline=18;md5=909275dfe35bdbdd8d9ddc0484cf03a9"

SRC_URI = "file://blkdiscard.c"

S = "${UNPACKDIR}"
#UNPACKDIR = "${S}"

do_compile () {
  ${CC} ${CFLAGS} ${LDFLAGS} ${S}/blkdiscard.c -o ${S}/blkdiscard
}

do_install() {
  install -d ${D}/usr/bin
  install -m 0755 ${S}/blkdiscard ${D}/usr/bin/
}

FILES:${PN} += "/usr/bin/blkdiscard"
