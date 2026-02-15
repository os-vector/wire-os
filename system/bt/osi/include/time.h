/******************************************************************************
 *
 *  Copyright (C) 2015 Google, Inc.
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

#include <stdint.h>

// Get the OS boot time in milliseconds.
//
// NOTE: The return value will rollover every 49.7 days,
// hence it cannot be used for absolute time comparison.
// Relative time comparison using 32-bits integers such
// as (t2_u32 - t1_u32 < delta_u32) should work as expected as long
// as there is no multiple rollover between t2_u32 and t1_u32.
uint32_t time_get_os_boottime_ms(void);
