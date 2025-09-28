#--------------------------------------------
#  The bouncycastle API is used by
#  kernel for secure boot image signing
#--------------------------------------------

DESCRIPTION = "Build Bouncycastle API JAR and Signed BootSignature JAR"
HOMEPAGE = "https//www.bouncycastle.org/java.html"

LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://../bouncycastle/MODULE_LICENSE_BSD_LIKE;md5=d41d8cd98f00b204e9800998ecf8427e"

# Commits of git repositories mentioned below are from LA.UM.5.6.c1 branch.

#######################################
#  bouncycastle git repo
#######################################

SRC_URI = "${CLO_LA_GIT}/platform/external/bouncycastle;protocol=https;nobranch=1;rev=a1a3e289e8f788a7757894c4131333ce38e89524;destsuffix=bouncycastle;name=bouncycastle"

SRC_URI[bouncycastle.md5sum] = "a786faa25a56a3e46331f34b4364575c"
SRC_URI[bouncycastle.sha256sum] = "909fb1e8ea06a9d6e918b3c5b0800a1f2408724a74e05cb310c019653d758e76"

S_BC = "${WORKDIR}/bouncycastle"


#######################################
#  system/extras git repo
#######################################
SRC_URI +=  "${CLO_LA_GIT}/platform/system/extras;protocol=https;nobranch=1;rev=20fe295659096920d7dfd3f317d4ecc8ef7c3c86;destsuffix=verity;subpath=verity;name=verity"

SRC_URI[verity.md5sum] = "ea0bac9d21dcae611ea6bdde97ed4125"
SRC_URI[verity.sha256sum] = "baa2967bbf0977150265ff878439c55bbd3b2095d7918146f6f9635268fc951e"

S_VERITY = "${WORKDIR}/verity"

#######################################
#  keys/certificates git repo
#######################################
SRC_URI +=  "${CLO_LA_GIT}/platform/build_repo;protocol=https;nobranch=1;rev=26bfd792b59eb163dfbc8602a931868fbb35bad4;destsuffix=security;subpath=target/product/security;name=security"

SRC_URI[security.md5sum] = "1ba8f1041c6f81834450aefc80e78ba8"
SRC_URI[security.sha256sum] = "2174c4c82d24ec91e94949a3d1eb4fac29cc657b3e35c662c3e80042576fb6b6"

S_SECURITY = "${WORKDIR}/security"
SECURITY_TOOLS_DIR = "${TMPDIR}/work-shared/security_tools"

# Move the scripts to a shared directory as described by SECURITY_TOOLS_DIR
#
base_do_unpack:append () {
    s = d.getVar("S_SECURITY", True)
    if s[-1] == '/':
        # drop trailing slash, so that os.symlink(signing_dir, s) doesn't use s as directory name and fail
        s=s[:-1]
    signing_dir = d.getVar("SECURITY_TOOLS_DIR", True)
    if s != signing_dir:
        bb.utils.mkdirhier(signing_dir)
        bb.utils.remove(signing_dir, recurse=True)
        import shutil
        shutil.move(s, signing_dir)
        os.symlink(signing_dir, s)
}

#######################################
#  build
#    - source
#    - classes
#    - libs
#######################################
BC_BUILD = "${WORKDIR}/build"

do_precompile() {
    rm -fr ${BC_BUILD}/classes
    rm -fr ${BC_BUILD}/libs
    rm -fr ${BC_BUILD}/sources

    mkdir -p ${BC_BUILD}/classes
    mkdir -p ${BC_BUILD}/libs
    mkdir -p ${BC_BUILD}/sources

    echo "Main-Class: com.android.verity.VeritySigner" > ${BC_BUILD}/VeritySigner.mf
    echo "Main-Class: com.android.verity.BootSignature" > ${BC_BUILD}/BootSignature.mf
    echo "Main-Class: com.android.verity.VerityVerifier" > ${BC_BUILD}/VerityVerifier.mf
    echo "Main-Class: com.android.verity.BootKeystore" > ${BC_BUILD}/KeystoreSigner.mf

}


