//
//  Copyright (C) 2015 Google, Inc.
//
//  Licensed under the Apache License, Version 2.0 (the "License");
//  you may not use this file except in compliance with the License.
//  You may obtain a copy of the License at:
//
//  http://www.apache.org/licenses/LICENSE-2.0
//
//  Unless required by applicable law or agreed to in writing, software
//  distributed under the License is distributed on an "AS IS" BASIS,
//  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//  See the License for the specific language governing permissions and
//  limitations under the License.
//

extern "C" {

// TODO(armansito): This provides a fake implementation of the function defined
// in btcore/include/hal_util.h. We have to provide this to prevent having to
// pull in libbtcore and transitively libhardware as dependencies for the unit
// tests. Instead of doing it this way, however, we should instead provide a C++
// class abstraction for this (and all other btif interfaces) that we can mock
// for testing.
int hal_util_load_bt_library(const struct hw_module_t** module) {
  return -1;
}

}  // extern "C"
