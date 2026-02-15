/******************************************************************************
 *
 *  Copyright (C) 2014 Google, Inc.
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at:
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 ******************************************************************************/

#define LOG_TAG "bt_btif_config"

#include "btif_config.h"

#include <assert.h>
#include <ctype.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "bt_types.h"
#include "btcore/include/bdaddr.h"
#include "btcore/include/module.h"
#include "btif_api.h"
#include "btif_common.h"
#include "btif_config.h"
#include "btif_config_transcode.h"
#include "btif_util.h"
#include "osi/include/alarm.h"
#include "osi/include/allocator.h"
#include "osi/include/compat.h"
#include "osi/include/config.h"
#include "osi/include/log.h"
#include "osi/include/osi.h"

/**
 * TODO(apanicke): cutils/properties.h is only being used to pull-in runtime
 * settings on Android. Remove this conditional include once we have a generic
 * way to obtain system properties.
 */
#if !defined(OS_GENERIC)
#include <cutils/properties.h>
#include <stdlib.h>
#endif  /* !defined(OS_GENERIC) */

#define BT_CONFIG_SOURCE_TAG_NUM 1010001

#define INFO_SECTION "Info"
#define FILE_TIMESTAMP "TimeCreated"
#define FILE_SOURCE "FileSource"
#define TIME_STRING_LENGTH sizeof("YYYY-MM-DD HH:MM:SS")
static const char* TIME_STRING_FORMAT = "%Y-%m-%d %H:%M:%S";

// TODO(armansito): Find a better way than searching by a hardcoded path.
#if defined(OS_GENERIC)
static const char *CONFIG_FILE_PATH = "bt_config.conf";
static const char *CONFIG_BACKUP_PATH = "bt_config.bak";
static const char *CONFIG_LEGACY_FILE_PATH = "bt_config.xml";
#elif ANDROID
static const char *CONFIG_FILE_PATH = "/data/misc/bluedroid/bt_config.conf";
static const char *CONFIG_BACKUP_PATH = "/data/misc/bluedroid/bt_config.bak";
static const char *CONFIG_LEGACY_FILE_PATH = "/data/misc/bluedroid/bt_config.xml";
#else
static const char *CONFIG_FILE_PATH = "/data/misc/bluetooth/bt_config.conf";
static const char *CONFIG_BACKUP_PATH = "/data/misc/bluetooth/bt_config.bak";
static const char *CONFIG_LEGACY_FILE_PATH = "/data/misc/bluetooth/bt_config.xml";
#endif  // defined(OS_GENERIC)

static const period_ms_t CONFIG_SETTLE_PERIOD_MS = 3000;

static void timer_config_save_cb(void *data);
static void btif_config_write(UINT16 event, char *p_param);
static bool is_factory_reset(void);
static void delete_config_files(void);
static void btif_config_remove_unpaired(config_t *config);
static void btif_config_remove_restricted(config_t *config);
static config_t *btif_config_open(const char* filename);

static enum ConfigSource {
  NOT_LOADED,
  ORIGINAL,
  BACKUP,
  LEGACY,
  NEW_FILE,
  RESET
} btif_config_source = NOT_LOADED;

static int btif_config_devices_loaded = -1;
static char btif_config_time_created[TIME_STRING_LENGTH];

// TODO(zachoverflow): Move these two functions out, because they are too specific for this file
// {grumpy-cat/no, monty-python/you-make-me-sad}
bool btif_get_device_type(const BD_ADDR bd_addr, int *p_device_type)
{
    if (p_device_type == NULL)
        return FALSE;

    bt_bdaddr_t bda;
    bdcpy(bda.address, bd_addr);

    bdstr_t bd_addr_str;
    bdaddr_to_string(&bda, bd_addr_str, sizeof(bd_addr_str));

    if (!btif_config_get_int(bd_addr_str, "DevType", p_device_type))
        return FALSE;

    LOG_DEBUG(LOG_TAG, "%s: Device [%s] type %d", __FUNCTION__, bd_addr_str, *p_device_type);
    return TRUE;
}

