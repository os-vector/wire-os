fastboot_version := $(shell git -C $(top_srcdir) rev-parse --short=12 HEAD 2>/dev/null)-android
fastboot_c_sources   := protocol.c engine.c utils.c fs.c
fastboot_cpp_sources := bootimg_utils.cpp fastboot.cpp

if HOST_OS_LINUX
fastboot_c_sources += usb_linux.c util_linux.c
endif

# Build fastboot tool for linux
bin_PROGRAMS = fastboot

fastboot_SOURCES := $(fastboot_c_sources) $(fastboot_cpp_sources)

fastboot_CFLAGS  := -Wall -Wextra -Werror -Wunreachable-code
fastboot_CFLAGS  += -DFASTBOOT_REVISION='"$(fastboot_version)"'

fastboot_LDFLAGS := -lpthread
fastboot_LDFLAGS += -lext4_utils -lsparse
fastboot_LDFLAGS += -llog -lbase -lz
#fastboot_LDFLAGS += -lutils
#fastboot_LDFLAGS += -lziparchive
