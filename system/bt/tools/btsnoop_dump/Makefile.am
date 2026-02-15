# sources and intermediate files are separated

AM_CFLAGS = -Wall \
        -Wundef \
        -Wstrict-prototypes \
        -Wno-trigraphs \
        -std=c99

# HAL layer
c_sources = \
        btsnoop_dump.c

common_CFLAGS =  $(AM_CFLAGS) -fPIC -D_GNU_SOURCE

library_includedir = $(pkgincludedir)
bin_PROGRAMS = btsnoop
btsnoop_CC = @CC@
btsnoop_SOURCES = $(c_sources)
btsnoop_CFLAGS = $(common_CFLAGS)
btsnoop_LDADD = -ldl -lrt ../../osi/libbtosi.la
