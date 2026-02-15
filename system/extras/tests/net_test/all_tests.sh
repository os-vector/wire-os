#!/bin/bash

# Copyright 2014 The Android Open Source Project
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

readonly PREFIX="#####"

function maybePlural() {
  # $1 = integer to use for plural check
  # $2 = singular string
  # $3 = plural string
  if [ $1 -ne 1 ]; then
    echo "$3"
  else
    echo "$2"
  fi
}


readonly tests=$(find . -name '*_test.py' -type f -executable)
readonly count=$(echo $tests | wc -w)
echo "$PREFIX Found $count $(maybePlural $count test tests)."

exit_code=0

i=0
for test in $tests; do
  i=$((i + 1))
  echo ""
  echo "$PREFIX $test ($i/$count)"
  echo ""
  $test || exit_code=$(( exit_code + 1 ))
  echo ""
done

echo "$PREFIX $exit_code failed $(maybePlural $exit_code test tests)."
exit $exit_code
