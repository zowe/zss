

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/

#ifndef ZIS_SERVICES_AUTH_H_
#define ZIS_SERVICES_AUTH_H_

#include "safIdService.h"

#define ZIS_SERVICE_ID_AUTH_SRV                   11

ZOWE_PRAGMA_PACK
typedef struct SAFAuthStatus_tag {
  int safRC;
  int racfRC;
  int racfRSN;
} SAFAuthStatus;

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
#define ZIS_AUTH_SERVICE_APPL_MAX_LENGTH     8

#define SAF_IDT_SERVICE_CURRENT_VERSION         1

typedef struct AuthServiceParmList_tag {
  char eyecatcher[8];
#define ZIS_AUTH_SERVICE_PARMLIST_EYECATCHER   "RSCMPASS"
  int fc;
#define ZIS_AUTH_SERVICE_PARMLIST_FC_VERIFY_PASSWORD 0
#define ZIS_AUTH_SERVICE_PARMLIST_FC_ENTITY_CHECK 1
#define ZIS_AUTH_SERVICE_PARMLIST_FC_GET_ACCESS 2
#define ZIS_AUTH_SERVICE_PARMLIST_FC_GENERATE_TOKEN 3
  char userIDNullTerm[ZIS_AUTH_SERVICE_USER_ID_MAX_LENGTH + 1];
  char passwordNullTerm[ZIS_AUTH_SERVICE_PASSWORD_MAX_LENGTH + 1];
  /* up to 8 characters: */
  char classNullTerm[ZIS_AUTH_SERVICE_CLASS_MAX_LENGTH + 1];
  char options;

  int access;
  /* up to 255 characters: */
  char entityNullTerm[ZIS_AUTH_SERVICE_ENTITY_MAX_LENGTH + 1];
  union {
    SAFAuthStatus safStatus;
    AbendInfo abendInfo;
  };
  int traceLevel;
  SafIdtService safIdtService;
} AuthServiceParmList;
ZOWE_PRAGMA_PACK_RESET

#pragma map(zisAuthServiceFunction, "ZISSAUTH")
int zisAuthServiceFunction(CrossMemoryServerGlobalArea *globalArea,
                           CrossMemoryService *service, void *parm);

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
#define RC_ZIS_AUTHSRV_CUSTOM_CLASS_NOT_ALLOWED   20
#define RC_ZIS_AUTHSRV_INPUT_STRING_TOO_SHORT     21
#define RC_ZIS_AUTHSRV_BAD_SAF_SERVICE_VERSION    22
#define RC_ZIS_AUTHSRV_IDT_NULL                   23
#define RC_ZIS_AUTHSRV_MAX_RC                     23

#endif /* ZIS_SERVICES_AUTH_H_ */


/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/
