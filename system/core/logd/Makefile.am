root_sbindir       := "/sbin"
root_sbin_PROGRAMS := logd

logd_SOURCES = main.cpp \
    LogCommand.cpp \
    CommandListener.cpp \
    LogListener.cpp \
    LogReader.cpp \
    FlushCommand.cpp \
    LogBuffer.cpp \
    LogBufferElement.cpp \
    LogTimes.cpp \
    LogStatistics.cpp \
    LogWhiteBlackList.cpp \
    libaudit.c \
    LogAudit.cpp \
    LogKlog.cpp \
    event.logtags

logd_CPPFLAGS  = -I$(top_srcdir)/include
logd_CPPFLAGS += -I$(top_srcdir)/base/include
logd_CPPFLAGS += -std=c++14 -std=gnu++14
logd_CPPFLAGS += -fpermissive -fms-extensions
logd_CPPFLAGS += -DAUDITD_LOG_TAG=1003 -DLOGD_LOG_TAG=1004
logd_CPPFLAGS += -DHAVE_ANDROID_OS
if LOGDPRIVS
logd_CPPFLAGS += -DLOGD_DROP_PRIVS
endif

logd_LDADD  = ../libsysutils/libsysutils.la
logd_LDFLAGS = -llog -lcutils -lbase -lpthread -lutils
