#!/bin/bash

# This script will recursively search all |FILES| from the current
# directory and replace all |TYPES| according to the list below.

# NOTE 1:
# If this script is run from .../system/bt (as it's intended to be),
# please edit stack/include/bt_types.h next and remove the typedef's
# near the top and restore the definitions of TRUE and FALSE. These
# are still used in the vnd_* files and device specific repositories.

# NOTE 2:
# The list of files to be modified also includes "*.patch", which means
# this script can be used to help cherry-picking changes from older
# branches. Follow this workflow outline:
#  1. git format-patch [-1] <your sha1>
#  2. Run change_type script on patch[es]
#  3. git apply / git am


# Regular expression matching the file name
FILES="\.h$|\.c$|\.cpp$|\.cc$|\.patch$"

# Search/replace terms, separated by ":"
TYPES=(
  "UINT8:uint8_t"
  "UINT16:uint16_t"
  "UINT32:uint32_t"
  "UINT64:uint64_t"
  "INT8:int8_t"
  "INT16:int16_t"
  "INT32:int32_t"
  "INT64:int64_t"
  "BOOLEAN:bool"
  "TRUE:true"
  "FALSE:false"
  "__FUNCTION__:__func__"
)

function process_file
{
  echo -n "Processing file $1 "

  for tt in "${TYPES[@]}" ;
  do
    before=${tt%%:*}
    after=${tt#*:}

    echo -n "."
    sed -i -e "s/\b${before}\b/${after}/g" "$1"
  done
  echo
}

function process_files
{
  until [ -z "$1" ]
  do
    process_file "$1"
    shift
  done
}


# Let's do this ...
process_files `find ./ | grep -E "${FILES}"`

# All done ...
echo
echo "All done."

# Try to be helpful ...
PWD=`pwd`
if [[ "${PWD}" == */system/bt ]]
then
  echo "Please edit ${PWD}/stack/include/bt_types.h next."
fi
