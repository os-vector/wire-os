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

#include "AllocationTestHarness.h"

extern int64_t TIMER_INTERVAL_FOR_WAKELOCK_IN_MS;

class AlarmTestHarness : public AllocationTestHarness {
  protected:
    virtual void SetUp();
    virtual void TearDown();

    std::string tmp_dir_;
    std::string lock_path_;
    std::string unlock_path_;

  public:
    // Returns true if a wake lock is held.
    bool WakeLockHeld();
};
