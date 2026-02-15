/*
 * Copyright (C) 2015 The Android Open Source Project
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

#include "event_fd.h"

#include <fcntl.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <memory>

#include <base/file.h>
#include <base/logging.h>
#include <base/stringprintf.h>

#include "event_type.h"
#include "perf_event.h"
#include "utils.h"

static int perf_event_open(perf_event_attr* attr, pid_t pid, int cpu, int group_fd,
                           unsigned long flags) {
  return syscall(__NR_perf_event_open, attr, pid, cpu, group_fd, flags);
}

std::unique_ptr<EventFd> EventFd::OpenEventFileForProcess(const perf_event_attr& attr, pid_t pid) {
  return OpenEventFile(attr, pid, -1);
}

std::unique_ptr<EventFd> EventFd::OpenEventFileForCpu(const perf_event_attr& attr, int cpu) {
  return OpenEventFile(attr, -1, cpu);
}

std::unique_ptr<EventFd> EventFd::OpenEventFile(const perf_event_attr& attr, pid_t pid, int cpu) {
  perf_event_attr perf_attr = attr;
  std::string event_name = "unknown event";
  const EventType* event_type =
      EventTypeFactory::FindEventTypeByConfig(perf_attr.type, perf_attr.config);
  if (event_type != nullptr) {
    event_name = event_type->name;
  }
  int perf_event_fd = perf_event_open(&perf_attr, pid, cpu, -1, 0);
  if (perf_event_fd == -1) {
    // It depends whether the perf_event_file configuration is supported by the kernel and the
    // machine. So fail to open the file is not an error.
    PLOG(DEBUG) << "open perf_event_file (event " << event_name << ", pid " << pid << ", cpu "
                << cpu << ") failed";
    return nullptr;
  }
  if (fcntl(perf_event_fd, F_SETFD, FD_CLOEXEC) == -1) {
    PLOG(ERROR) << "fcntl(FD_CLOEXEC) for perf_event_file (event " << event_name << ", pid " << pid
                << ", cpu " << cpu << ") failed";
    return nullptr;
  }
  return std::unique_ptr<EventFd>(new EventFd(perf_event_fd, event_name, pid, cpu));
}

EventFd::~EventFd() {
  if (mmap_addr_ != nullptr) {
    munmap(mmap_addr_, mmap_len_);
  }
  close(perf_event_fd_);
}

std::string EventFd::Name() const {
  return android::base::StringPrintf("perf_event_file(event %s, pid %d, cpu %d)",
                                     event_name_.c_str(), pid_, cpu_);
}

uint64_t EventFd::Id() const {
  if (id_ == 0) {
    PerfCounter counter;
    if (ReadCounter(&counter)) {
      id_ = counter.id;
    }
  }
  return id_;
}

bool EventFd::EnableEvent() {
  int result = ioctl(perf_event_fd_, PERF_EVENT_IOC_ENABLE, 0);
  if (result < 0) {
    PLOG(ERROR) << "ioctl(enable) " << Name() << " failed";
    return false;
  }
  return true;
}

bool EventFd::DisableEvent() {
  int result = ioctl(perf_event_fd_, PERF_EVENT_IOC_DISABLE, 0);
  if (result < 0) {
    PLOG(ERROR) << "ioctl(disable) " << Name() << " failed";
    return false;
  }
  return true;
}

bool EventFd::ReadCounter(PerfCounter* counter) const {
  CHECK(counter != nullptr);
  if (!android::base::ReadFully(perf_event_fd_, counter, sizeof(*counter))) {
    PLOG(ERROR) << "ReadCounter from " << Name() << " failed";
    return false;
  }
  return true;
}

bool EventFd::MmapContent(size_t mmap_pages) {
  CHECK(IsPowerOfTwo(mmap_pages));
  size_t page_size = sysconf(_SC_PAGE_SIZE);
  size_t mmap_len = (mmap_pages + 1) * page_size;
  void* mmap_addr = mmap(nullptr, mmap_len, PROT_READ | PROT_WRITE, MAP_SHARED, perf_event_fd_, 0);
  if (mmap_addr == MAP_FAILED) {
    PLOG(ERROR) << "mmap() failed for " << Name();
    return false;
  }
  mmap_addr_ = mmap_addr;
  mmap_len_ = mmap_len;
  mmap_metadata_page_ = reinterpret_cast<perf_event_mmap_page*>(mmap_addr_);
  mmap_data_buffer_ = reinterpret_cast<char*>(mmap_addr_) + page_size;
  mmap_data_buffer_size_ = mmap_len_ - page_size;
  return true;
}

size_t EventFd::GetAvailableMmapData(char** pdata) {
  // The mmap_data_buffer is used as a ring buffer like below. The kernel continuously writes
  // records to the buffer, and the user continuously read records out.
  //         _________________________________________
  // buffer | can write   |   can read   |  can write |
  //                      ^              ^
  //                    read_head       write_head
  //
  // So the user can read records in [read_head, write_head), and the kernel can write records
  // in [write_head, read_head). The kernel is responsible for updating write_head, and the user
  // is responsible for updating read_head.

  uint64_t buf_mask = mmap_data_buffer_size_ - 1;
  uint64_t write_head = mmap_metadata_page_->data_head & buf_mask;
  uint64_t read_head = mmap_metadata_page_->data_tail & buf_mask;

  if (read_head == write_head) {
    // No available data.
    return 0;
  }

  // Make sure we can see the data after the fence.
  std::atomic_thread_fence(std::memory_order_acquire);

  *pdata = mmap_data_buffer_ + read_head;
  if (read_head < write_head) {
    return write_head - read_head;
  } else {
    return mmap_data_buffer_size_ - read_head;
  }
}

void EventFd::DiscardMmapData(size_t discard_size) {
  mmap_metadata_page_->data_tail += discard_size;
}

void EventFd::PreparePollForMmapData(pollfd* poll_fd) {
  memset(poll_fd, 0, sizeof(pollfd));
  poll_fd->fd = perf_event_fd_;
  poll_fd->events = POLLIN;
}
