DESCRIPTION = "Anki Version Number"
LICENSE = "Anki-Inc.-Proprietary"
LIC_FILES_CHKSUM = "file://${COREBASE}/../victor/meta-qcom/files/anki-licenses/\
Anki-Inc.-Proprietary;md5=4b03b8ffef1b70b13d869dbce43e8f09"
inherit qperf

FILESPATH =+ "${WORKSPACE}:"
SRC_URI = "file://ANKI_VERSION"
SRC_URI += "file://VICTOR_COMPAT_VERSION"

S = "${UNPACKDIR}"
#UNPACKDIR = "${S}"

do_install:append () {
    install -d ${D}/etc
    install -m 0444 ${S}/ANKI_VERSION ${D}/etc/os-version-base

    # ANKI_BUILD_VERSION will be set by TeamCity for CI builds, default to 0 for developer builds
    : ${ANKI_BUILD_VERSION:=0}
    echo "${ANKI_BUILD_VERSION}" > ${D}/etc/os-version-code
    chmod 0444 ${D}/etc/os-version-code

    if [ -z ${ANKI_BUILD_REVISION} ]; then
        GIT=`which git`
        if [ ! -z $GIT ]; then
            ANKI_BUILD_REVISION="something"
        else
            ANKI_BUILD_REVISION=""
        fi
    fi
    echo "${ANKI_BUILD_REVISION}" > ${D}/etc/os-version-rev
    echo 0444 ${D}/etc/os-version-rev

    # build type tag
    if [[ ${USER_BUILD} != "1" ]]; then
        if [[ ${OSKR} = "1" ]]; then
            # set to "oskr" for oskr builds
	    ANKI_BUILD_TYPE="oskr"
	elif [[ ${ANKI_RESOURCE_ESCAPEPOD} == "1" ]]; then
            # set to "oskr" for oskr builds
	    ANKI_BUILD_TYPE="epd"
	else
            # set to "d" for dev builds
            ANKI_BUILD_TYPE="d"
	fi
    elif [[ ${DEV} = "1" ]]; then
	# set to "ud" for userdev builds
	ANKI_BUILD_TYPE="ud"
    elif [[ ${ANKI_RESOURCE_ESCAPEPOD} == "1" ]]; then
        ANKI_BUILD_TYPE="ep"
    else
        # empty for user (release) builds
        ANKI_BUILD_TYPE=""
    fi

    BASE_VERSION=$(cat ${S}/ANKI_VERSION)
    echo "${BASE_VERSION}.${ANKI_BUILD_VERSION}${ANKI_BUILD_TYPE}" > ${D}/etc/os-version
    chmod 0444 ${D}/etc/os-version

    # This victor compatibility version can be used to prevent victor.git developers from
    # deploying code onto a newer OS that they are no longer compatible with.  As well, it
    # will prevent them from deploying new code onto an older incompatible robot.
    install -m 0444 ${S}/VICTOR_COMPAT_VERSION ${D}/etc/victor-compat-version
}

FILES:${PN} += "etc/os-version-base"
FILES:${PN} += "etc/os-version-code"
FILES:${PN} += "etc/os-version-rev"
FILES:${PN} += "etc/os-version"
FILES:${PN} += "etc/victor-compat-version"
