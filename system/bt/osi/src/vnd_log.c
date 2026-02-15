/******************************************************************************
Copyright (c) 2016, The Linux Foundation. All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above
      copyright notice, this list of conditions and the following
      disclaimer in the documentation and/or other materials provided
      with the distribution.
    * Neither the name of The Linux Foundation nor the names of its
      contributors may be used to endorse or promote products derived
      from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
******************************************************************************/

#define LOG_TAG "bt_vnd_log"

#include <errno.h>
#include <string.h>
#include <dlfcn.h>
#include "osi/include/log.h"
#include "osi/include/osi.h"

static const char *LOGGER_LIBRARY_NAME = "libbt-logClient.so";
static const char *LOGGER_LIBRARY_SYMBOL_NAME = "BLUETOOTH_LOGGER_LIB_INTERFACE";

static void *lib_handle;
bt_logger_interface_t *logger_interface = NULL;
bool bt_logger_enabled = false;

void init_vnd_Logger(void)
{
  if(!bt_logger_enabled)
  {
    LOG_ERROR(LOG_TAG, "%s, Logger Not enabled from config file",  __func__);
    return;
  }

  if(logger_interface)
  {
    LOG_ERROR(LOG_TAG, "%s, Vendor Logger is already initialized",  __func__);
    return;
  }

  lib_handle = dlopen(LOGGER_LIBRARY_NAME, RTLD_NOW);

  if (!lib_handle) {
    LOG_ERROR(LOG_TAG, "%s unable to open %s: %s", __func__, LOGGER_LIBRARY_NAME, dlerror());
    return;
  }

  logger_interface = (bt_logger_interface_t *)dlsym(lib_handle, LOGGER_LIBRARY_SYMBOL_NAME);
  if (!logger_interface) {
    LOG_ERROR(LOG_TAG, "%s unable to find symbol %s in %s: %s", __func__, LOGGER_LIBRARY_SYMBOL_NAME, LOGGER_LIBRARY_NAME, dlerror());
    return;
  }

  logger_interface->init();
}

void clean_vnd_logger()
{
  if(!bt_logger_enabled)
    return;

  if(logger_interface)
    logger_interface->cleanup();

  logger_interface = NULL;

  if(lib_handle)
    dlclose(lib_handle);

  lib_handle = NULL;
}
