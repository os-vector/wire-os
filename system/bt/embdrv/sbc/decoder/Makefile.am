# sources and intermediate files are separated

AM_CFLAGS = -Wall \
        -Wundef \
        -Wstrict-prototypes \
        -Wno-trigraphs \
        -std=c99 \
        -DHAS_NO_BDROID_BUILDCFG

AM_CPPFLAGS = -Iinclude \
              -Isrce

c_sources = \
        ./srce/alloc.c \
        ./srce/bitalloc.c \
        ./srce/bitalloc-sbc.c \
        ./srce/bitstream-decode.c \
        ./srce/decoder-oina.c \
        ./srce/decoder-private.c \
        ./srce/decoder-sbc.c \
        ./srce/dequant.c \
        ./srce/framing.c \
        ./srce/framing-sbc.c \
        ./srce/oi_codec_version.c \
        ./srce/synthesis-sbc.c \
        ./srce/synthesis-dct8.c \
        ./srce/synthesis-8-generated.c

common_LDFLAGS = -lpthread  -lrt
#../stack/libbtstack.la
common_CFLAGS =  $(AM_CFLAGS) -fPIC -D_GNU_SOURCE

library_includedir = $(pkgincludedir)\
library_include_HEADERS = $(h_sources)
lib_LTLIBRARIES = libbt_qcom_sbc_decoder.la
libbt_qcom_sbc_decoder_la_CC = @CC@
libbt_qcom_sbc_decoder_la_SOURCES = $(c_sources)
libbt_qcom_sbc_decoder_la_CPPFLAGS =$(AM_CPPFLAGS)
libbt_qcom_sbc_decoder_la_CFLAGS = $(common_CFLAGS)
libbt_qcom_sbc_decoder_la_LDFLAGS := $(common_LDFLAGS) -static
