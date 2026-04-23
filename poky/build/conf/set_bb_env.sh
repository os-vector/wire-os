# set_bb_env.sh
# Define macros for build targets.
# Generate bblayers.conf from get_bblayers.py.
# Some convenience macros are defined to save some typing.
# Set the build environement
if [[ ! $(readlink $(which sh)) =~ bash ]]
then
  echo ""
  echo "### ERROR: Please Change your /bin/sh symlink to point to bash. ### "
  echo ""
  echo "### sudo ln -sf /bin/bash /bin/sh ### "
  echo ""
  return 1
fi

# The SHELL variable also needs to be set to /bin/bash otherwise the build
# will fail, use chsh to change it to bash.
if [[ ! $SHELL =~ bash ]]
then
  echo ""
  echo "### ERROR: Please Change your shell to bash using chsh. ### "
  echo ""
  echo "### Make sure that the SHELL variable points to /bin/bash ### "
  echo ""
  return 1
fi

umask 022
unset DISTRO MACHINE PRODUCT VARIANT FACTORY

# OE doesn't want a set-gid directory for its tmpdir
BT="./build/tmp-glibc"
if [ ! -d ${BT} ]
then
  mkdir -m u=rwx,g=rx,g-s,o=  ${BT}
elif [ -g ${BT} ]
then
  chmod -R g-s ${BT}
fi
unset BT

# Find where the global conf directory is...
scriptdir="$(dirname "${BASH_SOURCE}")"
# Find where the workspace is...
WS=$(readlink -f $scriptdir/../../..)

# Add a few helpful shortcuts
# Go to root of workspace
alias croot='cd $WS'

# Go to the directory from where you can kick off the build(workspace/poky/build)
# from wherever you are
alias gobuilddir='CUR_DIR=`pwd` && cd $WS/poky/build'

# Go back to the directory you were working in before you ran gobuild
alias goback='cd $CUR_DIR'

#Build recipe X for target Y
function build_x_for_y() { gobuilddir && MACHINE=$2 rebake $1 && goback; }

#Go to OUT directory
alias goout='croot && cd poky/build/tmp-glibc/deploy/images/$MACHINE'


# Dynamically generate our bblayers.conf since we effectively can't whitelist
# BBLAYERS (by OE-Core class policy...Bitbake understands it...) to support
# dynamic workspace layer functionality.
python3 $scriptdir/get_bblayers.py ${WS}/poky \"meta*\" > $scriptdir/bblayers.conf

# 8009 commands
function setenv-8009-robot-image() {
  unset_bb_env
  export MACHINE=apq8009-robot
  export VARIANT=debug
}

function build-8009-robot-image() {
  setenv-8009-robot-image
  cdbitbake ${@} machine-robot-image
}

function build-8009-robot-facdev-image() {
  unset_bb_env
  export MACHINE=apq8009-robot
  export DISTRO=msm-perf
  export VARIANT=perf
  #export PRODUCT=robot
  export FACTORY="1"
  cdbitbake ${@} machine-robot-image
}

function build-8009-robot-perf-image() {
  unset_bb_env
  export MACHINE=apq8009-robot
  export DISTRO=msm-perf
  export VARIANT=perf
  export PRODUCT=robot
  cdbitbake ${@} machine-robot-image
}

function build-8009-robot-perf-cloudless-image() {
  unset_bb_env
  export MACHINE=apq8009-robot
  export DISTRO=msm-perf
  export VARIANT=perf
  export PRODUCT=robot
  export CLOUDLESS=1
  cdbitbake ${@} machine-robot-image
}

function build-8009-robot-oskr-image() {
  unset_bb_env
  export MACHINE=apq8009-robot
  export DISTRO=msm-perf
  export VARIANT=perf
  export PRODUCT=robot
  export OSKR=1
  cdbitbake ${@} machine-robot-image
}

function build-8009-robot-user-image() {
  unset_bb_env
  export MACHINE=apq8009-robot
  export DISTRO=msm-user
  export VARIANT=perf
  export PRODUCT=robot
  cdbitbake ${@} machine-robot-image
}

function build-8009-robot-userdev-image() {
  unset_bb_env
  export MACHINE=apq8009-robot
  export DISTRO=msm-user
  export VARIANT=perf
  #export PRODUCT=robot
  export DEV="1"
  cdbitbake ${@} machine-robot-image
}

function build-8009-robot-beta-image() {
  unset_bb_env
  export MACHINE=apq8009-robot
  export DISTRO=msm-user
  export VARIANT=perf
  #export PRODUCT=robot
  export DEV="1"
  export BETA="1"
  cdbitbake ${@} machine-robot-image
}

function build-8009-robot-factory-image() {
  unset_bb_env
  export MACHINE=apq8009-robot
  export DISTRO=msm-user
  export VARIANT=perf
  #export PRODUCT=robot
  export FACTORY="1"
  cdbitbake ${@} machine-robot-image
}

build-all-8009-robot-images() {
  build-8009-robot-image
  build-8009-robot-perf-image
}

function build-victor-robot-image() {
  build-8009-robot-image ${@}
}

function build-victor-robot-image-incremental() {
  setenv-8009-robot-image
  rebakeList=(anki-version machine-robot-image victor)
  cdbitbake -c cleanall ${rebakeList[@]}
  cdbitbake machine-robot-image
}

function build-victor-robot-perf-image() {
  build-8009-robot-perf-image ${@}
}

