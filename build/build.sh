#!/bin/bash

set -e

function usage() {
    echo "$1"
    echo "Usage: ./build/build.sh -bt <dev/oskr> -s -op <OTA-pw> -bp <boot-passwd> -v <build-increment>"
    echo "Usage (no signing): ./build/build.sh -bt <dev/oskr> -bp <boot-passwd> -v <build-increment>"
    exit 1
}

function check_sign_prod() {
    if openssl rsa -in ota/qtipri.encrypted.key -passin pass:"$BOOT_PASSWORD" -noout 2>/dev/null; then
        echo "Prod boot image key password confirmed to be correct!"
    else
        echo
        echo -e "\033[1;31mProd boot image signing password is incorrect. exiting.\033[0m"
        echo -e "\033[1;31mHINT: we are using an older version of the key which has the same password as the ABOOT key\033[0m"
        echo
        exit 1
    fi
}

function check_sign_oskr() {
    if openssl rsa -in ota/qtioskrpri.encrypted.key -passin pass:"$BOOT_PASSWORD" -noout 2>/dev/null; then
        echo "OSKR boot image key password confirmed to be correct!"
    else
        echo
        echo -e "\033[1;31mOSKR boot image signing password is incorrect. exiting.\033[0m"
        echo
        exit 1
    fi
}

function check_sign_ota() {
    if openssl rsa -in ota/ota_prod.key -passin pass:"$OTA_SIGNING_KEY_PASSWORD" -noout 2>/dev/null; then
		echo "OTA signing key password is confirmed to be correct!"
	else
		echo
		echo -e "\033[1;31mOTA signing key is incorrect. exiting.\033[0m"
		echo
		exit 1
	fi
}

while [ $# -gt 0 ]; do
    case "$1" in
        -bt) BOT_TYPE="$2"; shift ;;
        -op) OTA_SIGNING_KEY_PASSWORD="$2"; shift ;;
        -bp) BOOT_PASSWORD="$2"; shift ;;
        -s) DO_SIGN=1 ;;
        -v) BUILD_INCREMENT="$2"; shift ;;
        *)
            usage "unknown option: $1"
            exit 1 ;;
    esac
    shift
done

if [[ "$BOT_TYPE" != "oskr" && "$BOT_TYPE" != "dev" && "$BOT_TYPE" != "prod" ]]; then
    usage "BOT_TYPE (-bt) should be 'oskr' or 'dev', got: $BOT_TYPE"
fi

if [[ "$DO_SIGN" == 1 && "$OTA_SIGNING_KEY_PASSWORD" == "" ]]; then
    usage "-s was given, but no OTA password was given"
fi

if [[ "$DO_SIGN" == 1 ]]; then
    check_sign_ota
fi

if [[ "$BOT_TYPE" == "oskr" ]]; then
    check_sign_oskr
fi

if [[ "$BOT_TYPE" == "prod" ]]; then
    check_sign_prod
fi

if [[ ! $BUILD_INCREMENT =~ ^-?[0000-9999]+$ ]]; then
    usage "Build increment is not an int between 0-9999."
fi

echo "All checks passed. Building."

mkdir -p build/cache

echo "Getting deps (if needed)..."
./build/deps.sh

rm -rf poky/build/tmp-glibc/deploy/images/apq8009-robot-robot-perf/apq8009-robot-sysfs.ext4

DIRPATH="$(pwd)"

if [[ $BOT_TYPE == "oskr" ]]; then
	echo "Building an OSKR OTA"
    export BOOT_IMAGE_SIGNING_PASSWORD="${BOOT_PASSWORD}"
	YOCTO_BUILD_COMMAND="clean-oskr && build-oskr"
	BOOT_MAKE_COMMAND="make oskrsign"
elif [[ $BOT_TYPE == "prod" ]]; then
	echo "Building a prod OTA"
    export BOOT_IMAGE_SIGNING_PASSWORD="${BOOT_PASSWORD}"
	YOCTO_BUILD_COMMAND="clean-prod && build-prod"
	BOOT_MAKE_COMMAND="make prodsign"
else
    echo "Building a dev OTA"
	YOCTO_BUILD_COMMAND="clean-dev && build-dev"
	BOOT_MAKE_COMMAND="make devsign"
fi

if [[ $DO_SIGN == 1 ]]; then
    export OTA_MANIFEST_SIGNING_KEY=$OTA_SIGNING_KEY_PASSWORD
    export DO_SIGN=$DO_SIGN
fi

docker build --build-arg DIR_PATH="${DIRPATH}" --build-arg USER_NAME=$USER --build-arg UID=$(id -u $USER) --build-arg GID=$(id -g $USER) -t vic-yocto-builder-2 build/

docker run -it \
    -v $(pwd)/anki-deps:/home/$USER/.anki \
    -v $(pwd):$(pwd) \
    -v $(pwd)/build/cache:/home/$USER/.ccache \
    vic-yocto-builder-2 bash -c \
    "cd $(pwd)/poky && \
    source build/conf/set_bb_env.sh && \
    export ANKI_BUILD_VERSION=$BUILD_INCREMENT && \
    ${YOCTO_BUILD_COMMAND} && \
    cd ${DIRPATH}/ota && \
    rm -rf ../_build/*.img ../_build/*.stats ../_build/*.ini ../_build/*.enc && \
    export DO_SIGN=${DO_SIGN} && \
    export OTA_MANIFEST_SIGNING_KEY=${OTA_SIGNING_KEY_PASSWORD} && \
    export BOOT_IMAGE_SIGNING_PASSWORD=${BOOT_PASSWORD} && \
    ${BOOT_MAKE_COMMAND} && \
    make"

echo
echo -e "\033[1;32mCompleted successfully. Output is in ./_build.\033[0m"
echo
