# Build shared lib.
lib_LTLIBRARIES := libsync.la

libsync_la_SOURCES := sync.c
libsync_la_CFLAGS  := -I$(top_srcdir)/include
libsync_la_CFLAGS  += -I$(top_srcdir)/libsync/include
libsync_la_LDFLAGS := -llog
if USE_GLIB
libsync_la_CFLAGS  += $(GLIB_CFLAGS) -include glib.h -Dstrlcpy=g_strlcpy
libsync_la_LDFLAGS += $(GLIB_LIBS)
endif
# Build sync_test
bin_PROGRAMS := sync_test

sync_test_SOURCES := sync.c sync_test.c
sync_test_CFLAGS  := -I$(top_srcdir)/include
sync_test_CFLAGS  += -I$(top_srcdir)/libsync/include
sync_test_LDADD   := libsync.la
sync_test_LDADD   += -llog -lpthread
if USE_GLIB
sync_test_CFLAGS  += $(GLIB_CFLAGS) -include glib.h -Dstrlcpy=g_strlcpy
sync_test_LDADD   += $(GLIB_LIBS)
endif
# Export headers
sync_includedir=$(includedir)/sync
sync_include_HEADERS := $(top_srcdir)/libsync/include/sync/sync.h

pkgconfigdir := $(libdir)/pkgconfig
pkgconfig_DATA := libsync.pc
EXTRA_DIST := $(pkgconfig_DATA)

