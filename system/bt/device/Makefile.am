# sources and intermediate files are separated

AM_CFLAGS = -Wall \
        -Wundef \
        -Wstrict-prototypes \
        -Wno-trigraphs \
        -std=c99 \
        -DHAS_NO_BDROID_BUILDCFG

AM_CPPFLAGS = \
        -I../ \
        -Iinclude \
        -I../btcore/include \
        -I../hci/include \
        -I../include \
        -I../stack/include \
		-I$(WORKSPACE)/vendor/qcom/opensource/bluetooth/system_bt_ext

c_sources = \
        src/classic/peer.c \
        src/controller.c \
		$(WORKSPACE)/vendor/qcom/opensource/bluetooth/system_bt_ext/device/src/interop.c

common_LDFLAGS = -lpthread
common_CFLAGS =  $(AM_CFLAGS) -fPIC -D_GNU_SOURCE

library_includedir = $(pkgincludedir)\
library_include_HEADERS = $(h_sources)
lib_LTLIBRARIES = libbtdevice.la
libbtdevice_la_CC = @CC@
libbtdevice_la_SOURCES = $(c_sources)
libbtdevice_la_CPPFLAGS = $(AM_CPPFLAGS)
libbtdevice_la_CFLAGS = $(common_CFLAGS)
libbtdevice_la_LDFLAGS = $(common_LDFLAGS) -static
