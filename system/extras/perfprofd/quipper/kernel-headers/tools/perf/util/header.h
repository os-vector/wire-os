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
#ifndef __PERF_HEADER_H
#define __PERF_HEADER_H
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
enum {
  HEADER_RESERVED = 0,
  HEADER_FIRST_FEATURE = 1,
  HEADER_TRACING_DATA = 1,
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  HEADER_BUILD_ID,
  HEADER_HOSTNAME,
  HEADER_OSRELEASE,
  HEADER_VERSION,
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  HEADER_ARCH,
  HEADER_NRCPUS,
  HEADER_CPUDESC,
  HEADER_CPUID,
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  HEADER_TOTAL_MEM,
  HEADER_CMDLINE,
  HEADER_EVENT_DESC,
  HEADER_CPU_TOPOLOGY,
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  HEADER_NUMA_TOPOLOGY,
  HEADER_BRANCH_STACK,
  HEADER_PMU_MAPPINGS,
  HEADER_GROUP_DESC,
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  HEADER_LAST_FEATURE,
  HEADER_FEAT_BITS = 256,
};
enum perf_header_version {
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  PERF_HEADER_VERSION_1,
  PERF_HEADER_VERSION_2,
};
struct perf_file_section {
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  u64 offset;
  u64 size;
};
struct perf_file_header {
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  u64 magic;
  u64 size;
  u64 attr_size;
  struct perf_file_section attrs;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  struct perf_file_section data;
  struct perf_file_section event_types;
  DECLARE_BITMAP(adds_features, HEADER_FEAT_BITS);
};
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
struct perf_pipe_file_header {
  u64 magic;
  u64 size;
};
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
struct perf_header;
struct perf_session_env {
  char * hostname;
  char * os_release;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  char * version;
  char * arch;
  int nr_cpus_online;
  int nr_cpus_avail;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  char * cpu_desc;
  char * cpuid;
  unsigned long long total_mem;
  int nr_cmdline;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  char * cmdline;
  int nr_sibling_cores;
  char * sibling_cores;
  int nr_sibling_threads;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  char * sibling_threads;
  int nr_numa_nodes;
  char * numa_nodes;
  int nr_pmu_mappings;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  char * pmu_mappings;
  int nr_groups;
};
struct perf_header {
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  enum perf_header_version version;
  bool needs_swap;
  u64 data_offset;
  u64 data_size;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  u64 feat_offset;
  DECLARE_BITMAP(adds_features, HEADER_FEAT_BITS);
  struct perf_session_env env;
};
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
struct perf_evlist;
struct perf_session;
#endif

