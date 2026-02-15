# sources and intermediate files are separated

AM_CFLAGS = -Wall \
        -Wundef \
        -Wstrict-prototypes \
        -Wno-trigraphs \
        -std=c99 \
        -DHAS_NO_BDROID_BUILDCFG

AM_CPPFLAGS= \
        -Iinclude \
        -I../btcore/include \
        -I../stack/include \
        -I../utils/include \
        -I../include \
        -I../ \
        -I$(bdroid_C_INCLUDES)

if MDM_ROME
AM_CFLAGS += -DBT_SOC_TYPE_ROME
AM_CPPFLAGS += -DBT_SOC_TYPE_ROME
else
if MDM_PRONTO
AM_CFLAGS += -DHCI_USE_MCT
AM_CPPFLAGS += -DHCI_USE_MCT
endif
endif

if MDM_CHEROKEE
AM_CFLAGS += -DBT_SOC_TYPE_CHEROKEE
AM_CPPFLAGS += -DBT_SOC_TYPE_CHEROKEE
endif

c_sources = \
        src/bt_utils.c

common_LDFLAGS = -lpthread ../osi/libbtosi.la
common_CFLAGS =  $(AM_CFLAGS) -fPIC

library_includedir = $(pkgincludedir)
lib_LTLIBRARIES = libbtutils.la
libbtutils_la_CC = @CC@
libbtutils_la_SOURCES = $(c_sources)
libbtutils_la_CPPFLAGS =$(AM_CPPFLAGS)
libbtutils_la_CFLAGS = $(common_CFLAGS)
libbtutils_la_LDFLAGS = $(common_LDFLAGS) -static
