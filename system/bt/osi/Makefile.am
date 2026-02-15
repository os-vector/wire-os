# sources and intermediate files are separated

AM_CFLAGS = -Wall \
        -Wundef \
        -Wstrict-prototypes \
        -Wno-trigraphs \
        -std=c99 \
        -DHAS_NO_BDROID_BUILDCFG \
        -D__GLIBC__

AM_CPPFLAGS = \
        -I../ \
        -I../utils/include \
        -I../stack/include

c_sources = \
        src/alarm.c \
        src/allocation_tracker.c \
        src/allocator.c \
        src/array.c \
        src/buffer.c \
        src/config.c \
        src/compat.c \
        src/data_dispatcher.c \
        src/eager_reader.c \
        src/fixed_queue.c \
        src/future.c \
        src/hash_functions.c \
        src/hash_map.c \
        src/hash_map_utils.c \
        src/list.c \
        src/mutex.c \
        src/osi.c \
        src/properties.c \
        src/reactor.c \
        src/ringbuffer.c \
        src/semaphore.c \
        src/socket.c \
        src/socket_utils/socket_local_client.c \
        src/socket_utils/socket_local_server.c \
        src/thread.c \
        src/time.c \
        src/wakelock.c \
        src/vnd_log.c \
        src/log.c \
        src/properties_compat.c

common_LDFLAGS = -lpthread
common_CFLAGS =  $(AM_CFLAGS) -fPIC -D_GNU_SOURCE

library_includedir = $(pkgincludedir)\
library_include_HEADERS = $(h_sources)
lib_LTLIBRARIES = libbtosi.la
libbtosi_la_CC = @CC@
libbtosi_la_SOURCES = $(c_sources)
libbtosi_la_CPPFLAGS = $(AM_CPPFLAGS)
libbtosi_la_CFLAGS = $(common_CFLAGS)
libbtosi_la_LDFLAGS = $(common_LDFLAGS) -static
