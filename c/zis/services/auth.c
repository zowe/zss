

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/


#include <metal/metal.h>
#include <metal/stddef.h>
#include <metal/string.h>

#ifndef METTLE
#error Non-metal C code is not supported
#endif

#include "zowetypes.h"
#include "crossmemory.h"
#include "recovery.h"
#include "zos.h"

#include "zis/utils.h"
#include "zis/services/auth.h"

#define ZIS_PARMLIB_PARM_AUTH_USER_CLASS    CMS_PROD_ID".AUTH.CLASS"

static int handleVerifyPassword(AuthServiceParmList *parmList,
                                const CrossMemoryServerGlobalArea *globalArea) {
  ACEE *acee = NULL;
  int safRC = 0, racfRC = 0, racfRsn = 0;
  int deleteSAFRC = 0, deleteRACFRC = 0, deleteRACFRsn = 0;
  int rc = RC_ZIS_AUTHSRV_OK;
  IDTA *idta = NULL;
  int options = VERIFY_CREATE;

  if (parmList->options & ZIS_AUTH_SERVICE_PARMLIST_OPTION_GENERATE_IDT) {
    idta = (IDTA *) safeMalloc31(sizeof(IDTA), "Idta structure");
    memset(idta, 0, sizeof(IDTA));
    memcpy(idta->id, "IDTA", 4);
    idta->version = IDTA_VERSION_0001;
    idta->length = sizeof(IDTA);
    idta->idtType = IDTA_JWT_IDT_Type;
    idta->idtBufferPtr = parmList->safIdt;
    idta->idtBufferLen = sizeof(parmList->safIdt);
    idta->idtLen = parmList->safIdtLen;
    idta->idtPropIn = IDTA_End_User_IDT;
    options |= VERIFY_GENERATE_IDT;
  }

  CMS_DEBUG(globalArea, "handleVerifyPassword(): username = %s, password = %s\n",
      parmList->userIDNullTerm, "******");
  if (idta == NULL) {
    safRC = safVerify(VERIFY_CREATE, parmList->userIDNullTerm,
      parmList->passwordNullTerm, &acee, &racfRC, &racfRsn);
  } else {
    safRC = safVerify6(options, parmList->userIDNullTerm,
      parmList->passwordNullTerm, &acee, &racfRC, &racfRsn, idta);
  }
  CMS_DEBUG(globalArea, "safVerify(VERIFY_CREATE) safStatus = %d, RACF RC = %d, "
      "RSN = %d, ACEE=0x%p\n", safRC, racfRC, racfRsn, acee);
  if (idta != NULL) {
    CMS_DEBUG(globalArea, "IDTA token: gen_rc = %d, prop_out = %X, prop_in = %X "
      "token length = %d\n", idta->idtGenRc, idta->idtPropOut, idta->idtPropIn,
      idta->idtLen);
  }

  if (safRC != 0) {
    rc = RC_ZIS_AUTHSRV_SAF_ERROR;
    goto acee_deleted;
  }

  if (idta != NULL) {
    parmList->safIdtLen = idta->idtLen;
  }

  deleteSAFRC = safVerify(VERIFY_DELETE, NULL, NULL, &acee, &deleteRACFRC,
      &deleteRACFRsn);
  CMS_DEBUG(globalArea, "safVerify(VERIFY_DELETE) safStatus = %d, RACF RC = %d, "
      "RSN = %d, ACEE=0x%p\n", deleteSAFRC, deleteRACFRC, deleteRACFRsn,
      acee);
  if (deleteSAFRC != 0) {
    rc = RC_ZIS_AUTHSRV_DELETE_FAILED;
  }
acee_deleted:

  FILL_SAF_STATUS(&parmList->safStatus, safRC, racfRC, racfRsn);
  CMS_DEBUG(globalArea, "handleVerifyPassword() done\n");
  if (idta != NULL) {
    safeFree(idta, sizeof(IDTA));
    idta = NULL;
  }
  return rc;
}

typedef struct AuthClass_tag {
  char valueNullTerm[ZIS_AUTH_SERVICE_CLASS_MAX_LENGTH + 1];
} AuthClass;

