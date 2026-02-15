common_sources := hashmap.c
common_sources += atomic.c.arm
common_sources += native_handle.c
common_sources += config_utils.c
common_sources += load_file.c
common_sources += strlcpy.c
common_sources += open_memstream.c
common_sources += strdup16to8.c
common_sources += strdup8to16.c
common_sources += record_stream.c
common_sources += process_name.c
common_sources += threads.c
common_sources += sched_policy.c
common_sources += iosched_policy.c
common_sources += str_parms.c
common_sources += fs_config.c

if HOST_OS_LINUX
common_sources += fs.c
common_sources += multiuser.c
common_sources += socket_inaddr_any_server.c
common_sources += socket_local_client.c
common_sources += socket_local_server.c
common_sources += socket_loopback_client.c
common_sources += socket_loopback_server.c
common_sources += socket_network_client.c
common_sources += sockets.c
endif

target_sources := android_reboot.c
target_sources += ashmem-dev.c
target_sources += debugger.c
target_sources += klog.c
target_sources += partition_utils.c
target_sources += properties.c
if LE_PROPERTIES
target_sources += property_ops.c
endif
target_sources += qtaguid.c
target_sources += trace-dev.c
target_sources += uevent.c

host_sources := ashmem-host.c
host_sources += trace-host.c
host_sources += dlmalloc_stubs.c

requiredlibs = -lpthread

# Build libcutils.so
lib_LTLIBRARIES := libcutils.la

libcutils_la_SOURCES  := $(common_sources)
libcutils_la_CPPFLAGS := -DHAVE_PTHREADS
libcutils_la_CPPFLAGS += -DLINUX_ENABLED
libcutils_la_CPPFLAGS += -D__linux__
libcutils_la_CPPFLAGS += -D_GNU_SOURCE
if LE_PROPERTIES
libcutils_la_CPPFLAGS += -DLE_PROPERTIES
endif
libcutils_la_LDFLAGS  := -lpthread -llog

if BUILD_HOST
libcutils_la_SOURCES  += $(host_sources)
endif

if BUILD_DEVICE
libcutils_la_SOURCES  += $(target_sources)
libcutils_la_CPPFLAGS += -DANDROID_SMP=0
libcutils_la_CPPFLAGS += -DHAVE_ASM_MEMSET
endif

if ARM
libcutils_la_SOURCES  += arch-arm/memset32.S
endif

if ARM64
libcutils_la_SOURCES  += arch-arm64/android_memset.S
endif

libcutils_la_LDFLAGS  := -lpthread -llog

# Export headers
cutils_includedir=$(includedir)/cutils
cutils_include_HEADERS := @core_incdir@/cutils/android_reboot.h
cutils_include_HEADERS += @core_incdir@/cutils/aref.h
cutils_include_HEADERS += @core_incdir@/cutils/ashmem.h
cutils_include_HEADERS += @core_incdir@/cutils/atomic.h
cutils_include_HEADERS += @core_incdir@/cutils/bitops.h
cutils_include_HEADERS += @core_incdir@/cutils/compiler.h
cutils_include_HEADERS += @core_incdir@/cutils/config_utils.h
cutils_include_HEADERS += @core_incdir@/cutils/debugger.h
cutils_include_HEADERS += @core_incdir@/cutils/fs.h
cutils_include_HEADERS += @core_incdir@/cutils/hashmap.h
cutils_include_HEADERS += @core_incdir@/cutils/iosched_policy.h
cutils_include_HEADERS += @core_incdir@/cutils/jstring.h
cutils_include_HEADERS += @core_incdir@/cutils/klog.h
cutils_include_HEADERS += @core_incdir@/cutils/list.h
cutils_include_HEADERS += @core_incdir@/cutils/log.h
cutils_include_HEADERS += @core_incdir@/cutils/memory.h
cutils_include_HEADERS += @core_incdir@/cutils/misc.h
cutils_include_HEADERS += @core_incdir@/cutils/multiuser.h
cutils_include_HEADERS += @core_incdir@/cutils/native_handle.h
cutils_include_HEADERS += @core_incdir@/cutils/open_memstream.h
cutils_include_HEADERS += @core_incdir@/cutils/partition_utils.h
cutils_include_HEADERS += @core_incdir@/cutils/process_name.h
cutils_include_HEADERS += @core_incdir@/cutils/properties.h
cutils_include_HEADERS += @core_incdir@/cutils/qtaguid.h
cutils_include_HEADERS += @core_incdir@/cutils/record_stream.h
cutils_include_HEADERS += @core_incdir@/cutils/sched_policy.h
cutils_include_HEADERS += @core_incdir@/cutils/sockets.h
cutils_include_HEADERS += @core_incdir@/cutils/stdatomic.h
cutils_include_HEADERS += @core_incdir@/cutils/str_parms.h
cutils_include_HEADERS += @core_incdir@/cutils/threads.h
cutils_include_HEADERS += @core_incdir@/cutils/trace.h
cutils_include_HEADERS += @core_incdir@/cutils/uevent.h

cutils_sys_includedir=$(includedir)/cutils/sys
cutils_sys_include_HEADERS := @core_incdir@/sys/capability.h
cutils_sys_include_HEADERS += @core_incdir@/sys/system_properties.h

privateheaders_includedir=$(includedir)/private
privateheaders_include_HEADERS := @core_incdir@/private/android_filesystem_config.h
privateheaders_include_HEADERS += @core_incdir@/private/android_filesystem_capability.h

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = libcutils.pc
EXTRA_DIST = $(pkgconfig_DATA)
