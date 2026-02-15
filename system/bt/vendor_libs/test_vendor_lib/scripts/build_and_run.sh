#!/bin/bash

#
# Copyright 2015 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

# Builds and pushes the test vendor library to a connected device and starts
# logcat with project-specific tag filters. If the --test-channel flag is set,
# logcat is started in a separate process and the test channel is run in the
# current shell. The kTestChannelEnabled flag must be set in the vendor manager
# if the test channel is to be used. Also ensure that 'lunch' has been run for
# the appropriate device.

if [[ "$#" -ne 2 && "$#" -ne 4 ]]; then
  echo "Usage:"
  echo "./build_and_run.sh [/path/to/aosp] [device_name] or"
  echo "./build_and_run.sh [/path/to/aosp] [device_name] --test-channel [port]"
  exit 1
fi

# Exit the script if any command fails.
set -e

# The home directory for AOSP.
AOSP_ABS=$1
# The name of the device to build for.
DEVICE=$2

# The location of Bluetooth within AOSP.
BT_REL=/system/bt
BT_ABS=${AOSP_ABS}${BT_REL}

# The location of the test vendor library.
TEST_VENDOR_LIB_REL=/vendor_libs/test_vendor_lib
TEST_VENDOR_LIB_ABS=${BT_ABS}${TEST_VENDOR_LIB_REL}

DEVICE_TARGET_REL=/out/target/product
DEVICE_TARGET_ABS=${AOSP_ABS}${DEVICE_TARGET_REL}

VENDOR_SYMBOLS_REL=/symbols/system/vendor/lib
VENDOR_SYMBOLS_ABS=${DEVICE_TARGET_ABS}/${DEVICE}/${VENDOR_SYMBOLS_REL}

# The name of the object built by the test vendor library.
TEST_VENDOR_LIB=test-vendor.so
# The name of the regular vendor object to be replaced by $TEST_VENDOR_LIB.
VENDOR_LIB=libbt-vendor.so
# The config file specifying controller properties.
CONTROLLER_PROPERTIES=controller_properties.json

if [[ "$#" -eq 4 && $3 == "--test-channel" ]]; then
  TEST_CHANNEL_PORT=$4
  TEST_CHANNEL_REL=/scripts
  TEST_CHANNEL_ABS=${TEST_VENDOR_LIB_ABS}${TEST_CHANNEL_REL}

  # Start logcat in a subshell.
  x-terminal-emulator -e "scripts/build_and_run.sh ${AOSP_ABS} ${DEVICE}"

  echo "Setting up build environment."
  cd ${AOSP_ABS}
  source build/envsetup.sh

  # Forward local port to the same port on the device.
  echo "Forwarding port ${TEST_CHANNEL_PORT} to device."
  adb forward tcp:${TEST_CHANNEL_PORT} tcp:${TEST_CHANNEL_PORT}

  # Turn Bluetooth on. Requires user approval via a dialog on the device.
  echo "Enabling Bluetooth. Please see dialog on device."
  adb shell am start -a android.bluetooth.adapter.action.REQUEST_ENABLE

  # Start the test channel once Bluetooth is on and logcat has started.
  read -p "Press [ENTER] once Bluetooth is enabling AND logcat has started."

  # Start the test channel.
  python ${TEST_CHANNEL_ABS}/test_channel.py localhost ${TEST_CHANNEL_PORT}
else
  echo "Setting up build environment."
  cd ${AOSP_ABS}
  source build/envsetup.sh

  echo "Navigating to test vendor library: ${TEST_VENDOR_LIB_ABS}"
  cd ${TEST_VENDOR_LIB_ABS}

  echo "Building test vendor library."
  mm

  echo "Remounting device rootfs."
  adb shell mount -o remount,rw /
  adb remount

  # Replace the actual vendor library with the test vendor library.
  mv ${DEVICE_TARGET_ABS}/${DEVICE}/system/lib/${TEST_VENDOR_LIB} \
    ${VENDOR_SYMBOLS_ABS}/${VENDOR_LIB}

  # Push the test vendor library to the device.
  echo "Pushing the test vendor library to device: $DEVICE"
  adb push ${VENDOR_SYMBOLS_ABS}/${VENDOR_LIB} /vendor/lib

  echo "Pushing controller properties."
  adb push ${TEST_VENDOR_LIB_ABS}/data/${CONTROLLER_PROPERTIES} /etc/bluetooth/

  echo "Pushing libevent."
  adb push ${DEVICE_TARGET_ABS}/${DEVICE}/system/lib/libevent.so /system/lib/

  echo "Pushing libchrome."
  adb push ${DEVICE_TARGET_ABS}/${DEVICE}/system/lib/libchrome.so /system/lib/

  # Clear logcat.
  adb logcat -c

  # Run logcat with filters.
  adb logcat bt_btif:D bt_btif_core:D bt_hci:D bt_main:D bt_vendor:D \
   bte_logmsg:D command_packet:D dual_mode_controller:D event_packet:D \
   hci_transport:D hci_handler:D packet:D packet_stream:D \
   test_channel_transport:D vendor_manager:D *:S
fi
