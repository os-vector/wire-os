#!/bin/bash

set -e

# Hidden arguments;
# 1. -au: enable auto-updates

# Hidden env vars:
# 1. AUTO_UPDATE: set to 1 if you want to inhibit the -au interaction

CREATOR="Wire"

CURRENT_CONTAINER_NAME="vic-yocto-builder-7"

function usage() {
    echo "$1"
    echo "Usage: ./build/build.sh -bt <dev/oskr/devcloudless> -s -op <OTA-pw> -bp <boot-passwd> -v <build-increment> -ui <ui-option>"
    echo "Usage (no signing): ./build/build.sh -bt <dev/oskr/devcloudless> -bp <boot-passwd> -v <build-increment> -ui <ui-option>"
    echo "Valid UI options are: knotty, ncurses, taskexp_ncurses, or teamcity. Default is knotty."
    exit 1
}

if [[ ! "$(uname -a)" == *"Linux"* ]] || [[ ! "$(uname -a)" == *"x86_64"* && ! "$(uname -a)" == *"aarch64"* ]]; then
	echo "This is not x86_64/amd64 or aarch64/arm64 Linux. Exiting."
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

function check_submodules() {
	BAD_SUBMODULE=0
	if [[ ! -d anki/victor/engine ]]; then
		errorMsg "The anki/victor submodule doesn't exist."
		BAD_SUBMODULE=1
	fi
	if [[ ! -d poky/openembedded-core/meta ]]; then
		errorMsg "The poky/openembedded-core submodule doesn't exist."
		BAD_SUBMODULE=1
	fi
	if [[ ! -d poky/meta-openembedded/meta-oe ]]; then
		errorMsg "The poky/meta-openembedded submodule doesn't exist."
		BAD_SUBMODULE=1
	fi
	if [[ ! -d external/purplpkg/bash ]]; then
		errorMsg "The external/purplpkg submodule doesn't exist."
		BAD_SUBMODULE=1
	fi
	if [[ ! -d anki/wired/webroot ]]; then
		errorMsg "The anki/wired submodule doesn't exist."
		BAD_SUBMODULE=1
	fi
	if [[ ${BAD_SUBMODULE} == 1 ]]; then
		errorMsg "Please configure your submodules properly."
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

#knotty, ncurses, taskexp_ncurses or teamcity - default knotty
function what_ui() {
    GIVEN_UI="$1"
    if [[ "${GIVEN_UI}" != "knotty" && "${GIVEN_UI}" != "ncurses" && "${GIVEN_UI}" != "taskexp" && "${GIVEN_UI}" != "taskexp_ncurses" && "${GIVEN_UI}" != "teamcity" ]]; then
        errorMsg "Invalid UI option: ${GIVEN_UI}"
        usage
    fi
    if [[ "${GIVEN_UI}" == *"taskexp"* ]]; then
        UI_FLAG="-g -u ${GIVEN_UI}"
    else
        UI_FLAG="-u ${GIVEN_UI}"
    fi
}

while [ $# -gt 0 ]; do
    case "$1" in
        -bt) BOT_TYPE="$2"; shift ;;
        -op) OTA_SIGNING_KEY_PASSWORD="$2"; shift ;;
        -bp) BOOT_PASSWORD="$2"; shift ;;
        -s) DO_SIGN=1 ;;
        -v) BUILD_INCREMENT="$2"; shift ;;
        -au) are_you_wire; AUTO_UPDATE=1 ;;
        -ui) what_ui "$2"; shift ;;
        -nd) NO_DOCKER=1 ;;
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

check_submodules

is_victor_there_and_compatible

if [[ "$BOT_TYPE" != "oskr" && "$BOT_TYPE" != "dev" && "$BOT_TYPE" != "prod" && "$BOT_TYPE" != "devcloudless" ]]; then
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

if [[ "${NO_DOCKER}" != "1" && "$(uname -a)" == *"aarch64" ]]; then
    errorMsg "Docker building does not work on aarch64. Follow the steps for building on bare metal."
    exit 1
fi

echo "All checks passed. Building."

mkdir -p build/cache
mkdir -p build/gocache
mkdir -p build/usercache
mkdir -p anki-deps

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

YOCTO_CLEAN_COMMAND="echo -e \"\e[1;32mCleaning some recipes...\e[0m\" && echo && clean-${BOT_TYPE} ${UI_FLAG}"
YOCTO_BUILD_COMMAND="echo && echo -e \"\e[1;32mBuilding the OS...\e[0m\" && echo && build-${BOT_TYPE} ${UI_FLAG}"

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
else
	BOOT_MAKE_COMMAND="make devsign"
fi

if [[ $DO_SIGN == 1 ]]; then
    export OTA_MANIFEST_SIGNING_KEY=$OTA_SIGNING_KEY_PASSWORD
    export DO_SIGN=$DO_SIGN
fi

if [[ "${NO_DOCKER}" != "1" ]]; then
    if [[ -z $(docker images -q ${CURRENT_CONTAINER_NAME}) ]]; then
        docker build --build-arg DIR_PATH="${DIRPATH}" --build-arg USER_NAME=$USER --build-arg UID=$(id -u $USER) --build-arg GID=$(id -u $USER) -t ${CURRENT_CONTAINER_NAME} build/
    else
        echo "Reusing ${CURRENT_CONTAINER_NAME}"
    fi
fi

function run_with_docker() {
    docker run -it --rm \
    -v $(pwd)/anki-deps:/home/$USER/.anki \
    -v $(pwd):$(pwd) \
    -v $(pwd)/build/cache:/home/$USER/.ccache \
    -v $(pwd)/build/gocache:/home/$USER/go \
    -v $(pwd)/build/usercache:/home/$USER/.cache \
    ${CURRENT_CONTAINER_NAME} bash -c "$@"
}

FINAL_BUILD_INVOCATION="cd $(pwd)/poky && \
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
    ANKIDEV=${ANKIDEV} make"

if [[ ${NO_DOCKER} == "1" ]]; then
    bash -c "${FINAL_BUILD_INVOCATION}"
else
    run_with_docker "${FINAL_BUILD_INVOCATION}"
fi

echo
echo -e "\033[1;32mCompleted successfully. Output is in ./_build.\033[0m"
echo
