/****************************************************************************
 ****************************************************************************
 ***
 ***   This header was automatically generated from a Linux kernel header
 ***   of the same name, to make information necessary for userspace to
 ***   call into the kernel available to libc.  It contains only constants,
 ***   structures, and macros generated from the original header, and thus,
 ***   contains no copyrightable information.
 ***
 ***   To edit the content of this header, modify the corresponding
 ***   source file (e.g. under external/kernel-headers/original/) then
 ***   run bionic/libc/kernel/tools/update_all.py
 ***
 ***   Any manual change here will be lost the next time this script will
 ***   be run. You've been warned!
 ***
 ****************************************************************************
 ****************************************************************************/
#ifndef __PERF_RECORD_H
#define __PERF_RECORD_H
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
struct mmap_event {
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  struct perf_event_header header;
  u32 pid, tid;
  u64 start;
  u64 len;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  u64 pgoff;
  char filename[PATH_MAX];
};
struct mmap2_event {
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  struct perf_event_header header;
  u32 pid, tid;
  u64 start;
  u64 len;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  u64 pgoff;
  u32 maj;
  u32 min;
  u64 ino;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  u64 ino_generation;
  char filename[PATH_MAX];
};
struct comm_event {
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  struct perf_event_header header;
  u32 pid, tid;
  char comm[16];
};
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
struct fork_event {
  struct perf_event_header header;
  u32 pid, ppid;
  u32 tid, ptid;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  u64 time;
};
struct lost_event {
  struct perf_event_header header;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  u64 id;
  u64 lost;
};
struct read_event {
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  struct perf_event_header header;
  u32 pid, tid;
  u64 value;
  u64 time_enabled;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  u64 time_running;
  u64 id;
};
#define PERF_SAMPLE_MASK (PERF_SAMPLE_IP | PERF_SAMPLE_TID | PERF_SAMPLE_TIME | PERF_SAMPLE_ADDR | PERF_SAMPLE_ID | PERF_SAMPLE_STREAM_ID | PERF_SAMPLE_CPU | PERF_SAMPLE_PERIOD | PERF_SAMPLE_IDENTIFIER)
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
struct sample_event {
  struct perf_event_header header;
  u64 array[];
};
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
struct regs_dump {
  u64 abi;
  u64 * regs;
};
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
struct stack_dump {
  u16 offset;
  u64 size;
  char * data;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
};
struct sample_read_value {
  u64 value;
  u64 id;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
};
struct sample_read {
  u64 time_enabled;
  u64 time_running;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  union {
    struct {
      u64 nr;
      struct sample_read_value * values;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
    } group;
    struct sample_read_value one;
  };
};
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
struct perf_sample {
  u64 ip;
  u32 pid, tid;
  u64 time;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  u64 addr;
  u64 id;
  u64 stream_id;
  u64 period;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  u64 weight;
  u32 cpu;
  u32 raw_size;
  u64 data_src;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  void * raw_data;
  struct ip_callchain * callchain;
  struct branch_stack * branch_stack;
  struct regs_dump user_regs;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  struct stack_dump user_stack;
  struct sample_read read;
};
#define PERF_MEM_DATA_SRC_NONE (PERF_MEM_S(OP, NA) | PERF_MEM_S(LVL, NA) | PERF_MEM_S(SNOOP, NA) | PERF_MEM_S(LOCK, NA) | PERF_MEM_S(TLB, NA))
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
struct build_id_event {
  struct perf_event_header header;
  pid_t pid;
  u8 build_id[PERF_ALIGN(BUILD_ID_SIZE, sizeof(u64))];
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  char filename[];
};
enum perf_user_event_type {
  PERF_RECORD_USER_TYPE_START = 64,
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  PERF_RECORD_HEADER_ATTR = 64,
  PERF_RECORD_HEADER_EVENT_TYPE = 65,
  PERF_RECORD_HEADER_TRACING_DATA = 66,
  PERF_RECORD_HEADER_BUILD_ID = 67,
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  PERF_RECORD_FINISHED_ROUND = 68,
  PERF_RECORD_HEADER_MAX
};
struct attr_event {
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  struct perf_event_header header;
  struct perf_event_attr attr;
  u64 id[];
};
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define MAX_EVENT_NAME 64
struct perf_trace_event_type {
  u64 event_id;
  char name[MAX_EVENT_NAME];
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
};
struct event_type_event {
  struct perf_event_header header;
  struct perf_trace_event_type event_type;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
};
struct tracing_data_event {
  struct perf_event_header header;
  u32 size;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
};
union perf_event {
  struct perf_event_header header;
  struct mmap_event mmap;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  struct mmap2_event mmap2;
  struct comm_event comm;
  struct fork_event fork;
  struct lost_event lost;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  struct read_event read;
  struct sample_event sample;
  struct attr_event attr;
  struct event_type_event event_type;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  struct tracing_data_event tracing_data;
  struct build_id_event build_id;
};
struct perf_tool;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
struct thread_map;
typedef int(* perf_event__handler_t) (struct perf_tool * tool, union perf_event * event, struct perf_sample * sample, struct machine * machine);
struct addr_location;
#endif
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */

