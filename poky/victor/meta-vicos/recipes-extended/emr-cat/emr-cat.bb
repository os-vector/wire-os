DESCRIPTION = "Anki Robot Electronic Medical Record Reading Utility"
#LICENSE = "MIT"
LICENSE = "Anki-Inc.-Proprietary"
LIC_FILES_CHKSUM = "file://${COREBASE}/../victor/meta-qcom/files/anki-licenses/\
Anki-Inc.-Proprietary;md5=4b03b8ffef1b70b13d869dbce43e8f09"

S = "${UNPACKDIR}"
#UNPACKDIR = "${S}"

SRC_URI = "file://emr-cat.c"

TARGET_CFLAGS += "-Os -Wall -Werror -Wno-unused-result -Wno-strict-aliasing -fPIC"

do_compile () {
  ${CC} ${CFLAGS} ${LDFLAGS} ${S}/emr-cat.c -o ${S}/emr-cat
}

do_install() {
  # idk where to put this, but we need a mount point for factory
  install -d ${D}/factory

  install -d ${D}/usr/bin
  install -m 0755 ${S}/emr-cat ${D}/usr/bin/
}

FILES:${PN} += "/usr/bin/emr-cat \
                /factory"
