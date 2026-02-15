# Build shared lib.
lib_LTLIBRARIES := libpagemap.la

libpagemap_la_SOURCES := \
        pm_kernel.c \
        pm_process.c \
        pm_map.c \
        pm_memusage.c

libpagemap_la_CFLAGS := -I$(top_srcdir)/include
libpagemap_la_CFLAGS += -D_LARGEFILE64_SOURCE

# Export headers
h_sources = include/pagemap/pagemap.h

libpagemap_includedir=$(includedir)/pagemap
libpagemap_include_HEADERS = $(h_sources)