static int getDefaultClassValue(const CrossMemoryServerGlobalArea *globalArea,
                                AuthClass *class) {

  CrossMemoryServerConfigParm userClassParm = {0};
  int getParmRC = cmsGetConfigParm(&globalArea->serverName,
                                   ZIS_PARMLIB_PARM_AUTH_USER_CLASS,
                                   &userClassParm);
  if (getParmRC != RC_CMS_OK) {
    return RC_ZIS_AUTHSRV_USER_CLASS_NOT_READ;
  }
  if (userClassParm.valueLength > sizeof(class->valueNullTerm) - 1) {
    return RC_ZIS_AUTHSRV_USER_CLASS_TOO_LONG;
  }

  memcpy(class->valueNullTerm, userClassParm.charValueNullTerm,
         userClassParm.valueLength);
  class->valueNullTerm[userClassParm.valueLength] = '\0';

  return RC_ZIS_AUTHSRV_OK;
}

static void fillAbendInfo(struct RecoveryContext_tag * __ptr32 context,
                          SDWA * __ptr32 sdwa, void * __ptr32 userData) {
  AbendInfo *abendInfo = userData;

  recoveryGetABENDCode(sdwa, &abendInfo->completionCode,
      &abendInfo->reasonCode);
}

static int handleEntityCheck(AuthServiceParmList *parmList,
                             const CrossMemoryServerGlobalArea *globalArea) {
  ACEE *acee = NULL;
  int safRC = 0, racfRC = 0, racfRsn = 0;
  int rcvRC = 0;
  int rc = RC_ZIS_AUTHSRV_OK;

  AuthClass defaultClass = {0};
  int getClassRC = getDefaultClassValue(globalArea, &defaultClass);
  if (getClassRC != RC_ZIS_AUTHSRV_OK) {
    return getClassRC;
  }

  size_t classLength = strlen(parmList->classNullTerm);
  if (classLength != 0) {
    if (strcmp(parmList->classNullTerm, defaultClass.valueNullTerm) != 0) {
      return RC_ZIS_AUTHSRV_CUSTOM_CLASS_NOT_ALLOWED;
    }
  }
  AuthClass class = defaultClass;

  CMS_DEBUG(globalArea, "handleEntityCheck(): user = %s, entity = %s, class = %s,"
      " access = %x\n", parmList->userIDNullTerm, parmList->entityNullTerm,
      class.valueNullTerm, parmList->access);
  safRC = safVerify(VERIFY_CREATE | VERIFY_WITHOUT_PASSWORD,
      parmList->userIDNullTerm, NULL, &acee, &racfRC, &racfRsn);
  CMS_DEBUG(globalArea, "safVerify(VERIFY_CREATE) safStatus = %d, RACF RC = %d, "
      "RSN = %d, ACEE=0x%p\n", safRC, racfRC, racfRsn, acee);
  if (safRC != 0) {
    rc = RC_ZIS_AUTHSRV_CREATE_FAILED;
    FILL_SAF_STATUS(&parmList->safStatus, safRC, racfRC, racfRsn);
    goto acee_deleted;
  }

  rcvRC = recoveryPush("safAuth()",
      RCVR_FLAG_RETRY | RCVR_FLAG_DELETE_ON_RETRY, NULL,
      &fillAbendInfo,
      &parmList->abendInfo,  NULL, NULL);
  if (rcvRC != RC_RCV_OK) {
    rc = (rcvRC == RC_RCV_ABENDED)? RC_ZIS_AUTHSRV_SAF_ABENDED :
        RC_ZIS_AUTHSRV_INSTALL_RECOVERY_FAILED;
    CMS_DEBUG(globalArea, "recovery: rc %d, abend code: %d\n", rcvRC,
        parmList->abendInfo.completionCode);
    goto recovery_removed;
  }
  safRC = safAuth(0, class.valueNullTerm, parmList->entityNullTerm,
      parmList->access, acee, &racfRC, &racfRsn);
  CMS_DEBUG(globalArea, "safAuth safStatus = %d, RACF RC = %d, RSN = %d, "
      "ACEE=0x%p\n", safRC, racfRC, racfRsn, acee);
  if (safRC != 0) {
    rc = RC_ZIS_AUTHSRV_SAF_ERROR;
  }
  FILL_SAF_STATUS(&parmList->safStatus, safRC, racfRC, racfRsn);
  recoveryPop();
recovery_removed:

  safRC  = safVerify(VERIFY_DELETE, NULL, NULL, &acee, &racfRC, &racfRsn);
  CMS_DEBUG(globalArea, "safVerify(VERIFY_DELETE) safStatus = %d, RACF RC = %d, "
      "RSN = %d, ACEE=0x%p\n", safRC, racfRC, racfRsn, acee);
  if (safRC != 0) {
    rc = RC_ZIS_AUTHSRV_DELETE_FAILED;
  }
acee_deleted:

  CMS_DEBUG(globalArea, "handleEntityCheck() done\n");
  return rc;
}