#---------------------------------------
#  bouncycastle API
#---------------------------------------
do_bc_compile() {

    find ${S_BC}/bcprov/ -name "*.java" > ${BC_BUILD}/sources/bcprov_java_source_list
    find ${S_BC}/bcpkix/ -name "*.java" > ${BC_BUILD}/sources/bcpkix_java_source_list

    /usr/bin/javac -J-Xmx1024M            \
          -Xmaxerrs 9999999      \
          -encoding UTF-8        \
          -g                     \
          -d ${BC_BUILD}/classes \
          @${BC_BUILD}/sources/bcprov_java_source_list


    #--------------------------------
    # create bcprov JAR
    #--------------------------------
    /usr/bin/jar -cf ${BC_BUILD}/libs/bcprov_jar.jar -C ${BC_BUILD}/classes .

    #--------------------------------
    # compile bcpkix
    #--------------------------------
    /usr/bin/javac -J-Xmx1024M       \
          -Xmaxerrs 9999999 \
          -encoding UTF-8   \
          -g                \
          -classpath ${BC_BUILD}/libs/bcprov_jar.jar  \
          -d ${BC_BUILD}/classes \
          @${BC_BUILD}/sources/bcpkix_java_source_list

    #--------------------------------
    # create bcprov + bcpkix JAR
    #--------------------------------
    /usr/bin/jar -cf ${BC_BUILD}/libs/bcprov_bcpkix_jar.jar -C ${BC_BUILD}/classes .
}

do_verity_compile () {

    #--------------------------------
    # Create Verity.jar file
    #--------------------------------
    find ${S_VERITY}/ -name "*.java" >> ${BC_BUILD}/sources/VeritySigner_source


    #--------------------------------
    # compile BootSignature
    #--------------------------------
    /usr/bin/javac -J-Xmx1024M        \
          -Xmaxerrs 9999999  \
          -encoding UTF-8    \
          -g                 \
          -classpath ${BC_BUILD}/libs/bcprov_bcpkix_jar.jar  \
          -d ${BC_BUILD}/classes \
          @${BC_BUILD}/sources/VeritySigner_source

    #--------------------------------
    # create VerityVerifier JAR
    #--------------------------------
    /usr/bin/jar -cfm ${BC_BUILD}/libs/VerityVerifier.jar ${BC_BUILD}/VerityVerifier.mf -C ${BC_BUILD}/classes .

    #--------------------------------
    # create BootSignture JAR
    #--------------------------------
    /usr/bin/jar -cfm ${BC_BUILD}/libs/BootSignature.jar ${BC_BUILD}/BootSignature.mf -C ${BC_BUILD}/classes .

    #--------------------------------
    # create VeritSigner JAR
    #--------------------------------
    /usr/bin/jar -cfm ${BC_BUILD}/libs/VeritSigner.jar ${BC_BUILD}/VeritySigner.mf -C ${BC_BUILD}/classes .

    #--------------------------------
    # create BootKeystoreSigner JAR
    #--------------------------------
    /usr/bin/jar -cfm ${BC_BUILD}/libs/BootKeystoreSigner.jar ${BC_BUILD}/KeystoreSigner.mf -C ${BC_BUILD}/classes .
}

do_compile () {

    do_precompile
    do_bc_compile
    do_verity_compile
}

BBCLASSEXTEND = "native"

do_install_class-native () {
    install -d ${D}/${libdir}
    install -m 755 ${BC_BUILD}/libs/VeritSigner.jar ${D}/${libdir}/VeritSigner.jar
}

do_install:append_class-native () {
    install -d ${SYSROOT_DESTDIR}/${libdir}
    install -m 755 ${D}/${libdir}/VeritSigner.jar ${SYSROOT_DESTDIR}/${libdir}/VeritSigner.jar
}
