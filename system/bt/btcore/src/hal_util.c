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

#define LOG_TAG "hal_util"

#include <hardware/bluetooth.h>
#include <hardware/hardware.h>

#include <dlfcn.h>
#include <errno.h>
#include <string.h>

#include "btcore/include/hal_util.h"
#include "osi/include/log.h"

#if defined(OS_GENERIC)

// TODO(armansito): All logging macros should include __func__ by default (see
// Bug: 22671731)
#define HULOGERR(fmt, args...) \
    LOG_ERROR(LOG_TAG, "[%s] failed to load the Bluetooth library: " fmt, \
              __func__, ## args)

// TODO(armansito): It might be better to pass the library name in a more
// generic manner as opposed to hard-coding it here.
static const char kBluetoothLibraryName[] = "libbluetooth.default.so";

static int load_bt_library(const struct hw_module_t **module) {
  const char *id = BT_STACK_MODULE_ID;

  // Always try to load the default Bluetooth stack on GN builds.
  void *handle = dlopen(kBluetoothLibraryName, RTLD_NOW);
  if (!handle) {
    char const *err_str = dlerror();
    HULOGERR("%s", err_str ? err_str : "error unknown");
    goto error;
  }

  // Get the address of the struct hal_module_info.
  const char *sym = HAL_MODULE_INFO_SYM_AS_STR;
  struct hw_module_t *hmi = (struct hw_module_t *)dlsym(handle, sym);
  if (!hmi) {
    HULOGERR("%s", sym);
    goto error;
  }

  // Check that the id matches.
  if (strcmp(id, hmi->id) != 0) {
    HULOGERR("id=%s does not match HAL module ID: %s", id, hmi->id);
    goto error;
  }

  hmi->dso = handle;

  // Success.
  LOG_INFO(
      LOG_TAG, "[%s] loaded HAL id=%s path=%s hmi=%p handle=%p",
      __func__, id, kBluetoothLibraryName, hmi, handle);

  *module = hmi;
  return 0;

error:
  *module = NULL;
  if (handle)
    dlclose(handle);

  return -EINVAL;
}

#endif  // defined(OS_GENERIC)

int hal_util_load_bt_library(const struct hw_module_t **module) {
#if defined(OS_GENERIC)
  return load_bt_library(module);
#else  // !defined(OS_GENERIC)
  return hw_get_module(BT_STACK_MODULE_ID, module);
#endif  // defined(OS_GENERIC)
}
