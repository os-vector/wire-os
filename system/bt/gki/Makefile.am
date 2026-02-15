# sources and intermediate files are separated

AM_CFLAGS = -Wall \
        -Wundef \
        -Wstrict-prototypes \
        -Wno-trigraphs \
        -std=c99 \
        -DHAS_NO_BDROID_BUILDCFG

AM_CPPFLAGS = \
        -I../ \
        -Icommon \
        -Iulinux \
        -I../btcore/include \
        -I../include \
        -I../stack/include \
        -I../osi/include \
        -I../utils/include

c_sources = \
        ./common/gki_buffer.c \
        ./ulinux/gki_ulinux.c

common_LDFLAGS = -lpthread
common_CFLAGS =  $(AM_CFLAGS) -fPIC -D_GNU_SOURCE

library_includedir = $(pkgincludedir)\
library_include_HEADERS = $(h_sources)
lib_LTLIBRARIES = libbtgki.la
libbtgki_la_CC = @CC@
libbtgki_la_SOURCES = $(c_sources)
libbtgki_la_CPPFLAGS = $(AM_CPPFLAGS)
libbtgki_la_CFLAGS = $(common_CFLAGS)
libbtgki_la_LDFLAGS = $(common_LDFLAGS) -static
