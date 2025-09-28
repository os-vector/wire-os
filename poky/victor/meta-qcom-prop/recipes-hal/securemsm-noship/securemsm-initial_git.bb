inherit qlicense qprebuilt

DESCRIPTION = "Provide QSEEComAPI Headers required from securemsm"

PR = "r22"

SRC_DIR = "${WORKSPACE}/security/securemsm"
FILESPATH =+ "${WORKSPACE}/:"
S = "${UNPACKDIR}/security/securemsm"

ALLOW_EMPTY:${PN} = "1"

do_install() {
   mkdir -p ${D}/usr/include
   cp -pPr ${WORKSPACE}/security/securemsm/QSEEComAPI/QSEEComAPI.h ${D}/usr/include
   cp -pPr ${WORKSPACE}/security/securemsm/drm/drmprov/inc/drmprov_clnt.h ${D}/usr/include
   cp -pPr ${WORKSPACE}/security/securemsm/drm/drmprov/inc/drmprov_entry.h ${D}/usr/include
   cp -pPr ${WORKSPACE}/security/securemsm/drm/playready/inc/drmresults.h ${D}/usr/include
   cp -pPr ${WORKSPACE}/security/securemsm/drm/playready/inc/drmtypes.h ${D}/usr/include
   cp -pPr ${WORKSPACE}/security/securemsm/drm/playready/inc/playready_entry.h ${D}/usr/include
   cp -pPr ${WORKSPACE}/security/securemsm/drm/playready/inc/pr_clnt.h ${D}/usr/include
   cp -pPr ${WORKSPACE}/security/securemsm/tzcommon/inc/tzcommon_entry.h ${D}/usr/include
   cp -pPr ${WORKSPACE}/security/securemsm/tzcommon/inc/app_main.h ${D}/usr/include
   cp -pPr ${WORKSPACE}/security/securemsm/sse/SecureUILib/SecureUILib.h ${D}/usr/include

}


