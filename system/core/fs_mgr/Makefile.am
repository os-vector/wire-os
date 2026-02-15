fs_mgr_c_sources := fs_mgr.c fs_mgr_verity.c fs_mgr_fstab.c fs_mgr_format.c

# Build libfs_mgr.so
lib_LTLIBRARIES := libfs_mgr.la

libfs_mgr_la_SOURCES := $(fs_mgr_c_sources)
libfs_mgr_la_CFLAGS  := -I$(top_srcdir)/include
libfs_mgr_la_CFLAGS  += -I$(top_srcdir)/fs_mgr/include
libfs_mgr_la_CFLAGS  += -I$(top_srcdir)/logwrapper/include
libfs_mgr_la_CFLAGS  += -DUSE_GLIB -Dstrlcat=g_strlcat @GLIB_CFLAGS@
libfs_mgr_la_LDFLAGS := -lm -lpthread -lcutils @GLIB_LIBS@
libfs_mgr_la_LDFLAGS += ../logwrapper/liblogwrap.la

# Build fs_mgr daemon
bin_PROGRAMS = fs_mgr

fs_mgr_SOURCES := fs_mgr_main.c
fs_mgr_CFLAGS  := -I$(top_srcdir)/fs_mgr/include
fs_mgr_LDADD   := libfs_mgr.la -lcutils -llog -lext4_utils -lmincrypt

# Export headers
fs_mgr_includedir=$(includedir)/fs_mgr
fs_mgr_include_HEADERS := $(top_srcdir)/fs_mgr/include/fs_mgr.h

pkgconfigdir := $(libdir)/pkgconfig
pkgconfig_DATA := fs_mgr.pc
EXTRA_DIST := $(pkgconfig_DATA)

