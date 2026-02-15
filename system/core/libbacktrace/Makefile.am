AM_CFLAGS := \
    -std=gnu++11 \
    -W \
    -Wall \
    -Wextra \
    -Wunused
ACLOCAL_AMFLAGS = -I m4

backtrace_sources := Backtrace.cpp
backtrace_sources += BacktraceCurrent.cpp
backtrace_sources += BacktracePtrace.cpp
backtrace_sources += BacktraceMap.cpp
backtrace_sources += ThreadEntry.cpp
backtrace_sources += UnwindCurrent.cpp
backtrace_sources += UnwindMap.cpp
backtrace_sources += UnwindPtrace.cpp
backtrace_sources += thread_utils.c
lib_LTLIBRARIES = libbacktrace.la
libbacktrace_la_CPPFLAGS := -I$(top_srcdir)/include -I$(top_srcdir)/base/include/
libbacktrace_la_SOURCES = $(backtrace_sources)
libbacktrace_la_CPPFLAGS += $(AM_CFLAGS)
libbacktrace_la_CPPFLAGS += -std=c++14
#libbacktrace_la_LIBADD   := ../base/libbase.la
libbacktrace_la_LDFLAGS := -lpthread -lrt -lunwindandroid -lbase

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = libbacktrace.pc
EXTRA_DIST = $(pkgconfig_DATA)

# Export headers
backtrace_includedir=$(pkgincludedir)/backtrace
backtrace_include_HEADERS := $(top_srcdir)/include/backtrace/backtrace_constants.h
backtrace_include_HEADERS += $(top_srcdir)/include/backtrace/Backtrace.h
backtrace_include_HEADERS += $(top_srcdir)/include/backtrace/BacktraceMap.h



