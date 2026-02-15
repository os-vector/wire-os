/******************************************************************************
 *
 *  Copyright (C) 2015 Google, Inc.
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

#define LOG_TAG "bt_osi_time"

#include <time.h>

#include "osi/include/time.h"

uint32_t time_get_os_boottime_ms(void) {
  struct timespec timespec;
  clock_gettime(CLOCK_BOOTTIME, &timespec);
  return (timespec.tv_sec * 1000) + (timespec.tv_nsec / 1000000);
}
