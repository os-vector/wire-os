# sources and intermediate files are separated

AM_CFLAGS = -Wall \
        -Wundef \
        -Wstrict-prototypes \
        -Wno-trigraphs \
        -std=c99 \
        -DHAS_NO_BDROID_BUILDCFG

AM_CPPFLAGS= \
        -I/.. \
        -I/include \
        -I/../btcore/include \
        -I/../include \
        -I/../osi/include \
        -I$(bdroid_C_INCLUDES)

c_sources = \
        src/manager.c

common_LDFLAGS = -lpthread
common_CFLAGS =  $(AM_CFLAGS) -fPIC -D_GNU_SOURCE

library_includedir = $(pkgincludedir)
lib_LTLIBRARIES = libbtprofile.la
libbtprofile_la_CC = @CC@
libbtprofile_la_SOURCES = $(c_sources)
libbtprofile_la_CPPFLAGS = $(AM_CPPFLAGS)
libbtprofile_la_CFLAGS = $(common_CFLAGS)
libbtprofile_la_LDFLAGS = $(common_LDFLAGS) -static
