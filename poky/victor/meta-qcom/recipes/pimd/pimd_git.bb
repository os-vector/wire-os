inherit autotools-brokensep linux-kernel-base
DESCRIPTION = "PIMD - Multicast Routing Daemon"
LICENSE = "BSD-3-Clause"
LIC_FILES_CHKSUM = "file://LICENSE;md5=94f108f91fab720d62425770b70dd790"

PR = "r5"
do_configure() {
    :
}

SRCREV = "c4b1c9f4b5eaa70931d0f62f456ae10ac4c4a829"
SRC_URI = "${CLO_LE_GIT}/pimd.git;protocol=https;branch=caf_migration/github/master \
           file://0001-pimb-multicast-support-on-network.patch \
           file://no-deprecated-declarations.patch \
           file://0001-pimd-Resolve-wrong-missing-if-clause-gaurds-error-fo.patch \
"

SRC_URI:append_sdxprairie = "\
           file://Resolve-Implicit-fallthrough-Werror.patch "

SRC_URI:append_9615-cdp = " \
           file://defs_fix_multicast_subnetmask_on_rmnet.patch \
           file://vif_fix_multicast_subnetmask_on_rmnet.patch \
           file://pimd.conf \
           file://config_fix_multicast_subnetmask_on_rmnet.patch "

S = "${UNPACKDIR}/git"

do_compile() {
  make
}

do_install() {
        make install DESTDIR=${D}
}
do_install:append_9615-cdp() {
    install -m 0755 ${WORKDIR}/pimd.conf ${D}${sysconfdir}
}
