libbase_sources := file.cpp
libbase_sources += stringprintf.cpp
libbase_sources += strings.cpp
libbase_sources += logging.cpp

# Build libbase.so
lib_LTLIBRARIES := libbase.la

libbase_la_SOURCES  := $(libbase_sources)
libbase_la_CPPFLAGS := -I@core_srcdir@/include
libbase_la_CPPFLAGS += -I$(srcdir)/include
if BUILD_DEVICE
libbase_la_CPPFLAGS += -std=c++14
endif
if BUILD_HOST
libbase_la_CPPFLAGS += -std=c++11
endif
libbase_la_LDFLAGS  := -lcutils

# Export headers
base_includedir=$(includedir)/base
base_include_HEADERS := $(srcdir)/include/base/file.h
base_include_HEADERS += $(srcdir)/include/base/logging.h
base_include_HEADERS += $(srcdir)/include/base/memory.h
base_include_HEADERS += $(srcdir)/include/base/strings.h
base_include_HEADERS += $(srcdir)/include/base/stringprintf.h

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = libbase.pc
EXTRA_DIST = $(pkgconfig_DATA)
