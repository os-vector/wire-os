#!/bin/bash

set -e

# Hidden arguments;
# 1. -au: enable auto-updates

# Hidden env vars:
# 1. AUTO_UPDATE: set to 1 if you want to inhibit the -au interaction

CREATOR="Wire"

CURRENT_CONTAINER_NAME="vic-yocto-builder-6"
OLD_CONTAINER_NAME="vic-yocto-builder-5"

function usage() {
    echo "$1"
    echo "Usage: ./build/build.sh -bt <dev/oskr/devcloudless/oskrcloudless> -s -op <OTA-pw> -bp <boot-passwd> -v <build-increment>"
    echo "Usage (no signing): ./build/build.sh -bt <dev/oskr/devcloudless/oskrcloudless> -bp <boot-passwd> -v <build-increment>"
    exit 1
}

if [[ ! "$(uname -a)" == *"Linux"* ]] || [[ ! "$(uname -a)" == *"x86_64"* ]]; then
	echo "This is not x86_64/amd64 Linux. Exiting."
	exit 1
fi

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

function are_you_wire() {
	if [[ "${AUTO_UPDATE}" != "1" ]]; then
		echo "Are you $CREATOR?"
		read -p "(y/n): " yn
		case $yn in
			[Yy]* ) echo "Cool." ;;
			[Nn]* ) echo; echo "Then don't use the -au argument!"; exit 1;;
			* ) echo "that is not a y or an n."; exit 1;;
		esac
	fi
}

function errorMsg() {
        echo -e "\033[1;31m${1}\033[0m"
}

function is_victor_there_and_compatible() {
	if [[ ! -d anki/victor/engine ]]; then
		errorMsg "anki/victor/engine not found. You likely don't have the victor submodule correctly configured."
		exit 1
	fi
	VICTOR_COMPAT="$(cat anki/victor/VICTOR_COMPAT_VERSION)"
	OELINUX_COMPAT="$(cat VICTOR_COMPAT_VERSION)"
	if [[ ! "${VICTOR_COMPAT}" == "${OELINUX_COMPAT}" ]]; then
		errorMsg "OELinux and victor compat versions are not the same."
		echo
		errorMsg "victor: ${VICTOR_COMPAT}"
		errorMsg "OELinux: ${OELINUX_COMPAT}"
		echo
		errorMsg "Make sure you have synced all WireOS changes into your OS."
		exit 1
	fi
	echo "OELinux and victor compat versions are the same"
}

while [ $# -gt 0 ]; do
    case "$1" in
        -bt) BOT_TYPE="$2"; shift ;;
        -op) OTA_SIGNING_KEY_PASSWORD="$2"; shift ;;
        -bp) BOOT_PASSWORD="$2"; shift ;;
        -s) DO_SIGN=1 ;;
        -v) BUILD_INCREMENT="$2"; shift ;;
        -au) are_you_wire; AUTO_UPDATE=1 ;;
        *)
            usage "unknown option: $1"
            exit 1 ;;
    esac
    shift
done

if [[ "${AUTO_UPDATE}" == "1" ]]; then
	echo "Build will auto-update (env var set)"
	AUTO_UPDATE=1
fi

is_victor_there_and_compatible

if [[ "$BOT_TYPE" != "oskr" && "$BOT_TYPE" != "dev" && "$BOT_TYPE" != "prod" && "$BOT_TYPE" != "devcloudless" && "$BOT_TYPE" != "oskrcloudless" ]]; then
    usage "BOT_TYPE (-bt) should be 'oskr', 'dev', 'devcloudless', or 'oskrcloudless', got: $BOT_TYPE"
fi

if [[ "$DO_SIGN" == 1 && "$OTA_SIGNING_KEY_PASSWORD" == "" ]]; then
    usage "-s was given, but no OTA password was given"
fi

if [[ "$DO_SIGN" == 1 ]]; then
    check_sign_ota
fi

if [[ "$BOT_TYPE" == "oskr" && "$BOT_TYPE" == "oskrcloudless" ]]; then
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
mkdir -p build/gocache
mkdir -p build/usercache

echo "Getting deps (if needed)..."
./build/deps.sh

rm -rf poky/build/tmp-glibc/deploy/images/apq8009-robot-robot-perf/apq8009-robot-sysfs.ext4

DIRPATH="$(pwd)"

function cleanMsg() {
	echo
	echo -e "\e[1;32mCleaning some recipes...\e[0m"
	echo
}

function buildMsg() {
	echo
        echo -e "\e[1;32mBuilding the OS...\e[0m"
	echo
}

