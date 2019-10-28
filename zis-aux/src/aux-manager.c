

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/

#include <metal/metal.h>
#include <metal/stddef.h>
#include <metal/stdio.h>
#include <metal/stdlib.h>
#include <metal/string.h>

#ifndef _LP64
#error LP64 is supported only
#endif

#ifndef METTLE
#error Non-metal C code is not supported
#endif

#include "zowetypes.h"
#include "as.h"
#include "collections.h"
#include "cmutils.h"
#include "crossmemory.h"
#include "pc.h"
#include "shrmem64.h"
#include "zos.h"

#include "aux-host.h"
#include "aux-utils.h"
#include "aux-manager.h"

#define STOKEN_MAP_BACKBONE 257
#define NICKNAME_MAP_BACKBONE 257
#define COMM_POOL_PSPACE  16
#define COMM_POOL_SSPACE  4

#define DEFAULT_HOST_STC "ZWESAUX "

int zisauxMgrInit(ZISAUXManager *mgr, int *reasonCode) {

  memset(mgr, 0, sizeof(ZISAUXManager));
  memcpy(mgr->eyecatcher, ZISAUX_MGR_EYECATCHER, sizeof(mgr->eyecatcher));

  mgr->managerTCB = getTCB();
  mgr->hostSTC = (ZISAUXSTCName){ DEFAULT_HOST_STC };

  mgr->ascbToComm = lhtCreate(STOKEN_MAP_BACKBONE, NULL);
  if (mgr->ascbToComm == NULL) {
    return RC_ZISAUX_ERROR;
  }

  mgr->nicknameToComm = lhtCreate(STOKEN_MAP_BACKBONE, NULL);
  if (mgr->nicknameToComm == NULL) {
    lhtDestroy(mgr->ascbToComm);
    mgr->ascbToComm = NULL;
    return RC_ZISAUX_ERROR;
  }

  CPHeader commCPHeader;
  mgr->commPool = cellpoolBuild(COMM_POOL_PSPACE, COMM_POOL_SSPACE,
                                sizeof(ZISAUXCommArea),
                                CROSS_MEMORY_SERVER_SUBPOOL,
                                CROSS_MEMORY_SERVER_KEY,
                                &commCPHeader);
  if (mgr->commPool == -1) {
    lhtDestroy(mgr->ascbToComm);
    mgr->ascbToComm = NULL;
    lhtDestroy(mgr->nicknameToComm);
    mgr->nicknameToComm = NULL;
    return RC_ZISAUX_CPOOL_ALLOC_FAILED;
  }

  mgr->sharedStorageToken = shrmem64GetAddressSpaceToken();

  return RC_ZISAUX_OK;
}

int zisauxMgrClean(ZISAUXManager *mgr, int *reasonCode) {

  if (memcmp(mgr->eyecatcher, ZISAUX_MGR_EYECATCHER,
             sizeof(mgr->eyecatcher))) {
    return RC_ZISAUX_BAD_EYECATCHER;
  }

  if (mgr->commPool != 0) {
    CPID cellPoolToFree = mgr->commPool;
    mgr->commPool = 0;
    cellpoolDelete(cellPoolToFree);
  }

  if (mgr->sharedStorageToken != 0) {
    MemObjToken tokenToFree = mgr->sharedStorageToken;
    mgr->sharedStorageToken = 0;
    int allocRC = RC_SHRMEM64_OK, allocRSN = 0;
    allocRC = shrmem64ReleaseAll(tokenToFree, &allocRSN);
    if (allocRC != RC_SHRMEM64_OK) {
      *reasonCode = allocRSN;
      return RC_ZISAUX_SHR64_ERROR;
    }
  }

  memset(mgr, 0, sizeof(ZISAUXManager));

  return RC_ZISAUX_OK;
}

static void auxTermExit(void *parm) {
  ZISAUXCommArea *comm = parm;
  auxutilPost(&comm->termECB, 0);
}

static ZISAUXCommArea *allocCommArea(ZISAUXManager *mgr) {

  ZISAUXCommArea *commArea = cellpoolGet(mgr->commPool, false);
  if (commArea == NULL) {
    return NULL;
  }
  memset(commArea, 0, sizeof(ZISAUXCommArea));
  memcpy(commArea->eyecatcher, ZISAUX_COMM_EYECATCHER,
         sizeof(commArea->eyecatcher));

  return commArea;
}