static int getAccessType(const CrossMemoryServerGlobalArea *globalArea,
                         const char *classNullTerm,
                         const char *idNullTerm,
                         const char *entityNullTerm,
                         int *accessType,
                         SAFAuthStatus *safStatus,
                         AbendInfo *abendInfo,
                         int traceLevel) {

  ACEE *acee = NULL;
  int rcvRC = 0;
  int safRC = 0;
  int racfRC = 0;
  int racfRsn = 0;
  int rc = RC_ZIS_AUTHSRV_OK;
  int accessTypeInternal = 0;

  safRC = safVerify(VERIFY_CREATE |
                    VERIFY_WITHOUT_PASSWORD |
                    VERIFY_WITHOUT_LOG,
                    (char *)idNullTerm, NULL, &acee, &racfRC, &racfRsn);

  CMS_DEBUG2(globalArea, traceLevel,
             "safVerify(VERIFY_CREATE) safStatus = %d, RACF RC = %d, "
             "RSN = %d, ACEE = 0x%p\n",
             safRC, racfRC, racfRsn, acee);

  FILL_SAF_STATUS(safStatus, safRC, racfRC, racfRsn);

  if (safRC != 0) {
    return RC_ZIS_AUTHSRV_CREATE_FAILED;
  }

  rcvRC = recoveryPush("safAuth()",
                       RCVR_FLAG_RETRY | RCVR_FLAG_DELETE_ON_RETRY, NULL,
                       &fillAbendInfo,
                       abendInfo,  NULL, NULL);

  if (rcvRC == RC_RCV_OK) {

    safRC = safAuthStatus(0, (char *)classNullTerm, (char *)entityNullTerm,
                          &accessTypeInternal, acee, &racfRC, &racfRsn);
    CMS_DEBUG2(globalArea, traceLevel,
               "safAuth accessType = %d, safStatus = %d, "
               "RACF RC = %d, RSN = %d, ACEE = 0x%p\n",
               accessTypeInternal, safRC, racfRC, racfRsn, acee);

    FILL_SAF_STATUS(safStatus, safRC, racfRC, racfRsn);

    if (safRC != 0) {
      rc = RC_ZIS_AUTHSRV_SAF_ERROR;
    }

    recoveryPop();

  } else {

    rc = (rcvRC == RC_RCV_ABENDED) ?
         RC_ZIS_AUTHSRV_SAF_ABENDED : RC_ZIS_AUTHSRV_INSTALL_RECOVERY_FAILED;
    CMS_DEBUG2(globalArea, traceLevel, "recovery: rc %d, abend code: %d\n",
               rcvRC, abendInfo->completionCode);

  }

  safRC  = safVerify(VERIFY_DELETE | VERIFY_WITHOUT_LOG, NULL, NULL, &acee,
                     &racfRC, &racfRsn);

  CMS_DEBUG2(globalArea, traceLevel,
             "safVerify(VERIFY_DELETE) safStatus = %d, RACF RC = %d, "
             "RSN = %d, ACEE=0x%p\n", safRC, racfRC, racfRsn, acee);

  if (safRC != 0) {
    if (rc == RC_ZIS_AUTHSRV_OK) {
      FILL_SAF_STATUS(safStatus, safRC, racfRC, racfRsn);
      rc = RC_ZIS_AUTHSRV_DELETE_FAILED;
    }
  }

  *accessType = accessTypeInternal;
  return rc;
}

