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

#pragma once
#include <string.h>
#include <stdbool.h>
#include "include/bt_logger_lib.h"

#define kLogLevelVerbose 2
#define kLogLevelDebug 3
#define kLogLevelInfo 4
#define kLogLevelWarn 5
#define kLogLevelError 6
#define kLogLevelOff 7


bool isUsingAndroidLogging();
void enableAndroidLogging(const bool enable);
void setAndroidLoggingTag(const char* tag);
int getMinLogLevel();
void setMinLogLevel(const int level);

void logv(const char* fmt, ...);
void logd(const char* fmt, ...);
void logi(const char* fmt, ...);
void logw(const char* fmt, ...);
void loge(const char* fmt, ...);


extern bt_logger_interface_t *logger_interface;
extern bool bt_logger_enabled;

/*
 * TODO(armansito): Work-around until we figure out a way to generate logs in a
 * platform-independent manner.
 */
#if defined(OS_GENERIC)

/* syslog didn't work well here since we would be redefining LOG_DEBUG. */
#include <stdio.h>

#define LOGWRAPPER(tag, fmt, args...) fprintf(stderr, "%s: " fmt "\n", tag, ## args)

#define LOG_VERBOSE(...) LOGWRAPPER(__VA_ARGS__)
#define LOG_DEBUG(...) LOGWRAPPER(__VA_ARGS__)
#define LOG_INFO(...) LOGWRAPPER(__VA_ARGS__)
#define LOG_WARN(...) LOGWRAPPER(__VA_ARGS__)
#define LOG_ERROR(...) LOGWRAPPER(__VA_ARGS__)

#else  /* !defined(OS_GENERIC) */

#include <cutils/log.h>

//#define VNDLOG(tag, fmt, ## args) if(logger_interface)logger_interface->send_log_data(tag, fmt, ## args)

#ifdef ANDROID

#if LOG_NDEBUG
#define LOG_VERBOSE(...) ((void)0)
#else  // LOG_NDEBUG
#define LOG_VERBOSE(tag, fmt, args...) ALOG(LOG_VERBOSE, tag, fmt, ## args)
#endif  // !LOG_NDEBUG
#define LOG_DEBUG(tag, fmt, args...)   {if(logger_interface)logger_interface->send_log_data(tag, fmt, ## args);ALOG(LOG_DEBUG, tag, fmt, ## args);}
#define LOG_INFO(tag, fmt, args...)    {if(logger_interface)logger_interface->send_log_data(tag, fmt, ## args);ALOG(LOG_INFO, tag, fmt, ## args);}
#define LOG_WARN(tag, fmt, args...)    {if(logger_interface)logger_interface->send_log_data(tag, fmt, ## args);ALOG(LOG_WARN, tag, fmt, ## args);}
#define LOG_ERROR(tag, fmt, args...)   {if(logger_interface)logger_interface->send_log_data(tag, fmt, ## args);ALOG(LOG_ERROR, tag, fmt, ## args);}

#else
#include <errno.h>
#include <limits.h>
#include <stdio.h>

#ifdef USE_ANDROID_LOGGING
#include <utils/Log.h>
#define ALOGV(...) logv(__VA_ARGS__)
#define ALOGD(...) logd(__VA_ARGS__)
#define ALOGI(...) logi(__VA_ARGS__)
#define ALOGW(...) logw(__VA_ARGS__)
#define ALOGE(...) loge(__VA_ARGS__)
#define LOG_TAG "bt_stack"
#define LOG_VERBOSE(...) logv(__VA_ARGS__)
#define LOG_DEBUG(...)   logd(__VA_ARGS__)
#define LOG_INFO(...)   logi(__VA_ARGS__)
#define LOG_WARN(...)   logw(__VA_ARGS__)
#define LOG_ERROR(...)   loge(__VA_ARGS__)
#else
#include <syslog.h>

#define LOG_TAG "bt_stack : "

#define PRI_INFO " I"
#define PRI_WARN " W"
#define PRI_ERROR " E"
#define PRI_DEBUG " D"
#define PRI_VERB " V"

#define ALOGV(fmt, arg...) syslog (LOG_WARNING, LOG_TAG fmt, ##arg)
#define ALOGD(fmt, arg...) syslog (LOG_NOTICE, LOG_TAG fmt, ##arg)
#define ALOGI(fmt, arg...) syslog (LOG_NOTICE, LOG_TAG fmt, ##arg)
#define ALOGW(fmt, arg...) syslog (LOG_WARNING, LOG_TAG fmt, ##arg)
#define ALOGE(fmt, arg...) syslog (LOG_ERR, LOG_TAG fmt, ##arg)

#define LOG_VERBOSE(fmt, arg...) syslog (LOG_WARNING, LOG_TAG fmt, ##arg)
#define LOG_DEBUG(fmt, arg...) syslog (LOG_NOTICE, LOG_TAG fmt, ##arg)
#define LOG_INFO(fmt, arg...)  syslog (LOG_NOTICE, LOG_TAG fmt, ##arg)
#define LOG_WARN(fmt, arg...)  syslog (LOG_WARNING, LOG_TAG fmt, ##arg)
#define LOG_ERROR(fmt, arg...) syslog (LOG_ERR, LOG_TAG fmt, ##arg)
#endif
#endif

#endif  /* defined(OS_GENERIC) */
