# Fluoride Style Guide
This document outlines the coding conventions and code style used in Fluoride.
Its primary purpose is to provide explicit guidance on style so that developers
are consistent with one another and spend less time debating style.

## Directory structure
Directories at the top-level should consist of major subsystems in Fluoride.
Each subsystem's purpose should be documented in the `doc/directory_layout.md`
file, even if it seems obvious from the name.

For a subsystem that contains code, its directory structure should look like:
```
  Android.mk
  include/
  src/
  test/
```
Further, the directory structure inside `src/` and `include/` should be
mirrored. In other words, if `src/` contains a subdirectory called `foo/`,
`include/` must also have a subdirectory named `foo/`.

## Target architecture
Fluoride targets a variety of hardware and cannot make many assumptions about
memory layout, sizes, byte order, etc. As a result, some operations are
considered unsafe and this section outlines the most important ones to watch out
for.

### Pointer / integer casts
In general, do not cast pointers to integers or vice versa.

The one exception is if an integer needs to be temporarily converted to a
pointer and then back to the original integer. This style of code is typically
needed when providing an integral value as the context to a callback, as in the
following example.
```
void my_callback(void *context) {
  uintptr_t arg = context;
}

set_up_callback(my_callback, (uintptr_t)5);
```
Note, however, that the integral value was written into the pointer and read
from the pointer as a `uintptr_t` to avoid a loss of precision (or to make the
loss explicit).

### Byte order
It is not safe to assume any particular byte order. When serializing or
deserializing data, it is unsafe to memcpy unless both source and destination
pointers have the same type.

## Language
Fluoride is written in C99 and should take advantage of the features offered by
it. However, not all language features lend themselves well to the type of
development required by Fluoride. This section provides guidance on some of the
features to embrace or avoid.

### C Preprocessor
The use of the C preprocessor should be minimized. In particular:
* use functions or, if absolutely necessary, inline functions instead of macros
* use `static const` variables instead of `#define`
* use `enum` for enumerations, not a collection of `#define`s
* minimize the use of feature / conditional macros

The last point is perhaps the most contentious. It's well-understood that
feature macros are useful in reducing code size but it leads to an exponential
explosion in build configurations. Setting up, testing, and verifying each of
the `2^n` build configurations is untenable for `n` greater than, say, 4.

### C++
Although C++ offers constructs that may make Fluoride development faster,
safer, more pleasant, etc. the decision _for the time being_ is to stick with
pure C99. The exceptions are when linking against libraries that are written
in C++. At the time of writing these libraries are `gtest` and `tinyxml2`,
where the latter is a dependency that should be eliminated in favor of simpler,
non-XML formats.

### Variadic functions
Variadic functions are dangerous and should be avoided for most code. The
exception is when implementing logging since the benefits of readability
outweigh the cost of safety.

### Functions with zero arguments
Functions that do not take any arguments (0 arity) should be declared like so:
```
void function(void);
```
Note that the function explicitly includes `void` in its parameter list to
indicate to the compiler that it takes no arguments.

### Variable declarations
Variables should be declared one per line as close to initialization as possible.
In nearly all cases, variables should be declared and initialized on the same line.
Variable declarations should not include extra whitespace to line up fields. For
example, the following style is preferred:
```
  int my_long_variable_name = 0;
  int x = 5;
```
whereas this code is not acceptable:
```
  int my_long_variable_name = 0;
  int                     x = 5;
```

As a result of the above rule to declare and initialize variables together,
`for` loops should declare and initialize their iterator variable in the
initializer statement:
```
  for (int i = 0; i < 10; ++i) {
    // use i
  }
```

### Contiguous memory structs
Use C99 flexible arrays as the last member of a struct if the array needs
to be allocated in contiguous memory with its containing struct.
A flexible array member is writen as `array_name[]` without a specified size.
For example:
```
typedef struct {
  size_t length;
  uint8_t data[];
} buffer_t;

// Allocate a buffer with 128 bytes available for my_buffer->data.
buffer_t *my_buffer = malloc(sizeof(buffer_t) + 128);
uint8_t *data = my_buffer->data;
```

### Pointer arithmetic
Avoid pointer arithmetic when possible as it results in difficult to read code.
Prefer array-indexing syntax over pointer arithmetic.

In particular, do not write code like this:
```
typedef struct {
  size_t length;
} buffer_t;

buffer_t *my_buffer = malloc(sizeof(buffer_t) + 128);
uint8_t *data = (uint8_t *)(my_buffer + 1);
```
Instead, use zero-length arrays as described above to avoid pointer arithmetic
and array indexing entirely.