static void freeCommArea(ZISAUXManager *mgr, ZISAUXCommArea *area) {
  cellpoolFree(mgr->commPool, area);
}

void zisauxMgrSetHostSTC(ZISAUXManager *mgr, ZISAUXSTCName hostSTC) {
  mgr->hostSTC = hostSTC;
}

int zisauxMgrStartGuest(ZISAUXManager *mgr,
                        ZISAUXNickname guestNickname,
                        ZISAUXModule guestModuleName,
                        const ZISAUXParm *guestParm,
                        int *reasonCode, int traceLevel) {

  if (memcmp(mgr->eyecatcher, ZISAUX_MGR_EYECATCHER,
             sizeof(mgr->eyecatcher))) {
    return RC_ZISAUX_BAD_EYECATCHER;
  }

  if (getTCB() != mgr->managerTCB) {
   return RC_ZISAUX_NOT_MANAGER_TCB;
  }

 ZISAUXCommArea *commArea = allocCommArea(mgr);
 if (commArea == NULL) {
   return RC_ZISAUX_COMM_NOT_ALLOCATED;
 }

 ASParmString startCmd;
 startCmd.length = sizeof(mgr->hostSTC.name);
 memcpy(startCmd.text, mgr->hostSTC.name, startCmd.length);

 ASParm hostParm = {0};
 int printRC = snprintf(hostParm.data, sizeof(hostParm.data),
                        ":=),MOD=%s,COMM=%08X,%s",
                        guestModuleName.text, commArea,
                        guestParm ? guestParm->value : "");
 if (printRC < 0) {
   freeCommArea(mgr, commArea);
   commArea = NULL;
   return RC_ZISAUX_PRINT_ERROR;
 }
 hostParm.length = strlen(hostParm.data);

 int wasProblem = supervisorMode(TRUE);

 ASOutputData asCreateResult = {0};
 int startRC = 0, startRSN = 0;
 uint32_t ascreAttributes = AS_ATTR_NOSWAP | AS_ATTR_REUSASID;
 startRC = addressSpaceCreateWithTerm(&startCmd, &hostParm, ascreAttributes,
                                      &asCreateResult, auxTermExit, commArea,
                                      &startRSN);

 if (wasProblem) {
   supervisorMode(FALSE);
 }

 if (startRC != 0) {
   freeCommArea(mgr, commArea);
   commArea = NULL;
   return RC_ZISAUX_AS_NOT_STARTED;
 }

 union {
   int64 intValue;
   struct {
     char padding[4];
     ZISAUXNickname nickname;
   };
 } nicknameKey = {.padding = "\0\0\0\0", .nickname = guestNickname};

 lhtPut(mgr->nicknameToComm, nicknameKey.intValue, commArea);

 union {
   int64 intValue;
   struct {
     char padding[4];
     ASCB * __ptr32 ascb;
   };
 } ascbKey = {0};
 ascbKey.ascb = asCreateResult.ascb;

 lhtPut(mgr->ascbToComm, ascbKey.intValue, commArea);

 commArea->stoken = asCreateResult.stoken;
 commArea->ascb = asCreateResult.ascb;
 commArea->parentASID = getASCB()->ascbasid;

 return RC_ZISAUX_OK;
}

int zisauxMgrStopGuest(ZISAUXManager *mgr,
                       ZISAUXNickname guestNickname,
                       int *reasonCode, int traceLevel) {

  if (memcmp(mgr->eyecatcher, ZISAUX_MGR_EYECATCHER,
             sizeof(mgr->eyecatcher))) {
    return RC_ZISAUX_BAD_EYECATCHER;
  }

  union {
    int64 intValue;
    struct {
      char padding[4];
      ZISAUXNickname nickname;
    };
  } nicknameKey = {.padding = "\0\0\0\0", .nickname = guestNickname};

  ZISAUXCommArea *commArea = lhtGet(mgr->nicknameToComm, nicknameKey.intValue);
  if (commArea == NULL) {
    return RC_ZISAUX_COMM_NOT_FOUND;
  }

  uint64_t stoken = commArea->stoken;

  int wasProblem = supervisorMode(TRUE);

  int termRC = 0, termRSN = 0;
  termRC = addressSpaceTerminate(stoken, &termRSN);

  if (wasProblem) {
    supervisorMode(FALSE);
  }

  if (termRC != 0) {
    return RC_ZISAUX_AS_NOT_STOPPED;
  }

  auxutilWait(&commArea->termECB);

  lhtRemove(mgr->nicknameToComm, nicknameKey.intValue);

  union {
    int64 intValue;
    struct {
      char padding[4];
      ASCB * __ptr32 ascb;
    };
  } ascbKey = {0};
  ascbKey.ascb = commArea->ascb;

  lhtRemove(mgr->ascbToComm, ascbKey.intValue);

  freeCommArea(mgr, commArea);
  commArea = NULL;

  return RC_ZISAUX_OK;
}

