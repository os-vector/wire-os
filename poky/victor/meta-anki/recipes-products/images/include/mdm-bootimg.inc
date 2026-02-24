# Force a few additional dependencies in the mix so that we get the needed
# recipes to build in the right order so we can make the bootimg file and
# our images...
DEPENDS += " \
             virtual/kernel \
             pkgconfig-native \
             gtk-doc-native \
             gettext-native \
             e2fsprogs-native"

IMAGE_LINGUAS = ""

# Default image output types
IMAGE_FSTYPES ?= "tar.gz ${INITRAMFS_FSTYPES}"

# Use busybox as login manager
IMAGE_LOGIN_MANAGER = ""

# Include minimum init and init scripts
IMAGE_DEV_MANAGER ?= "udev"
IMAGE_INIT_MANAGER ?= "sysvinit sysvinit-pidof"
IMAGE_INITSCRIPTS ?= ""

IMAGE_PREPROCESS_COMMAND:prepend = " gen_buildprop;"
EXTRA_IMAGE_FEATURES += "read-only-rootfs"

TARGET_NUM = "${@('0' if d.getVar('DEV') == '1' else '1' if d.getVar('OSKR') == '1' else '9' if d.getVar('USER_BUILD') == '1' else '0')}"

WHAT_OS_AM_I = "wire-os_wire_os"

gen_buildprop:append() {
   mkdir -p ${IMAGE_ROOTFS}/cache
   BUILD_TIMESTAMP=`cat ${IMAGE_ROOTFS}/etc/version`
   echo ro.build.version.release=${BUILD_TIMESTAMP} >> ${IMAGE_ROOTFS}/build.prop
   echo ro.product.name=${ANKI_PRODUCT_NAME} >> ${IMAGE_ROOTFS}/build.prop
   echo ${MACHINE} >> ${IMAGE_ROOTFS}/target

   # Anki-specific version info
   OS_VERSION=`cat ${IMAGE_ROOTFS}/etc/os-version`
   RELEASE_VERSION=`cat ${IMAGE_ROOTFS}/etc/version`
   
   # read from /anki/etc/version to prepend build.id
   VICTOR_BUILD_VERSION="${OS_VERSION}"
   VICTOR_BUILD_REV='rev'
   VICTOR_REV_TAG=""
   if [ ! -z "${VICTOR_BUILD_REV}" ]; then
      VICTOR_REV_TAG="-${VICTOR_BUILD_REV}"
   fi

   OS_REV=`cat ${IMAGE_ROOTFS}/etc/os-version-rev`
   REV_TAG=""
   if [ ! -z "${OS_REV}" ]; then
     REV_TAG="-${OS_REV}"
      # ro.revision is used by debuggerd to tag tombstone crash dumps
      echo "ro.revision=anki-${VICTOR_BUILD_REV}_os-${OS_REV}" >> ${IMAGE_ROOTFS}/build.prop
   fi

   # Combine victor + OS build versions
   VERSION_ID="${OS_VERSION}#${VICTOR_BUILD_REV}"
   BUILD_ID="v${VICTOR_BUILD_VERSION}${VICTOR_REV_TAG}_os${OS_VERSION}${REV_TAG}-${BUILD_TIMESTAMP}"

   echo "ro.anki.version=${OS_VERSION}" >> ${IMAGE_ROOTFS}/build.prop
   echo "ro.anki.victor.version=${VICTOR_BUILD_VERSION}" >> ${IMAGE_ROOTFS}/build.prop

   # ro.build.fingerprint is used by debuggerd to tag tombstone crash dumps
   echo "ro.build.fingerprint=${BUILD_ID}" >> ${IMAGE_ROOTFS}/build.prop

   # standard Android build info

   # build.id can be an alpha-numeric build identifier
   echo "ro.build.id=${BUILD_ID}" >> ${IMAGE_ROOTFS}/build.prop

   # build.display.id is intended for displaying a consumer facing build id
   if [ "${USER_BUILD}" == "1" ]; then
     # For Release builds, only display the OS version (victor code will never be deployed)
     echo "ro.build.display.id=${OS_VERSION}" >> ${IMAGE_ROOTFS}/build.prop
   else
     # For Dev builds, display victor + OS versions
     # Wire: my victor includes the sha in a new CCIS screen, not necessary to include here anymore
     echo "ro.build.display.id=${OS_VERSION}" >> ${IMAGE_ROOTFS}/build.prop
   fi

   # build.type is intended for apps to know what build type they are running on
   if [ "${USER_BUILD}" == "1" ]; then
      if [ "${FACTORY}" == "1" ]; then
         BUILD_TYPE_VALUE=factory
      else
         BUILD_TYPE_VALUE=production
      fi
   else
      if [ "${FACTORY}" == "1" ]; then
         BUILD_TYPE_VALUE=facdev
      else
         BUILD_TYPE_VALUE=development
      fi
   fi
   echo "ro.build.type=${BUILD_TYPE_VALUE}" >> ${IMAGE_ROOTFS}/build.prop

   # build.version.incremental is should be an increasing number
   VERSION_CODE=`cat ${IMAGE_ROOTFS}/etc/os-version-code`
   echo "ro.build.version.incremental=${VERSION_CODE}" >> ${IMAGE_ROOTFS}/build.prop

   echo "ro.build.target=${TARGET_NUM}" >> ${IMAGE_ROOTFS}/build.prop

   echo "ro.build.os.cfw.name=${WHAT_OS_AM_I}" >> ${IMAGE_ROOTFS}/build.prop
}
