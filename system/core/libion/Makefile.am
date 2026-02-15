AM_CFLAGS := \
    -W \
    -Wall \
    -Wextra \
    -Wunused \
    -Werror
ACLOCAL_AMFLAGS = -I m4

if USE_LEGACYION
ion_sources = ion.c
else
ion_sources = ion_new.c
endif

# Build libion.so
lib_LTLIBRARIES = libion.la

libion_la_SOURCES := $(ion_sources)
libion_la_CFLAGS := -I$(top_srcdir)/include
libion_la_CFLAGS += $(AM_CFLAGS) -Wno-error=pointer-to-int-cast
libion_la_CFLAGS += -include stddef.h
if USE_LEGACYION
libion_la_CFLAGS += -I$(top_srcdir)/kernel-headers
libion_la_CFLAGS += -DLEGACY_ION
else
libion_la_CFLAGS += -include ion_new.h
endif
libion_la_LDFLAGS := -llog

bindir       := "/bin"
bin_PROGRAMS := iontest

iontest_SOURCES := $(ion_sources)
iontest_SOURCES += ion_test.c
iontest_CFLAGS := -I$(top_srcdir)/include
iontest_CFLAGS += $(AM_CFLAGS) -Wno-error=strict-aliasing -Wno-error=pointer-to-int-cast
iontest_CFLAGS += -include stddef.h
if USE_LEGACYION
iontest_CFLAGS += -I$(top_srcdir)/kernel-headers
iontest_CFLAGS += -DLEGACY_ION
else
iontest_CFLAGS += -include ion_new.h
endif
iontest_LDFLAGS := -llog

# Export headers
ion_includedir=$(includedir)/ion_headers/ion
ion_linux_includedir=$(includedir)/ion_headers/linux
ion_include_HEADERS := $(top_srcdir)/include/ion/ion.h
ion_linux_include_HEADERS := $(top_srcdir)/kernel-headers/linux/ion.h
ion_linux_include_HEADERS += $(top_srcdir)/kernel-headers/linux/ion_test.h

pkgconfigdir := $(libdir)/pkgconfig
pkgconfig_DATA := libion.pc
EXTRA_DIST := $(pkgconfig_DATA)
