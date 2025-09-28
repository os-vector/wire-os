#
# <anki>
# VIC-2065: PasswordAuthentication=no
# </anki>
#
FILESEXTRAPATHS:prepend := "${THISDIR}/files:"

SRC_URI += "file://sshdgenkeys.service \
      file://sshd_config \
      file://ssh_host_rsa_key \
      file://ssh_host_dsa_key \
      file://ssh_host_ecdsa_key \
      file://ssh_host_ed25519_key \
      file://ssh_root_key.pub \
      "

# EXTRA_OECONF += " --sysconfdir=/data/ssh"

EXTRA_OECONF:append = " ${@bb.utils.contains('DISTRO_FEATURES', 'selinux', '--with-selinux', '', d)}"
BASEPRODUCT = "${@d.getVar('PRODUCT', False)}"
do_install:append () {
    sed -i -e 's:#PermitRootLogin yes:PermitRootLogin yes:' ${UNPACKDIR}/sshd_config ${D}${sysconfdir}/ssh/sshd_config
    sed -i -e 's:#PasswordAuthentication yes:PasswordAuthentication no:' ${UNPACKDIR}/sshd_config ${D}${sysconfdir}/ssh/sshd_config
    sed -i -e 's:#PermitRootLogin yes:PermitRootLogin yes:' ${UNPACKDIR}/sshd_config ${D}${sysconfdir}/ssh/sshd_config_readonly
    sed -i -e 's:#PasswordAuthentication yes:PasswordAuthentication no:' ${UNPACKDIR}/sshd_config ${D}${sysconfdir}/ssh/sshd_config_readonly
    sed -i '$a    StrictHostKeyChecking no' ${UNPACKDIR}/ssh_config ${D}${sysconfdir}/ssh/ssh_config
    sed -i '$a    UserKnownHostsFile /dev/null' ${UNPACKDIR}/ssh_config ${D}${sysconfdir}/ssh/ssh_config
    sed -i -e 's:.ssh/authorized_keys: .ssh/authorized_keys /etc/ssh/authorized_keys:' ${UNPACKDIR}/sshd_config ${D}${sysconfdir}/ssh/sshd_config
    
    # kercre123 - make sftp work
    sed -i -e 's:/usr/libexec/sftp-server:internal-sftp:' ${UNPACKDIR}/sshd_config ${D}${sysconfdir}/ssh/sshd_config

    # Switch-modder - Allow ssh over rsa
    sed -i '$a PubkeyAcceptedKeyTypes +ssh-rsa' ${UNPACKDIR}/sshd_config ${D}${sysconfdir}/ssh/sshd_config
    sed -i '$a HostKeyAlgorithms +ssh-rsa' ${UNPACKDIR}/sshd_config ${D}${sysconfdir}/ssh/sshd_config
    sed -i '$a HostKey /etc/ssh/ssh_host_rsa_key' ${UNPACKDIR}/sshd_config ${D}${sysconfdir}/ssh/sshd_config

    install -m 0600 ${UNPACKDIR}/ssh_host_rsa_key ${D}${sysconfdir}/ssh/ssh_host_rsa_key
    install -m 0600 ${UNPACKDIR}/ssh_host_dsa_key ${D}${sysconfdir}/ssh/ssh_host_dsa_key
    install -m 0600 ${UNPACKDIR}/ssh_host_ecdsa_key ${D}${sysconfdir}/ssh/ssh_host_ecdsa_key
    install -m 0600 ${UNPACKDIR}/ssh_host_ed25519_key ${D}${sysconfdir}/ssh/ssh_host_ed25519_key
    install -m 0600 ${UNPACKDIR}/ssh_root_key.pub ${D}${sysconfdir}/ssh/authorized_keys
}

RDEPENDS:${PN} += "${PN}-sftp"