static int handleAccessRetrieval(AuthServiceParmList *parmList,
                                 const CrossMemoryServerGlobalArea *globalArea)
{
  ACEE *acee = NULL;
  int safRC = 0, racfRC = 0, racfRsn = 0;
  int rcvRC = 0;
  int rc = RC_ZIS_AUTHSRV_OK;
  int traceLevel = parmList->traceLevel;

  AuthClass defaultClass = {0};
  int getClassRC = getDefaultClassValue(globalArea, &defaultClass);
  if (getClassRC != RC_ZIS_AUTHSRV_OK) {
    return getClassRC;
  }

  size_t classLength = strlen(parmList->classNullTerm);
  if (classLength != 0) {
    if (strcmp(parmList->classNullTerm, defaultClass.valueNullTerm) != 0) {
      return RC_ZIS_AUTHSRV_CUSTOM_CLASS_NOT_ALLOWED;
    }
  }
  AuthClass class = defaultClass;

  CMS_DEBUG2(globalArea, traceLevel,
             "handleAccessRetrieval(): user=\'%s\', entity=\'%s\', "
             "class=\'%s\', access = 0x%p\n",
             parmList->userIDNullTerm, parmList->entityNullTerm,
             class.valueNullTerm, parmList->access);

  rc = getAccessType(globalArea,
                     class.valueNullTerm,
                     parmList->userIDNullTerm,
                     parmList->entityNullTerm,
                     &parmList->access,
                     &parmList->safStatus,
                     &parmList->abendInfo,
                     traceLevel);

  CMS_DEBUG2(globalArea, traceLevel, "handleAccessRetrieval() done\n");

  return rc;
}

int zisAuthServiceFunction(CrossMemoryServerGlobalArea *globalArea,
                           CrossMemoryService *service, void *parm) {

  int logLevel = globalArea->pcLogLevel;

  if (logLevel >= ZOWE_LOG_DEBUG) {
    cmsPrintf(&globalArea->serverName, CMS_LOG_DEBUG_MSG_ID
              " in authServiceFunction, parm = 0x%p\n", parm);
  }

  void *clientParmAddr = parm;
  if (clientParmAddr == NULL) {
    return RC_ZIS_AUTHSRV_PARMLIST_NULL;
  }

  AuthServiceParmList localParmList;
  cmCopyFromPrimaryWithCallerKey(&localParmList, clientParmAddr,
                                 sizeof(AuthServiceParmList));

  if (memcmp(localParmList.eyecatcher, ZIS_AUTH_SERVICE_PARMLIST_EYECATCHER,
             sizeof(localParmList.eyecatcher))) {
    return RC_ZIS_AUTHSRV_BAD_EYECATCHER;
  }
  int handlerRC = RC_ZIS_AUTHSRV_OK;
  switch (localParmList.fc) {
  case ZIS_AUTH_SERVICE_PARMLIST_FC_VERIFY_PASSWORD:
    handlerRC = handleVerifyPassword(&localParmList, globalArea);
    break;
  case ZIS_AUTH_SERVICE_PARMLIST_FC_ENTITY_CHECK:
    handlerRC = handleEntityCheck(&localParmList, globalArea);
    break;
  case ZIS_AUTH_SERVICE_PARMLIST_FC_GET_ACCESS:
    handlerRC = handleAccessRetrieval(&localParmList, globalArea);
    break;
  default:
    handlerRC = RC_ZIS_AUTHSRV_UNKNOWN_FUNCTION_CODE;
  }
  cmCopyToPrimaryWithCallerKey(clientParmAddr, &localParmList,
                               sizeof(AuthServiceParmList));
  return handlerRC;
}


/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/

