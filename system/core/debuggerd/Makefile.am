AM_CFLAGS := \
    -W \
    -Wall \
    -Wextra \
    -fpermissive \
    -Wunused

ACLOCAL_AMFLAGS = -I m4
debuggerd_sources := utility.cpp
debuggerd_sources += backtrace.cpp
debuggerd_sources += elf_utils.cpp
debuggerd_sources += getevent.cpp
debuggerd_sources += debuggerd.cpp
debuggerd_sources += tombstone.cpp
crasher_sources := crasher.c

if ARM
debuggerd32_sources := arm/machine.cpp
crasher32_sources := arm/crashglue.S

bindir       := "/bin"
bin_PROGRAMS := debuggerd
bin_PROGRAMS += crasher

debuggerd_SOURCES := $(debuggerd_sources)
debuggerd_SOURCES += $(debuggerd32_sources)
debuggerd_CPPFLAGS := $(AM_CFLAGS) -I$(top_srcdir)/include -I$(top_srcdir)/base/include
debuggerd_CPPFLAGS += -std=c++14  -nostartfiles
debuggerd_LDADD   := ../libbacktrace/libbacktrace.la
debuggerd_LDFLAGS := -llog -lselinux  -lext4_utils -lcutils -lpthread -lbase

crasher_SOURCES := $(crasher_sources)
crasher_SOURCES += $(crasher32_sources)

crasher_CPPFLAGS := -mfpu=neon  $(AM_CFLAGS) -I$(top_srcdir)/include
crasher_CPPFLAGS += -std=c++14 -DLE_UM
crasher_LDFLAGS := -llog -lcutils -lc -lext4_utils -lpthread
endif

if ARM64
debuggerd64_sources := arm64/machine.cpp
crasher64_sources := arm64/crashglue.S

bindir       := "/bin"
bin_PROGRAMS := debuggerd64
bin_PROGRAMS += crasher64

debuggerd64_SOURCES := $(debuggerd_sources)
debuggerd64_SOURCES += $(debuggerd64_sources)
debuggerd64_CPPFLAGS := $(AM_CFLAGS) -I$(top_srcdir)/include -I$(top_srcdir)/base/include
debuggerd64_CPPFLAGS += -std=c++14  -nostartfiles
debuggerd64_LDADD   := ../libbacktrace/libbacktrace.la
debuggerd64_LDFLAGS := -llog -lcutils -lext4_utils -lselinux -lpthread -lbase

crasher64_SOURCES := $(crasher_sources)
crasher64_SOURCES +=  $(crasher64_sources)
crasher64_CPPFLAGS := $(AM_CFLAGS) -I$(top_srcdir)/include
crasher64_CPPFLAGS += -std=c++14 -DLE_UM
crasher64_LDFLAGS := -llog -lcutils -lc -lext4_utils -lpthread
endif