static ZISAUXCommArea *getCommArea(ZISAUXManager *mgr,
                                   ZISAUXNickname guestNickname) {

  union {
    int64 intValue;
    struct {
      char padding[4];
      ZISAUXNickname nickname;
    };
  } nicknameKey = {.padding = "\0\0\0\0", .nickname = guestNickname};

  return lhtGet(mgr->nicknameToComm, nicknameKey.intValue);

}

int zisauxMgrSendTermSignal(ZISAUXManager *mgr,
                             ZISAUXNickname guestNickname,
                             int *reasonCode, int traceLevel) {

  if (memcmp(mgr->eyecatcher, ZISAUX_MGR_EYECATCHER,
             sizeof(mgr->eyecatcher))) {
    return RC_ZISAUX_BAD_EYECATCHER;
  }

  ZISAUXCommArea *commArea = getCommArea(mgr, guestNickname);
  if (commArea == NULL) {
    return RC_ZISAUX_COMM_NOT_FOUND;
  }

  if (commArea->flag & ZISAUX_HOST_FLAG_TERMINATED) {
    return RC_ZISAUX_AUX_STOPPED;
  }

  if (!(commArea->flag & ZISAUX_HOST_FLAG_READY)) {
    return RC_ZISAUX_AUX_NOT_READY;
  }

  ZISAUXCommServiceParmList parmList = {
    .eyecatcher = ZISAUX_COMM_SERVICE_PARM_EYE,
    .version = ZISAUX_COMM_SERVICE_PARM_VERSION,
    .function = ZISAUX_COMM_SERVICE_FUNC_TERM,
    .traceLevel = traceLevel,
  };

  int callRC = pcCallRoutine(commArea->pcNumber, commArea->sequenceNumber,
                             &parmList);

  *reasonCode = parmList.auxRSN;

  return callRC;
}

int zisauxMgrInitCommand(ZISAUXCommand *command,
                         CMSWTORouteInfo routeInfo,
                         const char *commandText) {

  size_t commandLength = strlen(commandText);
  if (commandLength > sizeof(command->text)) {
    RC_ZISAUX_COMMAND_TOO_LONG;
  }

  command->routeInfo = routeInfo;
  command->length = commandLength;
  memset(command->text, ' ', sizeof(command->text));
  memcpy(command->text, commandText, command->length);

  return RC_ZISAUX_OK;
}

void zisauxMgrCleanCommand(ZISAUXCommand *command) {

  memset(command, 0, sizeof(ZISAUXCommand));

}

int zisauxMgrInitRequestPayload(ZISAUXManager *mgr,
                                ZISAUXRequestPayload *payload,
                                void *requestData, size_t requestDataSize,
                                bool isRequestDataLocal,  int *reasonCode) {

  if (memcmp(mgr->eyecatcher, ZISAUX_MGR_EYECATCHER,
             sizeof(mgr->eyecatcher))) {
    return RC_ZISAUX_BAD_EYECATCHER;
  }

  memset(payload, 0, sizeof(*payload));
  char *copyBuffer = NULL;

  if (requestDataSize <= sizeof(payload->inlineData)) {

    copyBuffer = (char *)&payload->inlineData;

  } else {

    int shrRC = RC_SHRMEM64_OK, shrRSN = 0;
    shrRC = shrmem64Alloc(mgr->sharedStorageToken,
                          requestDataSize,
                          &payload->data,
                          &shrRSN);
    if (shrRC != RC_SHRMEM64_OK) {
      *reasonCode = shrRSN;
      return RC_ZISAUX_SHR64_ERROR;
    }

    shrRC = shrmem64GetAccess(mgr->sharedStorageToken,
                              payload->data,
                              &shrRSN);
    if (shrRC != RC_SHRMEM64_OK) {
      shrmem64Release(mgr->sharedStorageToken, payload->data, &shrRSN);
      *reasonCode = shrRSN;
      return RC_ZISAUX_SHR64_ERROR;
    }

    copyBuffer = payload->data;

  }

  if (isRequestDataLocal) {
    memcpy(copyBuffer, requestData, requestDataSize);
  } else {
    cmCopyFromSecondaryWithCallerKey(copyBuffer, requestData, requestDataSize);
  }
  payload->length = requestDataSize;

  return RC_ZISAUX_OK;
}