function build-victor-robot-oskr-image() {
  build-8009-robot-oskr-image ${@}
}

function build-victor-robot-user-image() {
  build-8009-robot-user-image ${@}
}

function build-victor-robot-factory-image() {
  build-8009-robot-factory-image ${@}
}

function build-victor-robot-facdev-image() {
  build-8009-robot-facdev-image ${@}
}

function build-victor-robot-userdev-image() {
  build-8009-robot-userdev-image ${@}
}

function build-victor-robot-beta-image() {
  build-8009-robot-beta-image ${@}
}

function build-oskr() {
  build-victor-robot-oskr-image ${@}
}

function build-dev() {
  build-victor-robot-perf-image ${@}
}

function build-devcloudless() {
  build-8009-robot-perf-cloudless-image ${@}
}

function build-prod() {
  build-victor-robot-user-image ${@}
}

# cleared every time
cleanList=(linux-vector-mainline victor wirebluetoothd vic-christen leproperties vic-cloud rampost anki-version machine-robot-image extra-conf vic-engine vic-robot update-os update-engine wireutils initscript-anki rebooter vic-anim vic-init vic-switchboard vic-gateway-cert base-files libpvictor fake-hwclock purplpkg)

function clean-oskr() {
  unset_bb_env
  export MACHINE=apq8009-robot
  export DISTRO=msm-perf
  export VARIANT=perf
  export PRODUCT=robot
  export OSKR=1
  wire-clean
  cdbitbake ${@} -c cleanall ${cleanList[@]}
}

function clean-dev() {
  unset_bb_env
  export MACHINE=apq8009-robot
  export DISTRO=msm-perf
  export VARIANT=perf
  export PRODUCT=robot
  wire-clean
  cdbitbake ${@} -c cleanall ${cleanList[@]}
}

function clean-devcloudless() {
  unset_bb_env
  export MACHINE=apq8009-robot
  export DISTRO=msm-perf
  export VARIANT=perf
  export PRODUCT=robot
  export CLOUDLESS=1
  wire-clean
  cdbitbake ${@} -c cleanall ${cleanList[@]} vic-cloudless
}

function clean-prod() {
  unset_bb_env
  export MACHINE=apq8009-robot
  export DISTRO=msm-user
  export VARIANT=perf
  export PRODUCT=robot
  wire-clean
  cdbitbake ${@} -c cleanall ${cleanList[@]}
}

function wire-clean() {
	if [[ -f ${WS}/wire-cleaning ]]; then
		echo "Cleaning file detected, cleaning: $(cat ${WS}/wire-cleaning)"
		cdbitbake -c cleanall ${@} $(cat ${WS}/wire-cleaning)
		rm -f ${WS}/wire-cleaning
	fi
	if [[ -d "${WS}/poky/build/downloads" ]]; then
		if [[ "$(ls ${WS}/poky/build/downloads)" == *'release.2.41.master'* ]]; then
			echo "Walnascar build detected - removing build folders..."
			sudo rm -rf ${WS}/poky/build/cache ${WS}/poky/build/sstate-cache ${WS}/poky/build/tmp-glibc ${WS}/poky/build/downloads
		fi
	fi
}

# Utility commands
buildclean() {
  set -x
  cd ${WS}/poky/build

  rm -rf bitbake.lock pseudodone sstate-cache tmp-glibc/* cache && cd - || cd -

  pushd ${WS}/anki/victor
  git clean -xdff .
  git submodule foreach --recursive 'git clean -dffx .'
  popd

  set +x
}


# Lists only those build commands that are:
#   * prefixed with function keyword
#   * name starts with build-victor

list-build-commands()
{
    echo
    echo "Convenience commands for building Victor images:"
    echo "  build-dev"
    echo "  build-oskr"
    echo "  build-devcloudless"
    echo "  build-prod"
    echo
    echo "Use 'list-build-commands' to see this list again."
    echo
}

cdbitbake() {
  local ret=0
  cd ${WS}/poky/build
  bitbake $@ && cd - || ret=$? && cd -
  return $ret
}

rebake() {
  cdbitbake -c cleanall $@ && \
  cdbitbake $@
}

unset_bb_env() {
  unset DISTRO MACHINE PRODUCT VARIANT FACTORY DEV OSKR BETA ANKI_AMAZON_ENDPOINTS_ENABLED CLOUDLESS
}

# Find build templates from qti meta layer.
export TEMPLATECONF="${WS}/poky/victor/meta-qcom/conf/templates/msm"

# Yocto/OE-core works a bit differently than OE-classic so we're
# going to source the OE build environment setup script they provided.
# This will dump the user in ${WS}/yocto/build, ready to run the
# convienence function or straight up bitbake commands.
. ${WS}/poky/openembedded-core/oe-init-build-env

# Let bitbake use the following env-vars as if they were pre-set bitbake ones.
# (BBLAYERS is explicitly blocked from this within OE-Core itself, though...)
# oe-init-build-env calls oe-buildenv-internal which sets
# BB_ENV_EXTRAWHITE, append our vars to the list
export BB_ENV_PASSTHROUGH_ADDITIONS="${BB_ENV_PASSTHROUGH_ADDITIONS} DL_DIR PRODUCT VARIANT FACTORY DEV OSKR QSN BETA ANKI_AMAZON_ENDPOINTS_ENABLED ANKI_BUILD_VERSION AUTO_UPDATE CLOUDLESS"

list-build-commands