bool btif_get_address_type(const BD_ADDR bd_addr, int *p_addr_type)
{
    if (p_addr_type == NULL)
        return FALSE;

    bt_bdaddr_t bda;
    bdcpy(bda.address, bd_addr);

    bdstr_t bd_addr_str;
    bdaddr_to_string(&bda, bd_addr_str, sizeof(bd_addr_str));

    if (!btif_config_get_int(bd_addr_str, "AddrType", p_addr_type))
        return FALSE;

    LOG_DEBUG(LOG_TAG, "%s: Device [%s] address type %d", __FUNCTION__, bd_addr_str, *p_addr_type);
    return TRUE;
}

static pthread_mutex_t lock;  // protects operations on |config|.
static config_t *config;
static alarm_t *config_timer;

// Module lifecycle functions

static future_t *init(void) {
  pthread_mutex_init(&lock, NULL);
  pthread_mutex_lock(&lock);

  if (is_factory_reset())
    delete_config_files();

  const char *file_source = NULL;

  config = btif_config_open(CONFIG_FILE_PATH);
  btif_config_source = ORIGINAL;
  if (!config) {
    LOG_WARN("%s unable to load config file: %s; using backup.",
              __func__, CONFIG_FILE_PATH);
    config = btif_config_open(CONFIG_BACKUP_PATH);
    btif_config_source = BACKUP;
    file_source = "Backup";
  }
#ifdef USE_TINYXML2
  if (!config) {
    LOG_WARN("%s unable to load backup; attempting to transcode legacy file.", __func__);
    config = btif_config_transcode(CONFIG_LEGACY_FILE_PATH);
    btif_config_source = LEGACY;
    file_source = "Legacy";
  }
#endif
  if (!config) {
    LOG_ERROR("%s unable to transcode legacy file; creating empty config.", __func__);
    config = config_new_empty();
    btif_config_source = NEW_FILE;
    file_source = "Empty";
  }

  if (file_source != NULL)
    config_set_string(config, INFO_SECTION, FILE_SOURCE, file_source);

  if (!config) {
    LOG_ERROR("%s unable to allocate a config object.", __func__);
    goto error;
  }

  btif_config_remove_unpaired(config);

  // Cleanup temporary pairings if we have left guest mode
  if (!is_restricted_mode())
    btif_config_remove_restricted(config);

  // Read or set config file creation timestamp
  const char* time_str = config_get_string(config, INFO_SECTION, FILE_TIMESTAMP, NULL);
  if (time_str != NULL) {
    strlcpy(btif_config_time_created, time_str, TIME_STRING_LENGTH);
  } else {
    time_t current_time = time(NULL);
    struct tm* time_created = localtime(&current_time);
    if (time_created) {
      strftime(btif_config_time_created, TIME_STRING_LENGTH, TIME_STRING_FORMAT, time_created);
      config_set_string(config, INFO_SECTION, FILE_TIMESTAMP, btif_config_time_created);
    }
  }

  // Cleanup temporary pairings if we have left guest mode
  if (!is_restricted_mode())
    btif_config_remove_restricted(config);

  // TODO(sharvil): use a non-wake alarm for this once we have
  // API support for it. There's no need to wake the system to
  // write back to disk.
  config_timer = alarm_new("btif.config");
  if (!config_timer) {
    LOG_ERROR(LOG_TAG, "%s unable to create alarm.", __func__);
    goto error;
  }

  LOG_EVENT_INT(BT_CONFIG_SOURCE_TAG_NUM, btif_config_source);

  pthread_mutex_unlock(&lock);
  return future_new_immediate(FUTURE_SUCCESS);

error:
  alarm_free(config_timer);
  config_free(config);
  pthread_mutex_unlock(&lock);
  pthread_mutex_destroy(&lock);
  config_timer = NULL;
  config = NULL;
  btif_config_source = NOT_LOADED;
  return future_new_immediate(FUTURE_FAIL);
}

