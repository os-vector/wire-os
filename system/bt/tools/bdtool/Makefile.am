# sources and intermediate files are separated

AM_CFLAGS = -Wall \
        -Wundef \
        -Wstrict-prototypes \
        -Wno-trigraphs \
        -std=c99

AM_CPPFLAGS= \
        -I../inc/ \
        -I../../include \
        -I../../stack/include \
        -I../../test/suite

# HAL layer
c_sources = \
        adapter.c \
        bdtool.c \
        ../../test/suite/support/callbacks.c \
        ../../test/suite/support/gatt.c \
        ../../test/suite/support/hal.c \
        ../../test/suite/support/pan.c

common_CFLAGS =  $(AM_CFLAGS) -fPIC -D_GNU_SOURCE

library_includedir = $(pkgincludedir)
bin_PROGRAMS = net_bdtool
net_bdtool_CC = @CC@
net_bdtool_SOURCES = $(c_sources)
net_bdtool_CFLAGS = $(common_CFLAGS)
net_bdtool_LDFLAGS = $(Z_LIB)
net_bdtool_LDADD = -ldl -lrt -lresolv ../../btcore/libbtcore.la \
    ../../osi/libbtosi.la $(SYS_LIB)/libbtvendorhal.la
