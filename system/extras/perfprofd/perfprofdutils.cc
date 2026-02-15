/*
**
** Copyright 2015, The Android Open Source Project
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
*/

#define LOG_TAG "perfprofd"

#include <stdarg.h>
#include <unistd.h>

#include <utils/Log.h>

#include "perfprofdutils.h"

void perfprofd_log_error(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    LOG_PRI_VA(ANDROID_LOG_ERROR, LOG_TAG, fmt, ap);
    va_end(ap);
}

void perfprofd_log_warning(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    LOG_PRI_VA(ANDROID_LOG_WARN, LOG_TAG, fmt, ap);
    va_end(ap);
}

void perfprofd_log_info(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    LOG_PRI_VA(ANDROID_LOG_INFO, LOG_TAG, fmt, ap);
    va_end(ap);
}

void perfprofd_sleep(int seconds)
{
  sleep(seconds);
}