static config_t *btif_config_open(const char *filename) {
  config_t *config = config_new(filename);
  if (!config)
    return NULL;

  if (!config_has_section(config, "Adapter")) {
    LOG_ERROR(LOG_TAG, "Config is missing adapter section");
    config_free(config);
    return NULL;
  }

  return config;
}

static future_t *shut_down(void) {
  btif_config_flush();
  return future_new_immediate(FUTURE_SUCCESS);
}

static future_t *clean_up(void) {
  btif_config_flush();

  alarm_free(config_timer);
  config_free(config);
  pthread_mutex_destroy(&lock);
  config_timer = NULL;
  config = NULL;
  return future_new_immediate(FUTURE_SUCCESS);
}

//TODO: Fix this
#ifndef ANDROID
#define EXPORT_SYMBOL   __attribute__((visibility("default")))
#endif

EXPORT_SYMBOL const module_t btif_config_module = {
  .name = BTIF_CONFIG_MODULE,
  .init = init,
  .start_up = NULL,
  .shut_down = shut_down,
  .clean_up = clean_up,
  .dependencies = {
    NULL
  }
};

bool btif_config_has_section(const char *section) {
  assert(config != NULL);
  assert(section != NULL);

  pthread_mutex_lock(&lock);
  bool ret = config_has_section(config, section);
  pthread_mutex_unlock(&lock);

  return ret;
}

bool btif_config_exist(const char *section, const char *key) {
  assert(config != NULL);
  assert(section != NULL);
  assert(key != NULL);

  pthread_mutex_lock(&lock);
  bool ret = config_has_key(config, section, key);
  pthread_mutex_unlock(&lock);

  return ret;
}

bool btif_config_get_int(const char *section, const char *key, int *value) {
  assert(config != NULL);
  assert(section != NULL);
  assert(key != NULL);
  assert(value != NULL);

  pthread_mutex_lock(&lock);
  bool ret = config_has_key(config, section, key);
  if (ret)
    *value = config_get_int(config, section, key, *value);
  pthread_mutex_unlock(&lock);

  return ret;
}

bool btif_config_set_int(const char *section, const char *key, int value) {
  assert(config != NULL);
  assert(section != NULL);
  assert(key != NULL);

  pthread_mutex_lock(&lock);
  config_set_int(config, section, key, value);
  pthread_mutex_unlock(&lock);

  return true;
}

bool btif_config_get_str(const char *section, const char *key, char *value, int *size_bytes) {
  assert(config != NULL);
  assert(section != NULL);
  assert(key != NULL);
  assert(value != NULL);
  assert(size_bytes != NULL);

  pthread_mutex_lock(&lock);
  const char *stored_value = config_get_string(config, section, key, NULL);
  pthread_mutex_unlock(&lock);

  if (!stored_value)
    return false;

  strlcpy(value, stored_value, *size_bytes);
  *size_bytes = strlen(value) + 1;

  return true;
}

bool btif_config_set_str(const char *section, const char *key, const char *value) {
  assert(config != NULL);
  assert(section != NULL);
  assert(key != NULL);
  assert(value != NULL);

  pthread_mutex_lock(&lock);
  config_set_string(config, section, key, value);
  pthread_mutex_unlock(&lock);

  return true;
}

bool btif_config_get_bin(const char *section, const char *key, uint8_t *value, size_t *length) {
  assert(config != NULL);
  assert(section != NULL);
  assert(key != NULL);
  assert(value != NULL);
  assert(length != NULL);

  pthread_mutex_lock(&lock);
  const char *value_str = config_get_string(config, section, key, NULL);
  pthread_mutex_unlock(&lock);

  if (!value_str)
    return false;

  size_t value_len = strlen(value_str);
  if ((value_len % 2) != 0 || *length < (value_len / 2))
    return false;

  for (size_t i = 0; i < value_len; ++i)
    if (!isxdigit(value_str[i]))
      return false;

  for (*length = 0; *value_str; value_str += 2, *length += 1)
    sscanf(value_str, "%02hhx", &value[*length]);

  return true;
}

