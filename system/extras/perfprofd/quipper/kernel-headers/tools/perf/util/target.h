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
#ifndef _PERF_TARGET_H
#define _PERF_TARGET_H
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
struct perf_target {
  const char * pid;
  const char * tid;
  const char * cpu_list;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  const char * uid_str;
  uid_t uid;
  bool system_wide;
  bool uses_mmap;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
};
enum perf_target_errno {
  PERF_ERRNO_TARGET__SUCCESS = 0,
  __PERF_ERRNO_TARGET__START = - 10000,
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  PERF_ERRNO_TARGET__PID_OVERRIDE_CPU = __PERF_ERRNO_TARGET__START,
  PERF_ERRNO_TARGET__PID_OVERRIDE_UID,
  PERF_ERRNO_TARGET__UID_OVERRIDE_CPU,
  PERF_ERRNO_TARGET__PID_OVERRIDE_SYSTEM,
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  PERF_ERRNO_TARGET__UID_OVERRIDE_SYSTEM,
  PERF_ERRNO_TARGET__INVALID_UID,
  PERF_ERRNO_TARGET__USER_NOT_FOUND,
  __PERF_ERRNO_TARGET__END,
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
};
enum perf_target_errno perf_target__validate(struct perf_target * target);
enum perf_target_errno perf_target__parse_uid(struct perf_target * target);
#endif
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */

