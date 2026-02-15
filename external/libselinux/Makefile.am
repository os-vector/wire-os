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

### Build libselinux library

libselinux_h_sources = include/selinux/selinux.h include/selinux/label.h include/selinux/context.h include/selinux/avc.h include/selinux/android.h

libselinux_host_sources := \
    src/callbacks.c \
    src/check_context.c \
    src/freecon.c \
    src/init.c \
    src/label.c \
    src/label_file.c \
    src/label_android_property.c

libselinux_c_sources := \
    src/booleans.c \
    src/canonicalize_context.c \
    src/disable.c \
    src/enabled.c \
    src/fgetfilecon.c \
    src/fsetfilecon.c \
    src/getenforce.c \
    src/getfilecon.c \
    src/getpeercon.c \
    src/lgetfilecon.c \
    src/load_policy.c \
    src/lsetfilecon.c \
    src/policyvers.c \
    src/procattr.c \
    src/setenforce.c \
    src/setfilecon.c \
    src/context.c \
    src/mapping.c \
    src/stringrep.c \
    src/compute_create.c \
    src/compute_av.c \
    src/avc.c \
    src/avc_internal.c \
    src/avc_sidtab.c \
    src/get_initial_context.c \
    src/checkAccess.c \
    src/sestatus.c \
    src/deny_unknown.c

libselinux_la_SOURCES := $(libselinux_host_sources)
if BUILD_DEVICE
libselinux_la_SOURCES += $(libselinux_c_sources) src/android.c
libselinux_la_CFLAGS  := -DAUDITD_LOG_TAG=1003
libselinux_la_CFLAGS  += -D_GNU_SOURCE
endif
libselinux_la_LDFLAGS := $(PCRE_LIB) -lcutils -lmincrypt

### Export library
lib_LTLIBRARIES = libselinux.la

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = libselinux.pc
EXTRA_DIST = $(pkgconfig_DATA)

library_includedir = $(pkgincludedir)/selinux
library_include_HEADERS = $(libselinux_h_sources)