size_t btif_config_get_bin_length(const char *section, const char *key) {
  assert(config != NULL);
  assert(section != NULL);
  assert(key != NULL);

  pthread_mutex_lock(&lock);
  const char *value_str = config_get_string(config, section, key, NULL);
  pthread_mutex_unlock(&lock);

  if (!value_str)
    return 0;

  size_t value_len = strlen(value_str);
  return ((value_len % 2) != 0) ? 0 : (value_len / 2);
}

bool btif_config_set_bin(const char *section, const char *key, const uint8_t *value, size_t length) {
  const char *lookup = "0123456789abcdef";

  assert(config != NULL);
  assert(section != NULL);
  assert(key != NULL);

  if (length > 0)
      assert(value != NULL);

  char *str = (char *)osi_calloc(length * 2 + 1);

  for (size_t i = 0; i < length; ++i) {
    str[(i * 2) + 0] = lookup[(value[i] >> 4) & 0x0F];
    str[(i * 2) + 1] = lookup[value[i] & 0x0F];
  }

  pthread_mutex_lock(&lock);
  config_set_string(config, section, key, str);
  pthread_mutex_unlock(&lock);

  osi_free(str);
  return true;
}

const btif_config_section_iter_t *btif_config_section_begin(void) {
  assert(config != NULL);
  return (const btif_config_section_iter_t *)config_section_begin(config);
}

const btif_config_section_iter_t *btif_config_section_end(void) {
  assert(config != NULL);
  return (const btif_config_section_iter_t *)config_section_end(config);
}

const btif_config_section_iter_t *btif_config_section_next(const btif_config_section_iter_t *section) {
  assert(config != NULL);
  assert(section != NULL);
  return (const btif_config_section_iter_t *)config_section_next((const config_section_node_t *)section);
}

const char *btif_config_section_name(const btif_config_section_iter_t *section) {
  assert(config != NULL);
  assert(section != NULL);
  return config_section_name((const config_section_node_t *)section);
}

bool btif_config_remove(const char *section, const char *key) {
  assert(config != NULL);
  assert(section != NULL);
  assert(key != NULL);

  pthread_mutex_lock(&lock);
  bool ret = config_remove_key(config, section, key);
  pthread_mutex_unlock(&lock);

  return ret;
}

void btif_config_save(void) {
  assert(config != NULL);
  assert(config_timer != NULL);

  alarm_set(config_timer, CONFIG_SETTLE_PERIOD_MS, timer_config_save_cb, NULL);
}

void btif_config_flush(void) {
  assert(config != NULL);
  assert(config_timer != NULL);

  alarm_cancel(config_timer);
  btif_config_write(0, NULL);
}

bool btif_config_clear(void) {
  assert(config != NULL);
  assert(config_timer != NULL);

  alarm_cancel(config_timer);

  pthread_mutex_lock(&lock);
  config_free(config);

  config = config_new_empty();
  if (config == NULL) {
    pthread_mutex_unlock(&lock);
    return false;
  }

  bool ret = config_save(config, CONFIG_FILE_PATH);
  btif_config_source = RESET;
  pthread_mutex_unlock(&lock);
  return ret;
}

static void timer_config_save_cb(UNUSED_ATTR void *data) {
  // Moving file I/O to btif context instead of timer callback because
  // it usually takes a lot of time to be completed, introducing
  // delays during A2DP playback causing blips or choppiness.
  btif_transfer_context(btif_config_write, 0, NULL, 0, NULL);
}

