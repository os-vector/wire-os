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

#include "AlarmTestHarness.h"

#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

#include <hardware/bluetooth.h>

extern "C" {
#include "osi/include/alarm.h"
#include "osi/include/allocation_tracker.h"
#include "osi/include/wakelock.h"
}

static timer_t timer;
static alarm_cb saved_callback;
static void *saved_data;
static AlarmTestHarness *current_harness;

static void timer_callback(void *) {
  saved_callback(saved_data);
}

void AlarmTestHarness::SetUp() {
  AllocationTestHarness::SetUp();

  current_harness = this;
  TIMER_INTERVAL_FOR_WAKELOCK_IN_MS = 100;

  struct sigevent sigevent;
  memset(&sigevent, 0, sizeof(sigevent));
  sigevent.sigev_notify = SIGEV_THREAD;
  sigevent.sigev_notify_function = (void (*)(union sigval))timer_callback;
  sigevent.sigev_value.sival_ptr = NULL;
  timer_create(CLOCK_BOOTTIME, &sigevent, &timer);

  // TODO (jamuraa): maybe use base::CreateNewTempDirectory instead?
#if defined(OS_GENERIC)
  tmp_dir_ = "/tmp/btwlXXXXXX";
#else  // !defined(OS_GENERIC)
  tmp_dir_ = "/data/local/tmp/btwlXXXXXX";
#endif  // !defined(OS_GENERIC)

  char *buffer = const_cast<char *>(tmp_dir_.c_str());
  char *dtemp = mkdtemp(buffer);
  if (!dtemp) {
    perror("Can't make wake lock test directory: ");
    assert(false);
  }

  lock_path_ = tmp_dir_ + "/wake_lock";
  unlock_path_ = tmp_dir_ + "/wake_unlock";

  creat(lock_path_.c_str(), S_IRWXU);
  creat(unlock_path_.c_str(), S_IRWXU);

  wakelock_set_paths(lock_path_.c_str(), unlock_path_.c_str());
}

void AlarmTestHarness::TearDown() {
  alarm_cleanup();
  wakelock_cleanup();

  // clean up the temp wake lock directory
  unlink(lock_path_.c_str());
  unlink(unlock_path_.c_str());
  rmdir(tmp_dir_.c_str());

  timer_delete(timer);
  AllocationTestHarness::TearDown();
}


bool AlarmTestHarness::WakeLockHeld() {
  bool held = false;

  int lock_fd = open(lock_path_.c_str(), O_RDONLY);
  assert(lock_fd >= 0);

  int unlock_fd = open(unlock_path_.c_str(), O_RDONLY);
  assert(unlock_fd >= 0);

  struct stat lock_stat, unlock_stat;
  fstat(lock_fd, &lock_stat);
  fstat(unlock_fd, &unlock_stat);

  assert(lock_stat.st_size >= unlock_stat.st_size);

  void *lock_file = mmap(nullptr, lock_stat.st_size, PROT_READ,
                         MAP_PRIVATE, lock_fd, 0);

  void *unlock_file = mmap(nullptr, unlock_stat.st_size, PROT_READ,
                           MAP_PRIVATE, unlock_fd, 0);

  if (memcmp(lock_file, unlock_file, unlock_stat.st_size) == 0) {
    held = lock_stat.st_size > unlock_stat.st_size;
  } else {
    // these files should always either be with a lock that has more,
    // or equal.
    assert(false);
  }

  munmap(lock_file, lock_stat.st_size);
  munmap(unlock_file, unlock_stat.st_size);
  close(lock_fd);
  close(unlock_fd);

  return held;
}
