DESCRIPTION = "Anki OTA Engine"
LICENSE = "Anki-Inc.-Proprietary"
LIC_FILES_CHKSUM = "file://${COREBASE}/../victor/meta-qcom/files/anki-licenses/\
Anki-Inc.-Proprietary;md5=4b03b8ffef1b70b13d869dbce43e8f09"

DEPENDS = "libcutils zlib libbsd"
RDEPENDS:${PN} = "zlib liblog libbsd"

SRC_URI = " \
      file://bootctl.cpp \
      file://bootctl.h \
      file://gpt-utils.cpp \
      file://gpt-utils.h \
      file://main.cpp \
      file://boot-successful.service \
      file://boot-successful.sh \
      file://sysswitch \
      "

do_compile () {
  ${CXX} ${S}/gpt-utils.cpp ${S}/bootctl.cpp ${S}/main.cpp \
    ${CXXFLAGS} ${LDFLAGS} \
    -I${STAGING_INCDIR} \
    -L${STAGING_LIBDIR} -lstdc++ -lz -llog -lbsd -o ${S}/bootctl
}

S = "${UNPACKDIR}"
#UNPACKDIR = "${S}"

do_install() {
  install -d ${D}/usr/bin
  install -m 0700 ${S}/bootctl ${D}/usr/bin/bootctl-anki

  install -m 0755 ${S}/sysswitch ${D}/usr/bin/sysswitch

  install -d ${D}${sysconfdir}/initscripts
  [ "${AUTO_UPDATE}" != "1" ] && touch ${D}${sysconfdir}/do-not-auto-update
  install -m 0755 ${S}/boot-successful.sh ${D}${sysconfdir}/initscripts/boot-successful
  install -d ${D}${sysconfdir}/systemd/system/
  install -m 0644 ${S}/boot-successful.service \
    -D ${D}${sysconfdir}/systemd/system/boot-successful.service
  install -d ${D}${sysconfdir}/systemd/system/multi-user.target.wants/
  ln -sf /etc/systemd/system/boot-successful.service \
    ${D}${sysconfdir}/systemd/system/multi-user.target.wants/boot-successful.service
}

FILES:${PN} += "/usr/bin"
FILES:${PN} += "/etc"

RDEPENDS:${PN} += "bash"
