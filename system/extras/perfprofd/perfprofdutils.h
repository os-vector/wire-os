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

#include <sys/cdefs.h>

__BEGIN_DECLS

//
// These routines are separated out from the core perfprofd so
// as to be used as part of the unit test (see the README.txt
// alongside the unit test for more info).
//
extern void perfprofd_log_error(const char *fmt, ...);
extern void perfprofd_log_warning(const char *fmt, ...);
extern void perfprofd_log_info(const char *fmt, ...);
extern void perfprofd_sleep(int seconds);

#define W_ALOGE perfprofd_log_error
#define W_ALOGW perfprofd_log_warning
#define W_ALOGI perfprofd_log_info

__END_DECLS
