/******************************************************************************
 *
 *  Copyright (C) 2016 Google, Inc.
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

#include <string.h>

#include "osi/include/properties.h"

int osi_property_get(const char *key, char *value, const char *default_value) {
#if defined(OS_GENERIC)
    /* For linux right now just return default value, if present */
    int len = -1;
    if (!default_value)
      return len;

    len = strlen(default_value);
    if (len >= PROPERTY_VALUE_MAX)
      len = PROPERTY_VALUE_MAX - 1;

    memcpy(value, default_value, len);
    value[len] = '\0';
    return len;
#else
#ifdef ANDROID
    return property_get(key, value, default_value);
#else
    return property_get_bt(key, value, default_value);
#endif
#endif  // defined(OS_GENERIC)
}

int osi_property_set(const char *key, const char *value) {
#if defined(OS_GENERIC)
    return -1;
#else
#ifdef ANDROID
    return property_set(key, value);
#else
    return property_set_bt(key, value);
#endif
#endif  // defined(OS_GENERIC)
}