### Boolean type
Use the C99 `bool` type with values `true` and `false` defined in `stdbool.h`.
Not only is this a standardized type, it is also safer and provides more
compile-time checks.

### Booleans instead of bitfields
Use booleans to represent boolean state, instead of a set of masks into an
integer. It's more transparent and readable, and less error prone.

### Function names as strings
C99 defines `__func__` as an identifier that represents the function's name
in which it is used. The magic identifier `__FUNCTION__` should not be used
as it is a non-standard language extension and an equivalent standardized
mechanism exists. In other words, use `__func__` over `__FUNCTION__`.

## Fluoride conventions
This section describes coding conventions that are specific to Fluoride.
Whereas the _Language_ section describes the use of language features, this
section describes idioms, best practices, and conventions that are independent
of language features.

### Memory management
Use `osi_malloc` or `osi_calloc` to allocate bytes instead of plain `malloc`.
Likewise, use `osi_free` over `free`. These wrapped functions provide additional
lightweight memory bounds checks that can help track down memory errors.

By convention, functions that contain `*_new` in their name are allocation
routines and objects returned from those functions must be freed with the
corresponding `*_free` function. For example, list objects returned from
`list_new` should be freed with `list_free` and no other freeing routine.

### Asserts
Use `assert` liberally throughout the code to enforce invariants. Assertions
should not have any side-effects and should be used to detect programming logic
errors.

At minimum, every function should assert expectations on its arguments. The
following example demonstrates the kinds of assertions one should make on
function arguments.
```
  size_t open_and_read_file(const char *filename, void *target_buffer, size_t max_bytes) {
    assert(filename != NULL);
    assert(filename[0] != '\0');
    assert(target_buffer != NULL);
    assert(max_bytes > 0);

    // function implementation begins here
  }
```

## Header files
In general, every source file (`.c` or `.cpp`) in a `src/` directory should
have a corresponding header (`.h`) in the `include/` directory.

### Template header file
```
[copyright header]

#pragma once

#include <system/a.h>
#include <system/b.h>

#include "subsystem/include/a.h"
#include "subsystem/include/b.h"

typedef struct alarm_t alarm_t;
typedef struct list_t list_t;

// This comment describes the following function. It is not a structured
// comment, it's English prose. Function arguments can be referred to as
// |param|. This function returns true if a new object was created, false
// otherwise.
bool template_new(const list_t *param);

// Each public function must have a comment describing its semantics. In
// particular, edge cases, and whether a pointer argument may or may not be
// NULL.
void template_use_alarm(alarm_t *alarm);
```

### License header
Each header file must begin with the following Apache 2.0 License with `<year>`
and `<owner>` replaced with the year in which the file was authored and the
owner of the copyright, respectively.
```
/******************************************************************************
 *
 *  Copyright (C) <year> <owner>
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
```

### Include guard
After the license header, each header file must contain the include guard:
```
#pragma once
```
This form is used over traditional `#define`-based include guards as it is less
error-prone, doesn't pollute the global namespace, is more compact, and can
result in faster compilation.

## Formatting
Code formatting is pretty arbitrary, but the codebase is easier to follow if
everyone uses the same style. Individuals may not agree with every aspect of
the formatting rules, and some of the rules may take some getting used to,
but it is important that all engineers follow the formatting rules so we can all
understand and read the code easily.

### White space
* use only spaces, indent 2 spaces at a time
* no trailing whitespaces at the end of a line
* no tab characters
* use one blank line to separate logical code blocks, function definitions,
  and sections of a file

```
// Space after keyword in conditionals and loops.
// No space immeidately before or after parentheses.
if (foo)

// Space surrounding binary operators.
if (foo < 5)

// Space after comma.
for (int x = 0, y = 0; x; ++y)

// No space between unary operators and their argument.
++x;
z = -y;

// No space between function name and open parenthesis.
call_my_fn(arg1, arg2);

// Space before * in variable declaration.
int *x = NULL;

// Space after // beginning a comment.
// Notice the space between "//" and "N".
```

Use only spaces, and indent 2 spaces at a time. Do not use tab characters in the
codebase.

Use a single blank line to separate logical code blocks, function definitions,
and sections of a file.

### Brace style
```
// Open curly braces are never on a line by themselves.
void my_function(void) {
  // Conditional statements with only one child statement should elide braces.
  // The child statement must be on a new line, indented by 2 spaces.
  if (foo)
    do_bar();
  else
    do_baz();

  // Conditionals with a branch containing more than one child statement forces
  // braces on both branches.
  if (foo) {
    do_bar();
  } else {
    do_baz();
    ++var1;
  }
}
```
