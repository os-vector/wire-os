# Build shared lib.
lib_LTLIBRARIES := liblogwrap.la

liblogwrap_la_SOURCES := logwrap.c
liblogwrap_la_CFLAGS  := -I$(top_srcdir)/include
liblogwrap_la_CFLAGS  += -I$(top_srcdir)/logwrapper/include
liblogwrap_la_LDFLAGS :=  -lcutils -llog

# Build logwrapper daemon
bin_PROGRAMS = logwrapper

logwrapper_SOURCES := logwrapper.c
logwrapper_CFLAGS  := -I$(top_srcdir)/include
logwrapper_CFLAGS  += -I$(top_srcdir)/logwrapper/include
logwrapper_LDADD   := liblogwrap.la
logwrapper_LDADD   += -lcutils -llog -lpthread

# Export headers
logwrapper_includedir=$(includedir)/logwrapper
logwrapper_include_HEADERS := $(top_srcdir)/logwrapper/include/logwrap/logwrap.h

pkgconfigdir := $(libdir)/pkgconfig
pkgconfig_DATA := logwrapper.pc
EXTRA_DIST := $(pkgconfig_DATA)

