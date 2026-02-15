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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <string>
#include <sstream>
#include <sys/types.h>
#include <sys/wait.h>

#include <cutils/properties.h>

#include "cpuconfig.h"
#include "perfprofdutils.h"

#define SYSFSCPU "/sys/devices/system/cpu"

HardwireCpuHelper::HardwireCpuHelper(bool perform)
    : mpdecision_stopped_(false)
{
  if (perform && GetMpdecisionRunning()) {
    mpdecision_stopped_ = true;
    StopMpdecision();
    int ncores = GetNumCores();
    for (int i = 0; i < ncores; ++i) {
      OnlineCore(i, 1);
    }
  }
}

HardwireCpuHelper::~HardwireCpuHelper()
{
  if (mpdecision_stopped_) {
    RestartMpdecision();
  }
}

bool HardwireCpuHelper::GetMpdecisionRunning()
{
  char propBuf[PROPERTY_VALUE_MAX];
  property_get("init.svc.mpdecision", propBuf, "");
  return strcmp(propBuf, "running") == 0;
}


int HardwireCpuHelper::GetNumCores()
{
  int ncores = -1;
  std::string possible(SYSFSCPU "/possible");
  FILE *fp = fopen(possible.c_str(), "re");
  if (fp) {
    unsigned lo = 0, hi = 0;
    if (fscanf(fp, "%u-%u", &lo, &hi) == 2) {
      ncores = hi - lo + 1;
    }
    fclose(fp);
  }
  return ncores;
}

void HardwireCpuHelper::OnlineCore(int i, int onoff)
{
  std::stringstream ss;
  ss << SYSFSCPU "/cpu" << i << "/online";
  FILE *fp = fopen(ss.str().c_str(), "we");
  if (fp) {
    fprintf(fp, onoff ? "1\n" : "0\n");
    fclose(fp);
  } else {
    W_ALOGW("open failed for %s", ss.str().c_str());
  }
}

void HardwireCpuHelper::StopMpdecision()
{
  if (property_set("ctl.stop", "mpdecision")) {
    W_ALOGE("setprop ctl.stop mpdecision failed");
  }
}

void HardwireCpuHelper::RestartMpdecision()
{
  // Don't try to offline the cores we previously onlined -- let
  // mpdecision figure out what to do

  if (property_set("ctl.start", "mpdecision")) {
    W_ALOGE("setprop ctl.start mpdecision failed");
  }
}
