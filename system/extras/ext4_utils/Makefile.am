libext4_utils_c_sources := make_ext4fs.c
libext4_utils_c_sources += ext4fixup.c
libext4_utils_c_sources += ext4_utils.c
libext4_utils_c_sources += allocate.c
libext4_utils_c_sources += contents.c
libext4_utils_c_sources += extent.c
libext4_utils_c_sources += indirect.c
libext4_utils_c_sources += sha1.c
libext4_utils_c_sources += wipe.c
libext4_utils_c_sources += crc16.c
libext4_utils_c_sources += ext4_sb.c

libext4_utils_cpp_sources := key_control.cpp
libext4_utils_cpp_sources += ext4_crypt.cpp
libext4_utils_cpp_sources += unencrypted_properties.cpp

if BUILD_DEVICE
# Build a shared lib
lib_LTLIBRARIES := libext4_utils.la

libext4_utils_la_SOURCES  := $(libext4_utils_c_sources) $(libext4_utils_cpp_sources)
libext4_utils_la_CPPFLAGS := $(LIBSELINUX_CFLAGS) $(LIBSPARSE_CFLAGS)
libext4_utils_la_CPPFLAGS += -std=c++11
libext4_utils_la_LDFLAGS  := $(LIBSPARSE_LIBS)
libext4_utils_la_LDFLAGS  += -lcutils -lsparse -lselinux
endif

if BUILD_HOST
# Build a shared lib
lib_LTLIBRARIES := libext4_utils.la

libext4_utils_la_SOURCES  := $(libext4_utils_c_sources)
libext4_utils_la_CPPFLAGS := $(LIBSELINUX_CFLAGS) $(LIBSPARSE_CFLAGS)
libext4_utils_la_LDFLAGS  := $(LIBSPARSE_LIBS)
libext4_utils_la_LDFLAGS  += -lcutils -lsparse -lselinux

### Build make_ext4fs binary
bin_PROGRAMS = make_ext4fs

make_ext4fs_SOURCES := make_ext4fs_main.c canned_fs_config.c
make_ext4fs_CFLAGS  := $(AM_CFLAGS)
make_ext4fs_CFLAGS  += -DANDROID
make_ext4fs_CFLAGS  += -DHOST
make_ext4fs_LDADD   := libext4_utils.la
make_ext4fs_LDADD   += -lcutils -lsparse
endif

ext4_utils_includedir=$(includedir)/ext4_utils
ext4_utils_include_HEADERS := ext4.h
ext4_utils_include_HEADERS += ext4_crypt_init_extensions.h
ext4_utils_include_HEADERS += ext4_extents.h
ext4_utils_include_HEADERS += ext4_kernel_headers.h
ext4_utils_include_HEADERS += ext4_sb.h
ext4_utils_include_HEADERS += ext4_utils.h
ext4_utils_include_HEADERS += jbd2.h
ext4_utils_include_HEADERS += make_ext4fs.h
ext4_utils_include_HEADERS += wipe.h
ext4_utils_include_HEADERS += xattr.h

