

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

#ifndef ZIS_SERVER_H_
#define ZIS_SERVER_H_

#include "zowetypes.h"
#include "zos.h"

#define RC_ZIS_OK       0
#define RC_ZIS_ERROR    8

/*** Auth service ***/

#define ZIS_SERVICE_ID_AUTH_SRV                   11

ZOWE_PRAGMA_PACK
typedef struct SAFStatus_tag {
  int safRC;
  int racfRC;
  int racfRSN;
} SAFStatus;

#define FILL_SAF_STATUS($s, $safRC, $racfRC, $racfRsn) do {\
  ($s)->safRC = safRC; \
  ($s)->racfRC = racfRC; \
  ($s)->racfRSN = racfRsn; \
} while (0)

#define FORMAT_SAF_STATUS($s, $f) $f("SAF RC %d, RACF RC %d, RACF REASON %d",\
    ($s)->safRC, ($s)->racfRC, ($s)->racfRSN)

typedef struct AbendInfo_tag {
 int completionCode;
 int reasonCode;
} AbendInfo;

#define FORMAT_ABEND_INFO($ai, $f) $f("Abend: system completion code %d,"\
    " reason %d", ($ai)->completionCode, ($ai)->reasonCode)

#define ZIS_AUTH_SERVICE_USER_ID_MAX_LENGTH     8
#define ZIS_AUTH_SERVICE_PASSWORD_MAX_LENGTH    100
#define ZIS_AUTH_SERVICE_CLASS_MAX_LENGTH       8
#define ZIS_AUTH_SERVICE_ENTITY_MAX_LENGTH      255

typedef struct AuthServiceParmList_tag {
  char eyecatcher[8];
#define ZIS_AUTH_SERVICE_PARMLIST_EYECATCHER   "RSCMPASS"
  int fc;
#define ZIS_AUTH_SERVICE_PARMLIST_FC_VERIFY_PASSWORD 0
#define ZIS_AUTH_SERVICE_PARMLIST_FC_ENTITY_CHECK 1
#define ZIS_AUTH_SERVICE_PARMLIST_FC_GET_ACCESS 2
  char userIDNullTerm[ZIS_AUTH_SERVICE_USER_ID_MAX_LENGTH + 1];
  char passwordNullTerm[ZIS_AUTH_SERVICE_PASSWORD_MAX_LENGTH + 1];
  /* up to 8 characters: */
  char classNullTerm[ZIS_AUTH_SERVICE_CLASS_MAX_LENGTH + 1];
  char _padding0[1];
  int access;
  /* up to 255 characters: */
  char entityNullTerm[ZIS_AUTH_SERVICE_ENTITY_MAX_LENGTH + 1];
  union {
    SAFStatus safStatus;
    AbendInfo abendInfo;
  };
  int traceLevel;
} AuthServiceParmList;
ZOWE_PRAGMA_PACK_RESET

#define RC_ZIS_AUTHSRV_OK                         0
#define RC_ZIS_AUTHSRV_PARMLIST_NULL              8
#define RC_ZIS_AUTHSRV_BAD_EYECATCHER             9
#define RC_ZIS_AUTHSRV_DELETE_FAILED              10
#define RC_ZIS_AUTHSRV_CREATE_FAILED              11
#define RC_ZIS_AUTHSRV_UNKNOWN_FUNCTION_CODE      12
#define RC_ZIS_AUTHSRV_INPUT_STRING_TOO_LONG      13
#define RC_ZIS_AUTHSRV_INSTALL_RECOVERY_FAILED    14
#define RC_ZIS_AUTHSRV_SAF_ABENDED                15
#define RC_ZIS_AUTHSRV_SAF_ERROR                  16
#define RC_ZIS_AUTHSRV_SAF_NO_DECISION            17
#define RC_ZIS_AUTHSRV_USER_CLASS_NOT_READ        18
#define RC_ZIS_AUTHSRV_USER_CLASS_TOO_LONG        19
#define RC_ZIS_AUTHSRV_MAX_RC                     19

/*** Snarfer service ***/

#define ZIS_SERVICE_ID_SNARFER_SRV                12

ZOWE_PRAGMA_PACK
typedef struct SnarferServiceParmList_tag {
  char eyecatcher[8];
#define ZIS_SNARFER_SERVICE_PARMLIST_EYECATCHER "RSSNRFSR"
  uint64 destinationAddress;
  uint64 sourceAddress;
  ASCB * __ptr32 sourceASCB;
  unsigned int sourceKey;
  unsigned int size;
} SnarferServiceParmList;
ZOWE_PRAGMA_PACK_RESET

#define RC_ZIS_SNRFSRV_OK                         0
#define RC_ZIS_SNRFSRV_PARMLIST_NULL              8
#define RC_ZIS_SNRFSRV_BAD_EYECATCHER             9
#define RC_ZIS_SNRFSRV_BAD_DEST                   10
#define RC_ZIS_SNRFSRV_BAD_ASCB                   11
#define RC_ZIS_SNRFSRV_BAD_SIZE                   12
#define RC_ZIS_SNRFSRV_ECSA_ALLOC_FAILED          13
#define RC_ZIS_SNRFSRV_RECOVERY_ERROR             14
#define RC_ZIS_SNRFSRV_SRC_ASSB_ABEND             15
#define RC_ZIS_SNRFSRV_SRC_IEAMSCHD_ABEND         16
#define RC_ZIS_SNRFSRV_SRC_IEAMSCHD_FAILED        17
#define RC_ZIS_SNRFSRV_SHARED_OBJ_ALLOC_FAILED    18
#define RC_ZIS_SNRFSRV_SHARED_OBJ_SHARE_FAILED    19
#define RC_ZIS_SNRFSRV_SHARED_OBJ_DETACH_FAILED   20

/*** NWM Services ***/

#define ZIS_SERVICE_ID_NWM_SRV                    13

ZOWE_PRAGMA_PACK
typedef struct ZISNWMServiceParmList_tag {

  /* input fields */
  char eyecatcher[8];
#define ZIS_NWM_SERVICE_PARMLIST_EYECATCHER "RSZISNWM"
  int version;
  int flags;
  int traceLevel;

  /* output fields */
  char nmiJobName[8];
  PAD_LONG(0, char *nmiBuffer);
  unsigned int nmiBufferSize;

  /* output fields */
  int nmiReturnValue;
  int nmiReturnCode;
  int nmiReasonCode;

} ZISNWMServiceParmList;
ZOWE_PRAGMA_PACK_RESET

#define RC_ZIS_NWMSRV_OK                         0
#define RC_ZIS_NWMSRV_PARMLIST_NULL              8
#define RC_ZIS_NWMSRV_BAD_EYECATCHER             9
#define RC_ZIS_NWMSRV_BUFFER_NULL                10
#define RC_ZIS_NWMSRV_BUFFER_NOT_ALLOCATED       11
#define RC_ZIS_NWMSRV_RECOVERY_ERROR             12
#define RC_ZIS_NWMSRV_NWM_ABENDED                13
#define RC_ZIS_NWMSRV_NWM_FAILED                 14

#endif /* ZIS_SERVER_H_ */


/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

