bin_PROGRAMS = usb_composition_switch

# Program to switch USB compositions
# =========================================================
usb_composition_switch_CC = @CC@

usb_composition_switch_SOURCES = usb_composition_switch.c

usb_composition_switch_CFLAGS = -O2 \
                                -g \
                                -Wall \
                                -Wno-unused-parameter \
                                -fstack-protector-strong

usbhost_includedir=$(includedir)/usbhost
usbhost_include_HEADERS := $(top_srcdir)/include/usbhost/usbhost.h

