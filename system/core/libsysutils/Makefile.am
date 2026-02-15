# Build lisysutils.so
lib_LTLIBRARIES := libsysutils.la

libsysutils_la_SOURCES  := \
                  src/SocketListener.cpp      \
                  src/FrameworkListener.cpp   \
                  src/FrameworkCommand.cpp    \
                  src/SocketClient.cpp        \
                  src/ServiceManager.cpp      \
                  EventLogTags.logtags

libsysutils_la_CPPFLAGS := -I$(top_srcdir)/include
libsysutils_la_CPPFLAGS += -std=c++14 -std=gnu++14 -fpermissive
libsysutils_la_LDFLAGS   = -lcutils -llog

requiredlibs = -lpthread