int zisauxMgrCleanRequestPayload(ZISAUXManager *mgr,
                                 ZISAUXRequestPayload *payload,
                                 int *reasonCode) {

  if (memcmp(mgr->eyecatcher, ZISAUX_MGR_EYECATCHER,
             sizeof(mgr->eyecatcher))) {
    return RC_ZISAUX_BAD_EYECATCHER;
  }

  if (payload->data != NULL) {

    int shrRC = RC_SHRMEM64_OK, shrRSN = 0;

    shrRC = shrmem64RemoveAccess(mgr->sharedStorageToken,
                                 payload->data,
                                 &shrRSN);
    /* Try to remove it from the system regardless of access */

    shrRC = shrmem64Release(mgr->sharedStorageToken,
                            payload->data,
                            &shrRSN);
    if (shrRC != RC_SHRMEM64_OK) {
      *reasonCode = shrRSN;
      return RC_ZISAUX_SHR64_ERROR;
    }

  }

  memset(payload, 0, sizeof(*payload));
  return RC_ZISAUX_OK;
}

int zisauxMgrInitRequestResponse(ZISAUXManager *mgr,
                                 ZISAUXRequestResponse *response,
                                 size_t bufferSize,
                                 int *reasonCode) {

  if (memcmp(mgr->eyecatcher, ZISAUX_MGR_EYECATCHER,
             sizeof(mgr->eyecatcher))) {
    return RC_ZISAUX_BAD_EYECATCHER;
  }

  memset(response, 0, sizeof(*response));
  char *copyBuffer = NULL;

  if (bufferSize <= sizeof(response->inlineData)) {

    copyBuffer = (char *)&response->inlineData;

  } else {

    int shrRC = RC_SHRMEM64_OK, shrRSN = 0;
    shrRC = shrmem64Alloc(mgr->sharedStorageToken,
                            bufferSize,
                            &response->data,
                            &shrRSN);
    if (shrRC != RC_SHRMEM64_OK) {
      *reasonCode = shrRSN;
      return RC_ZISAUX_SHR64_ERROR;
    }

    shrRC = shrmem64GetAccess(mgr->sharedStorageToken,
                              response->data,
                              &shrRSN);
    if (shrRC != RC_SHRMEM64_OK) {
      shrmem64Release(mgr->sharedStorageToken, response->data, &shrRSN);
      *reasonCode = shrRSN;
      return RC_ZISAUX_SHR64_ERROR;
    }

    copyBuffer = response->data;

  }

  response->length = bufferSize;

  return RC_ZISAUX_OK;
}

int zisauxMgrCopyRequestResponseData(void *dest, size_t destSize, bool isDestLocal,
                                     const ZISAUXRequestResponse *response,
                                     int *reasonCode) {

  if (destSize < response->length) {
    return RC_ZISAUX_BUFFER_TOO_SMALL;
  }

  const char *sourceData = response->data ? response->data : &response->inlineData;

  if (isDestLocal) {
    memcpy(dest, sourceData, response->length);
  } else {
    cmCopyToSecondaryWithCallerKey(dest, sourceData, response->length);
  }

  return RC_ZISAUX_OK;
}

int zisauxMgrCleanRequestResponse(ZISAUXManager *mgr,
                                  ZISAUXRequestResponse *response,
                                  int *reasonCode) {

  if (memcmp(mgr->eyecatcher, ZISAUX_MGR_EYECATCHER,
             sizeof(mgr->eyecatcher))) {
    return RC_ZISAUX_BAD_EYECATCHER;
  }

  if (response->data != NULL) {

    int shrRC = RC_SHRMEM64_OK, shrRSN = 0;

    shrRC = shrmem64RemoveAccess(mgr->sharedStorageToken,
                                 response->data,
                                 &shrRSN);
    /* Try to remove it from the system regardless of access */

    shrRC = shrmem64Release(mgr->sharedStorageToken,
                            response->data,
                            &shrRSN);
    if (shrRC != RC_SHRMEM64_OK) {
      *reasonCode = shrRSN;
      return RC_ZISAUX_SHR64_ERROR;
    }

  }

  memset(response, 0, sizeof(*response));
  return RC_ZISAUX_OK;
}

