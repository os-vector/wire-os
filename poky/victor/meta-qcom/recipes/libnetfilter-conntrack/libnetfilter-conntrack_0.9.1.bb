DESCRIPTION = "Userspace library providing a programming interface (API) to \ 
the in-kernel connection tracking state table"
LICENSE = "GPLv2+"
LIC_FILES_CHKSUM = "file://COPYING;md5=8ca43cbc842c2336e835926c2166c28b"
DEPENDS = "libnfnetlink"

PR = "r0"

SRC_URI = \
"http://www.netfilter.org/projects/libnetfilter_conntrack/files/libnetfilter_conntrack-${PV}.tar.bz2"

S = "${UNPACKDIR}/libnetfilter_conntrack-${PV}"
 
inherit autotools pkgconfig

SRC_URI[md5sum] = "b7506cbb7580433859809d8eac53a199"
SRC_URI[sha256sum] = "11d33e720a8da692933965fc1eca3dec758bb04d6c06038f4db98c79d9b955ea"
