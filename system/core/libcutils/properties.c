/*
 * Copyright (C) 2006 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <cutils/properties.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "property_ops.h"

static void dump_persist(void);

int property_get(const char *key, char *value, const char *default_value) {
    int rc = 0;
#ifdef LE_PROPERTIES
    if(get_property_value(key,value) == true)
        rc = strlen(value);
    if( rc > 0) {
      return rc;
    }
#endif
    if (NULL != default_value) {
        rc = sprintf(value, "%.*s", PROPERTY_VALUE_MAX - 1, default_value);
    }
    return rc;
}

int property_set(const char *key, const char *value)
{
#ifdef LE_PROPERTIES
    char prop_name[PROP_NAME_MAX];
    char prop_value[PROP_VALUE_MAX];

    if (key == 0) return -1;
    if (value == 0) value = "";
    if (strlen(key) >= PROP_NAME_MAX) return -1;
    if (strlen(value) >= PROP_VALUE_MAX) return -1;

    memset(prop_name, 0, sizeof prop_name);
    memset(prop_value, 0, sizeof prop_value);

    strlcpy(prop_name, key, sizeof prop_name);
    strlcpy(prop_value, value, sizeof prop_value);

    set_property_value(prop_name, prop_value);
#endif
    return 0;
}

int8_t property_get_bool(const char *key, int8_t default_value) {

    if (!key) {
        return default_value;
    }

    int8_t result = default_value;
    char buf[PROPERTY_VALUE_MAX] = {'\0',};

    int len = property_get(key, buf, "");
    if (len == 1) {
        char ch = buf[0];
        if (ch == '0' || ch == 'n') {
            result = false;
        } else if (ch == '1' || ch == 'y') {
            result = true;
        }
    } else if (len > 1) {
         if (!strcmp(buf, "no") || !strcmp(buf, "false") || !strcmp(buf, "off")) {
            result = false;
        } else if (!strcmp(buf, "yes") || !strcmp(buf, "true") || !strcmp(buf, "on")) {
            result = true;
        }
    }

    return result;

}

void dump_properties(void) {
#ifdef LE_PROPERTIES
    dump_persist();
#endif
}

