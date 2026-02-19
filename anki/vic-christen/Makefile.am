AM_CPPFLAGS =	-O2 \
		-D_GNU_SOURCE \
		-std=gnu++14

AM_CFLAGS =	-std=c11 \
		-Wall \
		-Wundef \
		-Wstrict-prototypes \
		-Wno-trigraphs \
		-DLINUX \
		-Wno-unused-result \
		-Wno-strict-aliasing \
		-fPIC \
		-fvisibility=hidden \
		-ffunction-sections \
		-fdata-sections \
		-fstack-protector-strong \
		-D_FORTIFY_SOURCE=2 \
		-Wformat \
		-Werror=format-security \
		-Wl,-z,relro \
		-Wl,-z,now \
		-Wl,-z,noexecstack \
		-Wl,-z,nocopyreloc \
		-pie \
		-fPIE	  

ACLOCAL_AMFLAGS = -I m4

AM_LDFLAGS := $(LDFLAGS) -lpthread

bin_PROGRAMS = vic-christen
vic_christen_CC = @CC@
vic_christen_SOURCES =	christen.cpp

pkgconfigdir = $(libdir)/pkgconfig
