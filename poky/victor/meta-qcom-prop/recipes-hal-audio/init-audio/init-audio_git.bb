inherit update-rc.d systemd

DESCRIPTION = "Installing audio init script"
# do you really have to license a script which just echoes a 1 to somewhere in sys?
LIC_FILES_CHKSUM = "file://${COREBASE}/meta/files/common-licenses/BSD-3-Clause;md5=550794465ba0ec5312d6919e203a55f9"
LICENSE = "BSD-3-Clause"
PR = "r5"

SRC_URI = "file://init_qcom_audio"
SRC_URI += "file://init_audio.service"

S = "${UNPACKDIR}"
#UNPACKDIR = "${S}"

INITSCRIPT_NAME = "init_qcom_audio"
INITSCRIPT_PARAMS = "start 99 2 3 4 5 . stop 1 0 1 6 ."
INITSCRIPT_NAME_apq8009 = "init_qcom_audio"
INITSCRIPT_PARAMS_apq8009 = "start 38 2 3 4 5 . stop 1 0 1 6 ."
INITSCRIPT_NAME_msm8974 = "init_qcom_audio"
INITSCRIPT_PARAMS_msm8974 = "start 99 2 3 4 5 . stop 1 0 1 6 ."
INITSCRIPT_NAME_msm8610 = "init_qcom_audio"
INITSCRIPT_PARAMS_msm8610 = "start 99 2 3 4 5 . stop 1 0 1 6 ."

do_install() {
    install -m 0755 ${S}/init_qcom_audio -D ${D}${sysconfdir}/initscripts/init_qcom_audio
    install -d ${D}/etc/systemd/system/
    install -m 0644 ${S}/init_audio.service -D ${D}${sysconfdir}/systemd/system/init_audio.service
    install -d ${D}/etc/systemd/system/multi-user.target.wants
    ln -sf /etc/systemd/system/init_audio.service ${D}/etc/systemd/system/multi-user.target.wants/init_audio.service
    install -d ${D}/etc/systemd/system/ffbm.target.wants
    ln -sf /etc/systemd/system/init_audio.service ${D}/etc/systemd/system/ffbm.target.wants/init_audio.service
}

FILES:${PN} += "${sysconfdir}/initscripts"
FILES:${PN} += "${sysconfdir}/systemd/system"