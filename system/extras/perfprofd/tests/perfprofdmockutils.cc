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
#include <vector>
#include <string>
#include <assert.h>

#include <utils/Log.h>

#include "perfprofdutils.h"

static std::vector<std::string> *mock_log;

static void append_to_log(const std::string &s)
{
  assert(mock_log);
  mock_log->push_back(s);
}

void mock_perfprofdutils_init()
{
  assert(!mock_log);
  mock_log = new std::vector<std::string>;
}

void mock_perfprofdutils_finish()
{
  assert(mock_log);
  delete mock_log;
}

std::string mock_perfprofdutils_getlogged()
{
  std::string result;
  assert(mock_log);
  for (const std::string &s : (*mock_log)) {
    result += s;
  }
  mock_log->clear();
  return result;
}

extern "C" {

#define LMAX 8192

void perfprofd_mocklog(const char *tag, const char *fmt, va_list ap)
{
    char buffer[LMAX];
    strcpy(buffer, tag);
    vsnprintf(buffer+strlen(tag), LMAX, fmt, ap);
    std::string b(buffer); b += "\012";
    append_to_log(b);
}

void perfprofd_log_error(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap); fprintf(stderr, "\n");
    perfprofd_mocklog("E: ", fmt, ap);
    va_end(ap);
}

void perfprofd_log_warning(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap); fprintf(stderr, "\n");
    perfprofd_mocklog("W: ", fmt, ap);
    va_end(ap);
}

void perfprofd_log_info(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap); fprintf(stderr, "\n");
    perfprofd_mocklog("I: ", fmt, ap);
    va_end(ap);
}

void perfprofd_sleep(int seconds)
{
    perfprofd_log_info("sleep %d seconds", seconds);
}

}