YOCTO_CLEAN_COMMAND="echo -e \"\e[1;32mCleaning some recipes...\e[0m\" && echo && clean-${BOT_TYPE}"
YOCTO_BUILD_COMMAND="echo && echo -e \"\e[1;32mBuilding the OS...\e[0m\" && echo && build-${BOT_TYPE}"

echo "Building a $BOT_TYPE OTA"
export BOOT_IMAGE_SIGNING_PASSWORD="${BOOT_PASSWORD}"

ANKIDEV=1

if [[ $BOT_TYPE == "oskr" ]]; then
        export BOOT_IMAGE_SIGNING_PASSWORD="${BOOT_PASSWORD}"
	BOOT_MAKE_COMMAND="make oskrsign"
elif [[ $BOT_TYPE == "prod" ]]; then
        export BOOT_IMAGE_SIGNING_PASSWORD="${BOOT_PASSWORD}"
	BOOT_MAKE_COMMAND="make prodsign"
	ANKIDEV=0
elif [[ $BOT_TYPE == "devcloudless" ]]; then
        BOOT_MAKE_COMMAND="make devsign"
elif [[ $BOT_TYPE == "oskrcloudless" ]]; then
        export BOOT_IMAGE_SIGNING_PASSWORD="${BOOT_PASSWORD}"
        BOOT_MAKE_COMMAND="make oskrsign"
else
	BOOT_MAKE_COMMAND="make devsign"
fi

if [[ $DO_SIGN == 1 ]]; then
    export OTA_MANIFEST_SIGNING_KEY=$OTA_SIGNING_KEY_PASSWORD
    export DO_SIGN=$DO_SIGN
fi

if [[ ! -z $(docker images -q ${OLD_CONTAINER_NAME}) ]]; then
	echo "Purging old docker containers... this might take a while"
	docker ps -a --filter "ancestor=${OLD_CONTAINER_NAME}" -q | xargs -r docker rm -f
	docker rmi -f $(docker images --filter "reference=${OLD_CONTAINER_NAME}*" --format '{{.ID}}')
	#echo
	#echo -e "\033[5m\033[1m\033[31mOld Docker builder detected on system. If you have built victor or wire-os many times, it is recommended you run:\033[0m"
	#echo
	#echo -e "\033[1m\033[36mdocker system prune -a --volumes\033[0m"
	#echo
	#echo -e "\033[32mPrevious versions of wire-os did not include a --rm flag in the docker run command. This means you probably have wasted space which can be cleared out with the above command.\033[0m"
	#echo -e "\033[32mContinuing in 10 seconds...\033[0m"
	#sleep 10
fi

if [[ -z $(docker images -q ${CURRENT_CONTAINER_NAME}) ]]; then
	docker build --build-arg DIR_PATH="${DIRPATH}" --build-arg USER_NAME=$USER --build-arg UID=$(id -u $USER) --build-arg GID=$(id -u $USER) -t ${CURRENT_CONTAINER_NAME} build/
else
	echo "Reusing ${CURRENT_CONTAINER_NAME}"
fi
docker run -it --rm \
    -v $(pwd)/anki-deps:/home/$USER/.anki \
    -v $(pwd):$(pwd) \
    -v $(pwd)/build/cache:/home/$USER/.ccache \
    -v $(pwd)/build/gocache:/home/$USER/go \
    -v $(pwd)/build/usercache:/home/$USER/.cache \
    ${CURRENT_CONTAINER_NAME} bash -c \
    "cd $(pwd)/poky && \
    source build/conf/set_bb_env.sh && \
    export ANKI_BUILD_VERSION=$BUILD_INCREMENT && \
    export AUTO_UPDATE=${AUTO_UPDATE} && \
    ${YOCTO_CLEAN_COMMAND} && \
    sleep 2 && \
    ${YOCTO_BUILD_COMMAND} && \
    cd ${DIRPATH}/ota && \
    rm -rf ../_build/*.img ../_build/*.stats ../_build/*.ini ../_build/*.enc && \
    export DO_SIGN=${DO_SIGN} && \
    export OTA_MANIFEST_SIGNING_KEY=${OTA_SIGNING_KEY_PASSWORD} && \
    export BOOT_IMAGE_SIGNING_PASSWORD=${BOOT_PASSWORD} && \
    ${BOOT_MAKE_COMMAND} && \
    ANKIDEV=${ANKIDEV} make"

echo
echo -e "\033[1;32mCompleted successfully. Output is in ./_build.\033[0m"
echo
