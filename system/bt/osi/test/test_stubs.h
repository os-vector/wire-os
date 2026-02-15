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

// Helper macros for stubbing out functions and modules for testing.

// Stub out a function, with call counting and mode awareness
#define STUB_FUNCTION(ret, name, params) \
  UNUSED_ATTR static int name##_callcount; \
  static ret name params { \
    UNUSED_ATTR int _local_callcount = name##_callcount; \
    name##_callcount++;

// Expect a certain number of calls to the specified stub function
#define EXPECT_CALL_COUNT(name, count) \
  EXPECT_EQ((count), (name##_callcount)) << "expected " #name " to be called " #count " times"

// Reset the call count for the specificed stub function
#define RESET_CALL_COUNT(name) ((name##_callcount) = 0)

// Use this in a stub function to catch unexpected calls.
// Prints out a nice message including the call count, the
// stub function name, and the mode index (sadly no mode name)
#define UNEXPECTED_CALL EXPECT_TRUE(false) \
  << "unexpected call " << _local_callcount \
  << " to " << __func__ \
  << " during mode " << (int)_current_mode

#define MODE_IS(mode) (_current_mode == (mode))

// Macro selection helpers
#define OVERLOAD_CAT(A, B) A##B
#define OVERLOAD_SELECT(NAME, NUM) OVERLOAD_CAT(NAME##_, NUM)
#define OVERLOAD_GET_COUNT(_1, _2, _3, _4, _5, _6, COUNT, ...) COUNT
#define OVERLOAD_VA_SIZE(...) OVERLOAD_GET_COUNT(__VA_ARGS__, 6, 5, 4, 3, 2, 1)
#define OVERLOAD_OF(NAME, ...) OVERLOAD_SELECT(NAME, OVERLOAD_VA_SIZE(__VA_ARGS__))(__VA_ARGS__)

// Use this to branch stub function execution to a specific mode or modes.
// Treat it like an if statement. For example:
//
// DURING(dinner) EXPECT_EQ(bread_pudding, food);
// DURING(midday_snack, midnight_snack) EXPECT_EQ(chocolate, food);
#define DURING(...) OVERLOAD_OF(DURING, __VA_ARGS__)

#define DURING_1(mode0) \
  if (MODE_IS(mode0))
#define DURING_2(mode0, mode1) \
  if (MODE_IS(mode0) || MODE_IS(mode1))
#define DURING_3(mode0, mode1, mode2) \
  if (MODE_IS(mode0) || MODE_IS(mode1) || MODE_IS(mode2))
#define DURING_4(mode0, mode1, mode2, mode3) \
  if (MODE_IS(mode0) || MODE_IS(mode1) || MODE_IS(mode2) || MODE_IS(mode3))
#define DURING_5(mode0, mode1, mode2, mode3, mode4) \
  if (MODE_IS(mode0) || MODE_IS(mode1) || MODE_IS(mode2) || MODE_IS(mode3) || MODE_IS(mode4))
#define DURING_6(mode0, mode1, mode2, mode3, mode4, mode5) \
  if (MODE_IS(mode0) || MODE_IS(mode1) || MODE_IS(mode2) || MODE_IS(mode3) || MODE_IS(mode4) || MODE_IS(mode5))

// Use this to branch stub function exeuction to a specific call
// count index (zero based). Treat it like an if statement.
// Usually most helpful following a DURING clause. For example:
//
// DURING (breakfast) AT_CALL(0) EXPECT_EQ(bacon, food);
//
// or
//
// DURING (three_course_meal) {
//   AT_CALL(0) EXPECT_EQ(shrimp_cocktail, food);
//   AT_CALL(1) EXPECT_EQ(bacon_wrapped_bacon, food);
//   AT_CALL(1) EXPECT_EQ(chocolate_covered_fake_blueberries, food);
// }
#define AT_CALL(index) \
  if (_local_callcount == index)

// Declare all the available test modes for the DURING clauses
// For example:
//
// DECLARE_TEST_MODES(breakfast, lunch, dinner);
#define DECLARE_TEST_MODES(...) \
  typedef enum { __VA_ARGS__ } _test_modes_t; \
  static _test_modes_t _current_mode;

// Get the current test mode
#define CURRENT_TEST_MODE _current_mode

#define TEST_MODES_T _test_modes_t
