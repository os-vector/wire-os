AM_CFLAGS = -Wundef \
        -Wstrict-prototypes \
        -Wno-trigraphs \
        -g -O0 \
        -fno-inline \
        -fno-short-enums \
        -fpic \
        -I${top_srcdir}/include

AM_CPPFLAGS = $(AM_CFLAGS)

ACLOCAL_AMFLAGS = -I m4

### Build libsparse library

libsparse_h_sources = include/sparse/sparse.h \
        sparse_crc32.h

libsparse_c_sources := append2simg.c \
        backed_block.c \
        output_file.c \
        sparse.c \
        sparse_crc32.c \
        sparse_err.c \
        sparse_read.c

libsparse_la_SOURCES = $(libsparse_c_sources) $(libsparse_h_sources)
libsparse_la_LDFLAGS = $(Z_LIB)

### Export library
lib_LTLIBRARIES = libsparse.la

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = libsparse.pc
EXTRA_DIST = $(pkgconfig_DATA)

### Export headers
library_includedir = $(includedir)/sparse
library_include_HEADERS = $(libsparse_h_sources)


if IMG_CONVERT_UTILS
bin_PROGRAMS:=simg2img \
             img2simg \
             append2simg

simg2img_c_sources := simg2img.c \
        sparse_crc32.c
simg2img_CFLAGS := $(AM_CFLAGS) \
                   -Wno-strict-prototypes
simg2img_LDADD =libsparse.la
simg2img_LDFLAGS = @Z_LIB@

#bin_PROGRAMS=img2simg
img2simg_c_sources := img2simg.c
img2simg_CFLAGS := $(AM_CFLAGS) \
                   -Wno-strict-prototypes
img2simg_LDADD =libsparse.la
img2simg_LDFLAGS = @Z_LIB@

#bin_PROGRAMS=append2simg
append2simg_c_sources := append2simg.c
append2simg_CFLAGS := $(AM_CFLAGS) \
                      -Wno-strict-prototypes
append2simg_LDADD =libsparse.la
append2simg_LDFLAGS = @Z_LIB@
endif
