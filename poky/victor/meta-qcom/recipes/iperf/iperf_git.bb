SUMMARY = "iPerf - The ultimate speed test tool for TCP, UDP and SCTP"
DESCRIPTION = "iperf is a tool for active measurements of the maximum achievable \
bandwidth on IP networks.  It supports tuning of various parameters \
related to timing, protocols, and buffers.  For each test it reports \
the bandwidth, loss, and other parameters."
HOMEPAGE = "http://software.es.net/iperf/"

SECTION = "utils"

LICENSE = "BSD-3-Clause"
LIC_FILES_CHKSUM = "file://LICENSE;md5=8c3434c5a9a53c78c7739f0bc9e5adda"

inherit autotools

PV = "3.1.3+git${SRCPV}"

SRC_URI = "git://github.com/esnet/iperf.git;protocol=https;branch=3.1-STABLE \
           file://0001-remove-incompatible-gcc-arg.patch"

SRCREV = "274eaed5b17f664e4ac6c79f1ba854b55f15a3a3"

S = "${UNPACKDIR}/git"

do_compile:prepend () {
 cd ${S}
}

do_install:prepend () {
 cd ${S}
}

do_configure:prepend () {
#       damn picky automake...
        cd ${S}
        touch NEWS README AUTHORS COPYING ChangeLog
}
