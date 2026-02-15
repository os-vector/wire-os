Native tests for 'perfprofd'. Please run with 'runtest perfprofd'
(a.k.a. "$ANDROID_BUILD_TOP"/development/testrunner/runtest.py).

Notes:

1. One of the testpoints in this test suite performs a live 'perf'
run on the device; before invoking the test be sure that 'perf'
has been built and installed on the device in /system/bin/perf

2. The daemon under test, perfprofd, is broken into a main function, a
"core" library, and a "utils library. Picture:

	+-----------+   perfprofdmain.o
	| perfprofd |
	| main()    |   1-liner; calls perfprofd_main()
	+-----------+
	   |
	   v
	+-----------+   perfprofdcore.a
	| perfprofd |
	| core      |   most of the interesting code is here;
	|           |   calls into utils library when for
	+-----------+   operations such as sleep, log, etc
	   |
	   v
	+-----------+   perfprofdutils.a
	| perfprofd |
	| utils     |   real implementations of perfprofd_sleep,
	|           |   perfprofd_log_* etc
	+-----------+

Because the daemon tends to spend a lot of time sleeping/waiting,
it is impractical to try to test it directly. Instead we insert a
mock utilities layer and then have a test driver that invokes the
daemon main function. Picture for perfprofd_test:

	+----------------+   perfprofd_test.cc
	| perfprofd_test |
	|                |   makes calls into perfprofd_main(),
	+----------------+   then verifies behavior
	   |
	   v
	+-----------+   perfprofdcore.a
	| perfprofd |
	| core      |   same as above
	+-----------+
	   |
	   v
	+-----------+   perfprofdmockutils.a
	| perfprofd |
	| mockutils |   mock implementations of perfprofd_sleep,
	|           |   perfprofd_log_* etc
	+-----------+

The mockup versions of perfprofd_sleep() and  perfprofd_log_* do
simply log the fact that they are called; the test driver can
then examine the log to make sure that the daemon is doing
what it is supposed to be doing.
