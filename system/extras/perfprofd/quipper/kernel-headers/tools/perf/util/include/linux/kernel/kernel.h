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
#ifndef PERF_LINUX_KERNEL_H_
#define PERF_LINUX_KERNEL_H_
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define DIV_ROUND_UP(n,d) (((n) + (d) - 1) / (d))
#define PERF_ALIGN(x,a) __PERF_ALIGN_MASK(x, (typeof(x)) (a) - 1)
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define __PERF_ALIGN_MASK(x,mask) (((x) + (mask)) & ~(mask))
#ifndef offsetof
#define offsetof(TYPE,MEMBER) ((size_t) & ((TYPE *) 0)->MEMBER)
#endif
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#ifndef container_of
#define container_of(ptr,type,member) ({ const typeof(((type *) 0)->member) * __mptr = (ptr); (type *) ((char *) __mptr - offsetof(type, member)); })
#endif
#define BUILD_BUG_ON_ZERO(e) (sizeof(struct { int : - ! ! (e); }))
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#ifndef max
#define max(x,y) ({ typeof(x) _max1 = (x); typeof(y) _max2 = (y); (void) (& _max1 == & _max2); _max1 > _max2 ? _max1 : _max2; })
#endif
#ifndef min
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define min(x,y) ({ typeof(x) _min1 = (x); typeof(y) _min2 = (y); (void) (& _min1 == & _min2); _min1 < _min2 ? _min1 : _min2; })
#endif
#ifndef roundup
#define roundup(x,y) (\
{ const typeof(y) __y = y; (((x) + (__y - 1)) / __y) * __y; \
} \
)
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#endif
#ifndef BUG_ON
#ifdef NDEBUG
#define BUG_ON(cond) do { if(cond) { } } while(0)
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#else
#define BUG_ON(cond) assert(! (cond))
#endif
#endif
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define cpu_to_le64(x) (x)
#define cpu_to_le32(x) (x)
#ifndef pr_fmt
#define pr_fmt(fmt) fmt
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#endif
#define pr_err(fmt,...) eprintf(0, pr_fmt(fmt), ##__VA_ARGS__)
#define pr_warning(fmt,...) eprintf(0, pr_fmt(fmt), ##__VA_ARGS__)
#define pr_info(fmt,...) eprintf(0, pr_fmt(fmt), ##__VA_ARGS__)
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define pr_debug(fmt,...) eprintf(1, pr_fmt(fmt), ##__VA_ARGS__)
#define pr_debugN(n,fmt,...) eprintf(n, pr_fmt(fmt), ##__VA_ARGS__)
#define pr_debug2(fmt,...) pr_debugN(2, pr_fmt(fmt), ##__VA_ARGS__)
#define pr_debug3(fmt,...) pr_debugN(3, pr_fmt(fmt), ##__VA_ARGS__)
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define pr_debug4(fmt,...) pr_debugN(4, pr_fmt(fmt), ##__VA_ARGS__)
#define __round_mask(x,y) ((__typeof__(x)) ((y) - 1))
#define round_up(x,y) ((((x) - 1) | __round_mask(x, y)) + 1)
#define round_down(x,y) ((x) & ~__round_mask(x, y))
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#endif

