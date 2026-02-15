# sources and intermediate files are separated

AM_CFLAGS = -Wall \
        -Wundef \
        -Wstrict-prototypes \
        -Wno-trigraphs \
        -std=c99 \
        -DHAS_NO_BDROID_BUILDCFG

AM_CPPFLAGS = -Iinclude -I../include \
        -I../btcore/include \
        -I../stack/include \
        -I../utils/include \
		-I../bta/include

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

if BTC_SUPPORT
AM_CFLAGS += -DBTC_INCLUDED=TRUE
AM_CPPFLAGS += -DBTC_INCLUDED=TRUE
endif

if SSR_CLEANUP_SUPPORT
AM_CFLAGS += -DSSR_CLEANUP=TRUE
endif

c_sources = \
        src/btsnoop.c \
        src/btsnoop_mem.c \
        src/btsnoop_net.c \
        src/buffer_allocator.c \
        src/hci_audio.c \
        src/hci_hal.c \
        src/hci_hal_h4.c \
        src/hci_hal_mct.c \
        src/hci_inject.c \
        src/hci_layer.c \
        src/hci_packet_factory.c \
        src/hci_packet_parser.c \
        src/low_power_manager.c \
        src/packet_fragmenter.c \
        src/vendor.c

common_LDFLAGS = -lpthread -lrt
common_CFLAGS =  $(AM_CFLAGS) -fPIC -D_GNU_SOURCE -DBTSNOOP_DEFAULT=TRUE -DBT_NET_DEBUG=TRUE

library_includedir = $(pkgincludedir)\
library_include_HEADERS = $(h_sources)
lib_LTLIBRARIES = libbt-hci.la
libbt_hci_la_CC = @CC@
libbt_hci_la_SOURCES = $(c_sources)
libbt_hci_la_CPPFLAGS = $(AM_CPPFLAGS)
libbt_hci_la_CFLAGS = $(common_CFLAGS)
libbt_hci_la_LDFLAGS := $(common_LDFLAGS) -static
