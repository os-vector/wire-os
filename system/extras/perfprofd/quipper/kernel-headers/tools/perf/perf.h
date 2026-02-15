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
#ifndef _PERF_PERF_H
#define _PERF_PERF_H
#ifdef __i386__
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define rmb() asm volatile("lock; addl $0,0(%%esp)" : : : "memory")
#define cpu_relax() asm volatile("rep; nop" : : : "memory");
#define CPUINFO_PROC "model name"
#ifndef __NR_perf_event_open
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define __NR_perf_event_open 336
#endif
#endif
#ifdef __x86_64__
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define rmb() asm volatile("lfence" : : : "memory")
#define cpu_relax() asm volatile("rep; nop" : : : "memory");
#define CPUINFO_PROC "model name"
#ifndef __NR_perf_event_open
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define __NR_perf_event_open 298
#endif
#endif
#ifdef __powerpc__
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define rmb() asm volatile("sync" : : : "memory")
#define cpu_relax() asm volatile("" : : : "memory");
#define CPUINFO_PROC "cpu"
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#endif
#ifdef __s390__
#define rmb() asm volatile("bcr 15,0" : : : "memory")
#define cpu_relax() asm volatile("" : : : "memory");
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#endif
#ifdef __sh__
#if defined(__SH4A__) || defined(__SH5__)
#define rmb() asm volatile("synco" : : : "memory")
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#else
#define rmb() asm volatile("" : : : "memory")
#endif
#define cpu_relax() asm volatile("" : : : "memory")
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define CPUINFO_PROC "cpu type"
#endif
#ifdef __hppa__
#define rmb() asm volatile("" : : : "memory")
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define cpu_relax() asm volatile("" : : : "memory");
#define CPUINFO_PROC "cpu"
#endif
#ifdef __sparc__
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define rmb() asm volatile("" : : : "memory")
#define cpu_relax() asm volatile("" : : : "memory")
#define CPUINFO_PROC "cpu"
#endif
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#ifdef __alpha__
#define rmb() asm volatile("mb" : : : "memory")
#define cpu_relax() asm volatile("" : : : "memory")
#define CPUINFO_PROC "cpu model"
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#endif
#ifdef __ia64__
#define rmb() asm volatile("mf" : : : "memory")
#define cpu_relax() asm volatile("hint @pause" : : : "memory")
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define CPUINFO_PROC "model name"
#endif
#ifdef __arm__
#define rmb() ((void(*) (void)) 0xffff0fa0) ()
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define cpu_relax() asm volatile("" : : : "memory")
#define CPUINFO_PROC "Processor"
#endif
#ifdef __aarch64__
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define rmb() asm volatile("dmb ld" : : : "memory")
#define cpu_relax() asm volatile("yield" : : : "memory")
#endif
#ifdef __mips__
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define rmb() asm volatile(".set	mips2\n\t" "sync\n\t" ".set	mips0" : : : "memory")
#define cpu_relax() asm volatile("" : : : "memory")
#define CPUINFO_PROC "cpu model"
#endif
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#ifdef __arc__
#define rmb() asm volatile("" : : : "memory")
#define cpu_relax() rmb()
#define CPUINFO_PROC "Processor"
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#endif
#ifdef __metag__
#define rmb() asm volatile("" : : : "memory")
#define cpu_relax() asm volatile("" : : : "memory")
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define CPUINFO_PROC "CPU"
#endif
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define PR_TASK_PERF_EVENTS_DISABLE 31
#define PR_TASK_PERF_EVENTS_ENABLE 32
#ifndef NSEC_PER_SEC
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define NSEC_PER_SEC 1000000000ULL
#endif
#ifndef NSEC_PER_USEC
#define NSEC_PER_USEC 1000ULL
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#endif
#define __user
#define asmlinkage
#define unlikely(x) __builtin_expect(! ! (x), 0)
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define min(x,y) ({ typeof(x) _min1 = (x); typeof(y) _min2 = (y); (void) (& _min1 == & _min2); _min1 < _min2 ? _min1 : _min2; })
#define MAX_COUNTERS 256
#define MAX_NR_CPUS 256
struct ip_callchain {
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  u64 nr;
  u64 ips[0];
};
struct branch_flags {
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  u64 mispred : 1;
  u64 predicted : 1;
  u64 reserved : 62;
};
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
struct branch_entry {
  u64 from;
  u64 to;
  struct branch_flags flags;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
};
struct branch_stack {
  u64 nr;
  struct branch_entry entries[0];
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
};
enum perf_call_graph_mode {
  CALLCHAIN_NONE,
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  CALLCHAIN_FP,
  CALLCHAIN_DWARF
};
struct perf_record_opts {
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  struct perf_target target;
  int call_graph;
  bool group;
  bool inherit_stat;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  bool no_delay;
  bool no_inherit;
  bool no_samples;
  bool pipe_output;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  bool raw_samples;
  bool sample_address;
  bool sample_weight;
  bool sample_time;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  bool period;
  unsigned int freq;
  unsigned int mmap_pages;
  unsigned int user_freq;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  u64 branch_stack;
  u64 default_interval;
  u64 user_interval;
  u16 stack_dump_size;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
};
#endif

