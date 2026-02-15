root_sbindir       := "/sbin"
root_sbin_PROGRAMS := leprop-service

leprop_service_SOURCES  = property_service.c
leprop_service_SOURCES += ll.c

leprop_service_CPPFLAGS = -I$(top_srcdir)/include
leprop_service_CPPFLAGS += -I$(top_srcdir)/libcutils

leprop_service_LDFLAGS  = -llog -lcutils

root_etcdir      := "/etc/"
root_etc_SCRIPTS := proptrigger.sh
root_etc_SCRIPTS += proptrigger.conf

install-data-hook:
	chmod  go-x $(DESTDIR)$(root_etcdir)/proptrigger.sh
	chmod  go-x $(DESTDIR)$(root_etcdir)/proptrigger.conf
