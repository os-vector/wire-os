# sources and intermediate files are separated

AM_CFLAGS = -Wall \
        -Wundef \
        -Wstrict-prototypes \
        -Wno-trigraphs \
        -std=c99

AM_CPPFLAGS= \
        -I../../include \
        -I../../stack/include

# HAL layer
c_sources = \
        bluedroidtest.c

common_CFLAGS =  $(AM_CFLAGS) -fPIC -D_GNU_SOURCE

library_includedir = $(pkgincludedir)
bin_PROGRAMS = bdt
bdt_CC = @CC@
bdt_SOURCES = $(c_sources)
bdt_CFLAGS = $(common_CFLAGS)
bdt_LDFLAGS = $(Z_LIB)
bdt_LDADD = -lcutils -lhardware -lpthread -lm -ldl -lrt

