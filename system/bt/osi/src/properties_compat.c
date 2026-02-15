/**
 * File: properties_compat.c
 *
 * Author: seichert
 * Created: 3/19/2018
 *
 * Description: fill in functions for properties (cutils)
 *
 * Copyright: Anki, Inc. 2018
 *
 **/


#include <cutils/properties.h>
#include <stdlib.h>

int32_t property_get_int32(const char *key, int32_t default_value)
{
  if (!key) {
    return default_value;
  }

  int32_t result = default_value;
  char buf[PROPERTY_VALUE_MAX] = {0};
  int len = property_get(key, buf, "");

  if (len > 0) {
    result = atoi(buf);
  }

  return result;
}