int zisauxMgrSendCommand(ZISAUXManager *mgr,
                         ZISAUXNickname guestNickname,
                         const ZISAUXCommand *command,
                         ZISAUXCommandReponse *response,
                         int *reasonCode, int traceLevel) {

  if (memcmp(mgr->eyecatcher, ZISAUX_MGR_EYECATCHER,
             sizeof(mgr->eyecatcher))) {
    return RC_ZISAUX_BAD_EYECATCHER;
  }

  ZISAUXCommArea *commArea = getCommArea(mgr, guestNickname);
  if (commArea == NULL) {
    return RC_ZISAUX_COMM_NOT_FOUND;
  }

  if (!(commArea->flag & ZISAUX_HOST_FLAG_READY)) {
    return RC_ZISAUX_AUX_NOT_READY;
  }

  ZISAUXCommServiceParmList parmList = {
    .eyecatcher = ZISAUX_COMM_SERVICE_PARM_EYE,
    .version = ZISAUX_COMM_SERVICE_PARM_VERSION,
    .function = ZISAUX_COMM_SERVICE_FUNC_COMMAND,
    .traceLevel = traceLevel,
  };

  ZISAUXCommand *auxCommand = (ZISAUXCommand *)&parmList.payload.inlineData;
  ZISAUXCommandReponse *auxResponse =
      (ZISAUXCommandReponse *)&parmList.response.inlineData;

  if (command->length > sizeof(auxCommand->text)) {
    return RC_ZISAUX_COMMAND_TOO_LONG;
  }

  *auxCommand = *command;
  auxResponse->status = CMS_MODIFY_COMMAND_STATUS_UNKNOWN;

  int callRC = pcCallRoutine(commArea->pcNumber, commArea->sequenceNumber,
                             &parmList);

  *response = *auxResponse;
  *reasonCode = parmList.auxRSN;

  return callRC;
}

int zisauxMgrSendWork(ZISAUXManager *mgr,
                      ZISAUXNickname guestNickname,
                      const ZISAUXRequestPayload *requestPayload,
                      ZISAUXRequestResponse *response,
                      int *reasonCode, int traceLevel) {

  if (memcmp(mgr->eyecatcher, ZISAUX_MGR_EYECATCHER,
             sizeof(mgr->eyecatcher))) {
    return RC_ZISAUX_BAD_EYECATCHER;
  }

  ZISAUXCommArea *commArea = getCommArea(mgr, guestNickname);
  if (commArea == NULL) {
    return RC_ZISAUX_COMM_NOT_FOUND;
  }

  if (!(commArea->flag & ZISAUX_HOST_FLAG_READY)) {
    return RC_ZISAUX_AUX_NOT_READY;
  }

  ZISAUXCommServiceParmList parmList = {
    .eyecatcher = ZISAUX_COMM_SERVICE_PARM_EYE,
    .version = ZISAUX_COMM_SERVICE_PARM_VERSION,
    .function = ZISAUX_COMM_SERVICE_FUNC_WORK,
    .payload = *requestPayload,
    .response = *response,
    .traceLevel = traceLevel,
  };

  int callRC = pcCallRoutine(commArea->pcNumber, commArea->sequenceNumber,
                             &parmList);

  *response = parmList.response;
  *reasonCode = parmList.auxRSN;

  return callRC;

}

int zisauxMgrWaitForTerm(struct ZISAUXManager_tag *mgr,
                         ZISAUXNickname guestNickname,
                         int *reasonCode, int traceLevel) {

  if (memcmp(mgr->eyecatcher, ZISAUX_MGR_EYECATCHER,
             sizeof(mgr->eyecatcher))) {
    return RC_ZISAUX_BAD_EYECATCHER;
  }

  ZISAUXCommArea *commArea = getCommArea(mgr, guestNickname);
  if (commArea == NULL) {
    return RC_ZISAUX_COMM_NOT_FOUND;
  }

  auxutilWait(&commArea->termECB);

  return RC_ZISAUX_OK;
}


/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/
