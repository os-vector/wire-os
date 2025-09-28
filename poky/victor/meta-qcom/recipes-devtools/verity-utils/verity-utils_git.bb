inherit autotools pkgconfig

BBCLASSEXTEND += "native"

DESCRIPTION = "Verity utilites"
HOMEPAGE = "http://developer.android.com/"
LICENSE = "Apache-2.0"
LIC_FILES_CHKSUM = "file://${COREBASE}/meta/files/common-licenses/\
${LICENSE};md5=89aea4e17d99a7cacdbeed46a0096b10"

PR = "r0"

DEPENDS = "libgcc libmincrypt libsparse zlib openssl bouncycastle"

FILESPATH =+ "${WORKSPACE}/system/extras/:"
SRC_URI = "file://verity"

S = "${UNPACKDIR}/verity"

EXTRA_OECONF =  "--with-coreheader-includes=${WORKSPACE}/system/core/include"
EXTRA_OECONF += "--with-mkbootimgheader-includes=${WORKSPACE}/system/core/mkbootimg"
EXTRA_OECONF += "--with-ext4utils-includes=${WORKSPACE}/system/extras/ext4_utils"

editveritysigner () {
    sed -i -e '/^java/d' ${S}/verity_signer
    echo 'java -Xmx512M -jar ${STAGING_LIBDIR}/VeritSigner.jar "$@"' >> ${S}/verity_signer
}

do_install:append () {
    editveritysigner
    install -m 755 ${S}/verity_signer ${D}/${bindir}/verity_signer
    install -m 755 ${S}/build_verity_metadata.py ${D}/${bindir}/build_verity_metadata.py
}

#NATIVE_INSTALL_WORKS="1"
