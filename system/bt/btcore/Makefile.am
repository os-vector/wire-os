# sources and intermediate files are separated

AM_CFLAGS = -Wall \
        -Wundef \
        -Wstrict-prototypes \
        -Wno-trigraphs \
        -std=c99 \
        -DHAS_NO_BDROID_BUILDCFG

AM_CPPFLAGS = \
        -I../ \
        -Iinclude

c_sources = \
        src/bdaddr.c \
        src/device_class.c \
        src/module.c \
        src/osi_module.c \
        src/property.c \
        src/uuid.c \
        src/hal_util.c

common_LDFLAGS = -lpthread  -lrt
common_CFLAGS =  $(AM_CFLAGS) -fPIC -D_GNU_SOURCE

library_includedir = $(pkgincludedir)\
library_include_HEADERS = $(h_sources)
lib_LTLIBRARIES = libbtcore.la
libbtcore_la_CC = @CC@
libbtcore_la_SOURCES = $(c_sources)
libbtcore_la_CPPFLAGS = $(AM_CPPFLAGS)
libbtcore_la_CFLAGS = $(common_CFLAGS)
libbtcore_la_LDFLAGS = $(common_LDFLAGS) -static
