inherit autotools qcommon qprebuilt qlicense

DESCRIPTION = "Time Genoff Library"
PR = "r0"

SRC_DIR = "${WORKSPACE}/time-services/time-genoff"
FILESPATH =+ "${WORKSPACE}/:"
SRC_URI = "file://time-services/time-genoff"

S = "${UNPACKDIR}/time-services/time-genoff"
