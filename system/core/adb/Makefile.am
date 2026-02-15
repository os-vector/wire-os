adb_sources := adb.cpp
adb_sources += adb_auth.cpp
adb_sources += adb_io.cpp
adb_sources += adb_listeners.cpp
adb_sources += adb_utils.cpp
adb_sources += sockets.cpp
adb_sources += transport.cpp
adb_sources += transport_local.cpp
adb_sources += transport_usb.cpp

adb_linux_sources := fdevent.cpp
adb_linux_sources += get_my_path_linux.cpp
adb_linux_sources += usb_linux.cpp

adb_version := $(shell git -C $(top_srcdir) rev-parse --short=12 HEAD 2>/dev/null)-android

# Device daemon and lib.
# =========================================================
if BUILD_DEVICE
# Build libadbd.so
lib_LTLIBRARIES := libadbd.la

libadbd_la_SOURCES := $(adb_sources)
libadbd_la_SOURCES += adb_auth_client.cpp
libadbd_la_SOURCES += fdevent.cpp
libadbd_la_SOURCES += jdwp_service.cpp
libadbd_la_SOURCES += usb_linux_client.cpp
#libadbd_la_SOURCES += qemu_tracing.cpp

libadbd_la_CPPFLAGS := -I$(top_srcdir)/include
libadbd_la_CPPFLAGS += -I$(top_srcdir)/base/include
libadbd_la_CPPFLAGS += -I$(top_srcdir)/fs_mgr/include
libadbd_la_CPPFLAGS += -Wall -Wno-unused-parameter
libadbd_la_CPPFLAGS += -Wno-deprecated-declarations
libadbd_la_CPPFLAGS += -std=c++14 -fpermissive
libadbd_la_CPPFLAGS += -DUSE_GLIB -Dstrlcat=g_strlcat @GLIB_CFLAGS@
libadbd_la_CPPFLAGS += -DADB_HOST=0
libadbd_la_CPPFLAGS += -DADB_REVISION='"$(adb_version)"'

libadbd_la_LIBADD   := ../fs_mgr/libfs_mgr.la
libadbd_la_LDFLAGS  := -lselinux -lcutils -lext4_utils -lmincrypt -lresolv -lbase

# Build adb daemon
# Prefer adbd to be located in /sbin for historical reasons
root_sbindir       := "/sbin"
root_sbin_PROGRAMS := adbd

adbd_SOURCES  := adb_main.cpp
adbd_SOURCES  += services.cpp
adbd_SOURCES  += file_sync_service.cpp
adbd_SOURCES  += framebuffer_service.cpp
adbd_SOURCES  += remount_service.cpp
adbd_SOURCES  += set_verity_enable_state_service.cpp

adbd_CPPFLAGS := -I$(top_srcdir)/include
adbd_CPPFLAGS += -I$(top_srcdir)/base/include
adbd_CPPFLAGS += -I$(top_srcdir)/fs_mgr/include
adbd_CPPFLAGS += -Wall -Wno-unused-parameter
adbd_CPPFLAGS += -Wno-deprecated-declarations
adbd_CPPFLAGS += -std=c++14 -fpermissive
adbd_CPPFLAGS += -D_GNU_SOURCE
adbd_CPPFLAGS += -DADB_HOST=0
adbd_CPPFLAGS += -DADB_REVISION='"$(adb_version)"'
adbd_CPPFLAGS += -DALLOW_ADBD_NO_AUTH=1
adbd_CPPFLAGS += -DSYSTEM_SIZE=@SYSTEM_SIZE@
adbd_CPPFLAGS += -DFEC_SIZE=@FEC_SIZE@

if ADB_ROOT
adbd_CPPFLAGS += -DALLOW_ADBD_ROOT=1
endif
adbd_CPPFLAGS += -DADB_REBOOT_ENABLED=1
adbd_CPPFLAGS += -DALLOW_ADBD_DISABLE_VERITY=1
adbd_CPPFLAGS += -DUSE_GLIB -Dstrlcat=g_strlcat @GLIB_CFLAGS@

adbd_LDADD   := libadbd.la
adbd_LDADD   += ../fs_mgr/libfs_mgr.la
adbd_LDFLAGS := -lcutils -lresolv -lmincrypt -lselinux -lbase
endif

# adb host tool for linux
# =========================================================
if BUILD_HOST
# Build libadb
lib_LTLIBRARIES := libadb.la

libadb_la_SOURCES := $(adb_sources) $(adb_linux_sources)
libadb_la_SOURCES += adb_auth_host.cpp

libadbd_la_CPPFLAGS := -I$(top_srcdir)/include
libadbd_la_CPPFLAGS += -I$(top_srcdir)/base/include
libadbd_la_CPPFLAGS += -Wall -Wno-unused-parameter
libadbd_la_CPPFLAGS += -std=c++14 -fpermissive
libadbd_la_CPPFLAGS += -DADB_HOST
libadbd_la_CPPFLAGS += -DADB_REVISION='"$(adb_version)"'
libadbd_la_LDFLAGS   = -lbase

# Build adb for linux
#bin_PROGRAMS = adb

endif
