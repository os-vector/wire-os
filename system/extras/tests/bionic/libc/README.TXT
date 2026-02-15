This directory contains a set of tests for Android's Bionic C library.

You must define the BIONIC_TESTS environment variable to build these
test programs. For example, do:

    cd system/extras/tests/bionic/libc
    mm BIONIC_TESTS=1

All test programs, except those in the 'other' directory, should exit
with a status code of 0 in case of success, and 1 in case of failure.

The directory layout is simple:

  common/
    Contains tests that can be compiled either with Bionic or another
    C library.

  bionic/
    Contains tests that can *only* be compiled against Bionic

  other/
    Other unrelated tests. These are not run by the test runner
    program but will be installed to your device nevertheless.
    Put benchmarks and various debug/info stuff there.