static void btif_config_write(UNUSED_ATTR UINT16 event, UNUSED_ATTR char *p_param) {
  assert(config != NULL);
  assert(config_timer != NULL);

  pthread_mutex_lock(&lock);
  rename(CONFIG_FILE_PATH, CONFIG_BACKUP_PATH);
  config_t *config_paired = config_new_clone(config);
  btif_config_remove_unpaired(config_paired);
  config_save(config_paired, CONFIG_FILE_PATH);
  config_free(config_paired);
  pthread_mutex_unlock(&lock);
}

static void btif_config_remove_unpaired(config_t *conf) {
  assert(conf != NULL);
  int paired_devices = 0;

  // The paired config used to carry information about
  // discovered devices during regular inquiry scans.
  // We remove these now and cache them in memory instead.
  const config_section_node_t *snode = config_section_begin(conf);
  while (snode != config_section_end(conf)) {
    const char *section = config_section_name(snode);
    if (string_is_bdaddr(section)) {
      if (!config_has_key(conf, section, "LinkKey") &&
          !config_has_key(conf, section, "LE_KEY_PENC") &&
          !config_has_key(conf, section, "LE_KEY_PID") &&
          !config_has_key(conf, section, "LE_KEY_PCSRK") &&
          !config_has_key(conf, section, "LE_KEY_LENC") &&
          !config_has_key(conf, section, "LE_KEY_LCSRK")) {
        snode = config_section_next(snode);
        config_remove_section(conf, section);
        continue;
      }
      paired_devices++;
    }
    snode = config_section_next(snode);
  }

  // should only happen once, at initial load time
  if (btif_config_devices_loaded == -1)
    btif_config_devices_loaded = paired_devices;
}

void btif_debug_config_dump(int fd) {
    dprintf(fd, "\nBluetooth Config:\n");

    dprintf(fd, "  Config Source: ");
    switch(btif_config_source) {
        case NOT_LOADED:
            dprintf(fd, "Not loaded\n");
            break;
        case ORIGINAL:
            dprintf(fd, "Original file\n");
            break;
        case BACKUP:
            dprintf(fd, "Backup file\n");
            break;
        case LEGACY:
            dprintf(fd, "Legacy file\n");
            break;
        case NEW_FILE:
            dprintf(fd, "New file\n");
            break;
        case RESET:
            dprintf(fd, "Reset file\n");
            break;
    }

    dprintf(fd, "  Devices loaded: %d\n", btif_config_devices_loaded);
    dprintf(fd, "  File created/tagged: %s\n", btif_config_time_created);
    dprintf(fd, "  File source: %s\n", config_get_string(config, INFO_SECTION,
                                           FILE_SOURCE, "Original"));
}

static void btif_config_remove_restricted(config_t* config) {
  assert(config != NULL);

  const config_section_node_t *snode = config_section_begin(config);
  while (snode != config_section_end(config)) {
    const char *section = config_section_name(snode);
    if (string_is_bdaddr(section) && config_has_key(config, section, "Restricted")) {
        BTIF_TRACE_DEBUG("%s: Removing restricted device %s", __func__, section);
        config_remove_section(config, section);
    }
    snode = config_section_next(snode);
  }
}

static bool is_factory_reset(void) {
  char factory_reset[PROPERTY_VALUE_MAX] = {0};
#ifdef ANDROID
  property_get("persist.bluetooth.factoryreset", factory_reset, "false");
#else
  property_get_bt("persist.bluetooth.factoryreset", factory_reset, "false");
#endif
  return strncmp(factory_reset, "true", 4) == 0;
}

static void delete_config_files(void) {
  remove(CONFIG_FILE_PATH);
  remove(CONFIG_BACKUP_PATH);
#ifdef ANDROID
  property_set("persist.bluetooth.factoryreset", "false");
#else
  property_set_bt("persist.bluetooth.factoryreset", "false");
#endif
}
