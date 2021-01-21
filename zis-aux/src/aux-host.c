

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/

#include <metal/metal.h>
#include <metal/stdbool.h>
#include <metal/stddef.h>
#include <metal/stdio.h>
#include <metal/stdint.h>
#include <metal/stdlib.h>
#include <metal/string.h>

#include "zowetypes.h"
#include "alloc.h"
#include "as.h"
#include "le.h"
#include "logging.h"
#include "metalio.h"
#include "pc.h"
#include "qsam.h"
#include "shrmem64.h"
#include "stcbase.h"
#include "utils.h"
#include "recovery.h"
#include "resmgr.h"
#include "scheduling.h"
#include "zos.h"

#include "zis/message.h"
#include "zis/parm.h"

#include "aux-guest.h"
#include "aux-host.h"
#include "aux-utils.h"

#ifndef _LP64
#error LP64 is supported only
#endif

#ifndef METTLE
#error Non-metal C code is not supported
#endif

static void initLogging(void) {
  logConfigureDestination2(NULL, LOG_DEST_PRINTF_STDOUT, "printf(stdout)",
                           NULL, auxutilPrintWithPrefix,
                           auxutilDumpWithEmptyMessageID);
  logConfigureComponent(NULL, LOG_COMP_STCBASE, "STCBASE",
                        LOG_DEST_PRINTF_STDOUT, ZOWE_LOG_INFO);
  printf("DATESTAMP              JOBNAME  ASCB    (ASID) TCB       MSGID     MSGTEXT\n");

}

static void printStartMessage(void) {
  zowelog(NULL, LOG_COMP_STCBASE, ZOWE_LOG_INFO, ZISAUX_LOG_STARTUP_MSG,
          ZISAUX_MAJOR_VERSION,
          ZISAUX_MINOR_VERSION,
          ZISAUX_REVISION,
          ZISAUX_VERSION_DATE_STAMP);
}

static void printStopMessage(void) {
  zowelog(NULL, LOG_COMP_STCBASE, ZOWE_LOG_INFO, ZISAUX_LOG_TERM_OK_MSG);
}

#ifdef _LP64
#pragma linkage(BPX4QDB,OS)
#define BPXQDB BPX4QDB
#else
#pragma linkage(BPX1QDB,OS)
#define BPXQDB BPX1QDB
#endif

static bool isDubStatusOk(int *status, int *bpxRC, int *bpxRSN) {

  const int dubFailRC = 4; /* QDB_DUB_MAY_FAIL */

  BPXQDB(status, bpxRC, bpxRSN);

  zowelog(NULL, LOG_COMP_STCBASE, ZOWE_LOG_DEBUG,
          CMS_LOG_DEBUG_MSG_ID" BPXnQDB RV = %d, RC = %d, RSN = 0x%08X\n",
          *status, *bpxRC, *bpxRSN);

  if (*status == dubFailRC) {
    return false;
  }

  return true;
}

static int checkEnv(void) {

  int authStatus = testAuth();
  if (authStatus != 0) {
    zowelog(NULL, LOG_COMP_STCBASE, ZOWE_LOG_SEVERE,
            ZISAUX_LOG_NOT_APF_MSG, authStatus);
    return RC_ZISAUX_ERROR;
  }

  int tcbKey = auxutilGetTCBKey();
  if (tcbKey != CROSS_MEMORY_SERVER_KEY) {
    zowelog(NULL, LOG_COMP_STCBASE, ZOWE_LOG_SEVERE,
            ZISAUX_LOG_BAD_KEY_MSG, tcbKey);
    return RC_ZISAUX_ERROR;
  }

  int dubStatus = 0, bpxRC = 0, bpxRSN = 0;
  if (!isDubStatusOk(&dubStatus, &bpxRC, &bpxRSN)) {
    zowelog(NULL, LOG_COMP_STCBASE, ZOWE_LOG_SEVERE, ZISAUX_LOG_DUB_ERROR_MSG,
            dubStatus, bpxRC, bpxRSN & 0xFFFF);
    return RC_ZISAUX_ERROR;
  }

  return RC_ZISAUX_OK;
}

static void setReadyFlag(ZISAUXContext *context, bool value) {
  if (value) {
    context->masterCommArea->flag |= ZISAUX_HOST_FLAG_READY;
  } else {
    context->masterCommArea->flag &= ~ZISAUX_HOST_FLAG_READY;
  }
}

static void setTermFlag(ZISAUXContext *context, bool value) {
  if (value) {
    context->masterCommArea->flag |= ZISAUX_HOST_FLAG_TERMINATED;
  } else {
    context->masterCommArea->flag &= ~ZISAUX_HOST_FLAG_TERMINATED;
  }
}

static bool isNormalTermination(const ZISAUXContext *context) {

  if (context->flags & ZISAUX_CONTEXT_FLAG_TERM_COMMAND_RECEIVED) {
    return true;
  }

  return false;
}

static void notifyAllPendingCallers(ZISAUXContext *context,
                                    PEReleaseCode releaseCode) {

  PET *allPETs = context->requestPETs;

  for (int i = 0; i < ZISAUX_MAX_REQUEST_COUNT; i++) {
    peRelease(&allPETs[i], releaseCode, false);
  }

}

static int handleCancel(void * __ptr32 managerParmList,
                        void * __ptr32 userData,
                        int * __ptr32 reasonCode) {

  ZISAUXContext *context = userData;
  setReadyFlag(context, false);

  if (context->flags & ZISAUX_CONTEXT_FLAG_GUEST_INITIALIZED) {

    ZISAUXModuleDescriptor *guestDescriptor = context->userModuleDescriptor;
    if (guestDescriptor->handleCancel) {
      guestDescriptor->handleCancel(context, guestDescriptor->moduleData, 0);
    }

  }

  notifyAllPendingCallers(context, (PEReleaseCode){ZISAUX_PE_RC_CANCEL});
  setTermFlag(context, true);

  *reasonCode = 0;
  return 0;
}

static void destroyContext(ZISAUXContext *context);

static ZISAUXContext *makeContext(STCBase *base) {

  ZISAUXContext *context =
      (ZISAUXContext *)safeMalloc(sizeof(ZISAUXContext), "ZISAUXContext");
  if (context == NULL) {
    zowelog(NULL, LOG_COMP_STCBASE, ZOWE_LOG_SEVERE,
            ZISAUX_LOG_RES_ALLOC_FAILED_MSG, "context");
    return NULL;
  }

  memset(context, 0, sizeof(ZISAUXContext));
  memcpy(context->eyecatcher, ZISAUX_CONTEXT_EYE, sizeof(context->eyecatcher));
  context->version = ZISAUX_CONTEXT_VERSION;
  context->key = CROSS_MEMORY_SERVER_KEY; /* we use same key as xmem server */
  context->subpool = SUBPOOL; /* same as the one used in alloc.c */
  context->size = sizeof(ZISAUXContext);

  context->base = base;

  int status = RC_ZISAUX_OK;

  if (status == RC_ZISAUX_OK) {
    context->availablePETs = makeQueue(0);
    if (context->availablePETs == NULL) {
      status = RC_ZISAUX_ERROR;
      zowelog(NULL, LOG_COMP_STCBASE, ZOWE_LOG_SEVERE,
              ZISAUX_LOG_RES_ALLOC_FAILED_MSG, "PET queue");
    }
  }

  if (status == RC_ZISAUX_OK) {
    context->requestPETs =
        (PET *)safeMalloc(sizeof(PET) * ZISAUX_MAX_REQUEST_COUNT, "PET array");
    if (context->requestPETs == NULL) {
      status = RC_ZISAUX_ERROR;
      zowelog(NULL, LOG_COMP_STCBASE, ZOWE_LOG_SEVERE,
              ZISAUX_LOG_RES_ALLOC_FAILED_MSG, "PET array");
    }
  }

  if (status == RC_ZISAUX_OK) {
    for (int i = 0; i < ZISAUX_MAX_REQUEST_COUNT; i++) {
      int allocRC = peAlloc(&context->requestPETs[i], NULL,
                            (PEReleaseCode){ZISAUX_PE_RC_TERM},
                            PE_AUTH_AUTHORIZED, false);
      if (allocRC != 0) {
        status = RC_ZISAUX_ERROR;
        zowelog(NULL, LOG_COMP_STCBASE, ZOWE_LOG_SEVERE,
                ZISAUX_LOG_RES_ALLOC_FAILED_MSG", RC = %d", "PE", allocRC);
        break;
      }
      qInsert(context->availablePETs, &context->requestPETs[i]);
    }
  }

  if (status == RC_ZISAUX_OK) {
    context->parms = zisMakeParmSet();
    if (context->parms == NULL) {
      status = RC_ZISAUX_ERROR;
      zowelog(NULL, LOG_COMP_STCBASE, ZOWE_LOG_SEVERE,
              ZISAUX_LOG_RES_ALLOC_FAILED_MSG, "parmset");
    }
  }

  if (status == RC_ZISAUX_OK) {
    int resmgrRC = 0;
    int resmgrAddRC = resmgrAddTaskResourceManager(handleCancel, context,
                                                   &context->taskResManager,
                                                   &resmgrRC);
    if (resmgrAddRC != RC_RESMGR_OK) {
      status = RC_ZISAUX_ERROR;
      zowelog(NULL, LOG_COMP_STCBASE, ZOWE_LOG_SEVERE,
              ZISAUX_LOG_RESMGR_FAILED_MSG, resmgrAddRC, resmgrRC);
    }
  }

  if (status != RC_ZISAUX_OK) {
    destroyContext(context);
    context = NULL;
  }

  return context;
}

static void destroyContext(ZISAUXContext *context) {

  if (context == NULL) {
    return;
  }

  if ((uint32_t)&context->taskResManager.token != 0) {
    int resmgrRC = 0;
    int resmgrAddRC = resmgrDeleteTaskResourceManager(&context->taskResManager,
                                                      &resmgrRC);
  }

  for (int i = 0; i < ZISAUX_MAX_REQUEST_COUNT; i++) {
    if (context->requestPETs[i].part1 != 0) {
      peDealloc(&context->requestPETs[i], false);
    }
  }

  if (context->requestPETs) {
    safeFree((char *)context->requestPETs,
             sizeof(PET) * ZISAUX_MAX_REQUEST_COUNT);
    context->requestPETs = NULL;
  }

  if (context->availablePETs) {
    destroyQueue(context->availablePETs);
    context->availablePETs = NULL;
  }

  if (context->parms) {
    zisRemoveParmSet(context->parms);
    context->parms = NULL;
  }

  safeFree((char *)context, sizeof(ZISAUXContext));
  context = NULL;

}

#pragma prolog(handleCommunicationPCSS,\
"        PUSH  USING                              \n\
CSSPRLG  LARL  10,CSSPRLG                         \n\
         USING CSSPRLG,10                         \n\
         STORAGE OBTAIN,COND=YES,LENGTH=65536,CALLRKY=YES,SP=132,LOC=31,BNDRY=PAGE \n\
         LTR   15,15                              \n\
         BZ    CSSINIT                            \n\
         LA    15,52                              \n\
         EREGG 0,1                                \n\
         PR                                       \n\
CSSINIT  DS    0H                                 \n\
         LGR   13,1                               \n\
         USING PCHSTACK,13                        \n\
         EREGG 1,1                                \n\
         MVC   PCHSAEYE,=C'F1SA'                  \n\
         MVC   PCHPLEYE,=C'AUXPCPRM'              \n\
         STG   4,PCHPLLPL                         \n\
         STG   1,PCHPLAPL                         \n\
         LA    1,PCHPARM                          \n\
         LA    13,PCHSA                           \n\
         J     CSSRET                             \n\
         LTORG                                    \n\
         DROP  10                                 \n\
CSSRET   DS    0H                                 \n\
         DROP                                     \n\
         POP   USING                               ")

#pragma epilog(handleCommunicationPCSS,\
"        PUSH  USING                              \n\
         SLGFI 13,PCHPARML                        \n\
         LGR   1,13                               \n\
         LGR   2,15                               \n\
         LARL  10,&CCN_BEGIN                      \n\
         USING &CCN_BEGIN,10                      \n\
         STORAGE RELEASE,COND=YES,LENGTH=65536,CALLRKY=YES,SP=132,ADDR=(1) \n\
         LTR   15,15                              \n\
         BZ    CSSTERM                            \n\
         LTR   2,2                                \n\
         BNZ   CSSTERM                            \n\
         LA    2,53                               \n\
CSSTERM  DS    0H                                 \n\
         DROP  10                                 \n\
         LGR   15,2                               \n\
         EREGG 0,1                                \n\
         PR                                       \n\
         DROP                                     \n\
         POP   USING                               ")

#define GET_R1()\
({ \
  void *data = NULL;\
  __asm(\
      ASM_PREFIX\
      "         STG   1,%0                                                     \n"\
      : "=m"(data)\
  );\
  data;\
})

/* The environment similar to crossmemory.c */

ZOWE_PRAGMA_PACK

typedef struct AUXPCParmList_tag {
  char eyecatcher[8];
#define AUX_PC_PARM_EYECATCHER  "AUXPCPRM"
  PCLatentParmList *latentParmList;
  ZISAUXCommServiceParmList *serviceParmList;
} AUXPCParmList;

typedef struct PCRoutineEnvironment_tag {
  char eyecatcher[8];
#define PC_ROUTINE_ENV_EYECATCHER  "RSPCEEYE"
  CAA dummyCAA;
  RLETask dummyRLETask;
} PCRoutineEnvironment;

ZOWE_PRAGMA_PACK_RESET

#define INIT_PC_ENVIRONMENT(envAddr) \
  ({ \
    memset((envAddr), 0, sizeof(PCRoutineEnvironment)); \
    memcpy((envAddr)->eyecatcher, PC_ROUTINE_ENV_EYECATCHER, sizeof((envAddr)->eyecatcher)); \
    (envAddr)->dummyCAA.rleTask = &(envAddr)->dummyRLETask; \
    int returnCode = RC_CMS_OK; \
    __asm(" LA    12,0(,%0) " : : "r"(&(envAddr)->dummyCAA) : ); \
    int recoveryRC = recoveryEstablishRouter(RCVR_ROUTER_FLAG_PC_CAPABLE | \
                                             RCVR_ROUTER_FLAG_RUN_ON_TERM); \
    if (recoveryRC != RC_RCV_OK) { \
      returnCode = RC_CMS_ERROR; \
    } \
    returnCode; \
  })

#define TERMINATE_PC_ENVIRONMENT(envAddr) \
  ({ \
    int returnCode = RC_CMS_OK; \
    int recoveryRC = recoveryRemoveRouter(); \
    if (recoveryRC != RC_RCV_OK) { \
      returnCode = RC_CMS_ERROR; \
    } \
    __asm(" LA    12,0 "); \
    memset((envAddr), 0, sizeof(PCRoutineEnvironment)); \
    returnCode; \
  })

static bool isAUXPCParmListValid(const AUXPCParmList *parmList) {

  if (memcmp(parmList->eyecatcher, AUX_PC_PARM_EYECATCHER,
             sizeof(parmList->eyecatcher))) {
    return false;
  }

  if (parmList->latentParmList == NULL) {
    return false;
  }

  if (parmList->serviceParmList == NULL) {
    return false;
  }

  return true;
}

static bool isAUXContextValid(const ZISAUXContext *context) {

  if (memcmp(context->eyecatcher, ZISAUX_CONTEXT_EYE,
             sizeof(context->eyecatcher))) {
    return false;
  }

  return true;
}

static bool isCommServiceParmListValid(const ZISAUXCommServiceParmList *parmList) {

  if (memcmp(parmList->eyecatcher, ZISAUX_COMM_SERVICE_PARM_EYE,
             sizeof(parmList->eyecatcher))) {
    return false;
  }

  if (!(parmList->function == ZISAUX_COMM_SERVICE_FUNC_COMMAND ||
        parmList->function == ZISAUX_COMM_SERVICE_FUNC_WORK ||
        parmList->function == ZISAUX_COMM_SERVICE_FUNC_TERM)) {
    return false;
  }

  return true;
}

static int copyServiceParmList(ZISAUXCommServiceParmList *parmListCopy,
                               const ZISAUXCommServiceParmList *originalParmList) {

  cmCopyFromSecondaryWithCallerKey(parmListCopy, originalParmList,
                                   sizeof(ZISAUXCommServiceParmList));

  MemObjToken mobjToken = shrmem64GetAddressSpaceToken();
  int shrRC = RC_SHRMEM64_OK, shrRSN = 0;

  if (parmListCopy->payload.data != NULL) {
    shrRC = shrmem64GetAccess(mobjToken, parmListCopy->payload.data, &shrRSN);
    if (shrRC != RC_SHRMEM64_OK) {
      return RC_ZISAUX_SHR64_ERROR;
    }
  }

  if (parmListCopy->response.data != NULL) {
    shrRC = shrmem64GetAccess(mobjToken, parmListCopy->response.data, &shrRSN);
    if (shrRC != RC_SHRMEM64_OK) {
      return RC_ZISAUX_SHR64_ERROR;
    }
  }

  return RC_ZISAUX_OK;
}

static void updateServiceParmList(ZISAUXCommServiceParmList *originalParmList,
                                  const ZISAUXCommServiceParmList *parmListCopy) {

  cmCopyToSecondaryWithCallerKey(originalParmList, parmListCopy,
                                 sizeof(ZISAUXCommServiceParmList));
}

static int cleanServiceParmListCopy(ZISAUXCommServiceParmList *parmListCopy) {

  int status = RC_ZISAUX_OK;
  MemObjToken mobjToken = shrmem64GetAddressSpaceToken();
  int shrRC = RC_SHRMEM64_OK, shrRSN = 0;

  if (parmListCopy->payload.data != NULL) {
    shrRC = shrmem64RemoveAccess(mobjToken, parmListCopy->payload.data, &shrRSN);
    if (shrRC != RC_SHRMEM64_OK) {
      status = status != RC_ZISAUX_OK ? status : RC_ZISAUX_SHR64_ERROR;
    }
  }

  if (parmListCopy->response.data != NULL) {
    shrRC = shrmem64RemoveAccess(mobjToken, parmListCopy->response.data, &shrRSN);
    if (shrRC != RC_SHRMEM64_OK) {
      status = status != RC_ZISAUX_OK ? status : RC_ZISAUX_SHR64_ERROR;
    }
  }

  memset(parmListCopy, 0, sizeof(ZISAUXCommServiceParmList));

  return status;
}

ZOWE_PRAGMA_PACK

typedef struct AUXWorkElement_tag {
  ZISAUXContext *context;
  ZISAUXCommServiceParmList *serviceParmList;
  PET completionPET;
} AUXWorkElement;

ZOWE_PRAGMA_PACK_RESET

static WorkElementPrefix *
makeAUXWorkElement(ZISAUXContext *context,
                   ZISAUXCommServiceParmList *serviceParmList,
                   PET completionPET) {

  int allocSize = sizeof(WorkElementPrefix) + sizeof(AUXWorkElement);
  WorkElementPrefix *element =
      (WorkElementPrefix *)safeMalloc(allocSize, "AUX work element");
  if (element == NULL) {
    return NULL;
  }

  memset(element, 0, allocSize);
  memcpy(element->eyecatcher, "WRKELMNT", sizeof(element->eyecatcher));
  element->payloadLength = sizeof(AUXWorkElement);
  AUXWorkElement *auxWorkElement =
      (AUXWorkElement *)((char *)element + sizeof(WorkElementPrefix));
  auxWorkElement->context = context;
  auxWorkElement->serviceParmList = serviceParmList;
  auxWorkElement->completionPET = completionPET;

  return element;
}

static void removeAUXWorkElement(WorkElementPrefix *element) {
  int allocSize = sizeof(WorkElementPrefix) + sizeof(AUXWorkElement);
  safeFree((char *)element, allocSize);
  element = NULL;
}

static bool isPETValid(const PET *pet) {

  PEState peState;
  PEReleaseCode releaseCode = {0};
  int rc = peTest(pet, &peState, &releaseCode);
  if (rc != 0) {
    return false;
  }

  if (peState == PE_STATUS_PAUSED) {
    return false;
  }

  return true;
}

static void abend(int completionCode, int reasonCode) {
  __asm(
      ASM_PREFIX
      "         ABEND (%[cc]),REASON=(%[rsn])                                  \n"
      :
      : [cc]"NR:r2"(completionCode), [rsn]"NR:r3"(reasonCode)
  );
}

static int scheduleCommRequestProcessing(ZISAUXContext *context,
                                         ZISAUXCommServiceParmList *serviceParmList) {

  PET *requestPET = qRemove(context->availablePETs);
  if (requestPET == NULL) {
    return RC_ZISAUX_REQUEST_LIMIT_REACHED;
  }

  if (!isPETValid(requestPET)) {
    return RC_ZISAUX_BAD_PET;
  }

  ZISAUXCommServiceParmList serviceParmListCopy = {0};
  int copyRC = copyServiceParmList(&serviceParmListCopy, serviceParmList);
  if (copyRC != RC_ZISAUX_OK) {
    return copyRC;
  }

  int status = RC_ZISAUX_OK;
  do {

    WorkElementPrefix *workElement = makeAUXWorkElement(context,
                                                        &serviceParmListCopy,
                                                        *requestPET);
    if (workElement == NULL) {
      status = RC_ZISAUX_ALLOC_FAILED;
      break;
    }

    stcEnqueueWork(context->base, workElement);

    PET updatedPET = {0};
    PEReleaseCode releaseCode = {ZISAUX_PE_OK};
    int pauseRC = pePause(requestPET, &updatedPET, &releaseCode, true);
    if (pauseRC != 0) {
      abend(ZISAUX_ABEND_CC_PAUSE_FAILED, pauseRC);
    }

    *requestPET = updatedPET;
    qInsert(context->availablePETs, requestPET);

    if (releaseCode.value == ZISAUX_PE_OK) {
      status = serviceParmListCopy.auxRC;
    } else if (releaseCode.value == ZISAUX_PE_RC_TERM) {
      status = RC_ZISAUX_AUX_TERMINATION;
    } else if (releaseCode.value == ZISAUX_PE_RC_WORKER_ABEND) {
      status = RC_ZISAUX_REQUEST_TCB_ABEND;
    } else if (releaseCode.value == ZISAUX_PE_RC_SCHEDULE_ERROR) {
      status = RC_ZISAUX_HANDLER_NOT_SCHEDULED;
    } else if (releaseCode.value == ZISAUX_PE_RC_HOST_ABEND) {
      status = RC_ZISAUX_HOST_ABENDED;
    }

  } while (0);

  updateServiceParmList(serviceParmList, &serviceParmListCopy);

  int cleanRC = cleanServiceParmListCopy(&serviceParmListCopy);
  if (cleanRC != RC_ZISAUX_OK) {
    status = status != RC_ZISAUX_OK ? status : cleanRC;
  }

  return status;
}

static unsigned short getSASN(void) {

  unsigned short sasn = 0;
  __asm(
      ASM_PREFIX
      "         LA    %0,0                                                     \n"
      "         ESAR  %0                                                       \n"
      : "=r"(sasn)
      :
      :
  );

  return sasn;
}

static bool isCallerParentServer(ZISAUXContext *context) {

  /*
   * Check 2 cases:
   *  - ZIS    -> AUX
   *  - Client -> ZIS -> AUX
   *  In both cases, SASN should be the ASID of the parent address space, i.e.
   *  ZIS.
   */
  if (context->masterCommArea->parentASID == getSASN()) {
    return true;
  }

  return false;
}

static int handleUnsafePCSS(AUXPCParmList *parmList) {

  if (!isAUXPCParmListValid(parmList)) {
    return RC_ZISAUX_PC_PARM_INVALID;
  }

  ZISAUXContext *context = parmList->latentParmList->parm1;
  if (!isAUXContextValid(context)) {
    return RC_ZISAUX_COMM_AREA_INVALID;
  }

  if (!isCallerParentServer(context)) {
    return RC_ZISAUX_CALLER_NOT_RECOGNIZED;
  }

  ZISAUXCommServiceParmList *serviceParmList = parmList->serviceParmList;
  ZISAUXCommServiceParmList localServiceParmList;
  cmCopyFromSecondaryWithCallerKey(&localServiceParmList, serviceParmList,
                                   sizeof(ZISAUXCommServiceParmList));

  if (!isCommServiceParmListValid(&localServiceParmList)) {
    return RC_ZISAUX_COMM_SRVC_PARM_INVALID;
  }

  if (localServiceParmList.version > ZISAUX_COMM_SERVICE_PARM_VERSION) {
    return RC_ZISAUX_UNSUPPORTED_VERSION;
  }

  int scheduleRC = scheduleCommRequestProcessing(context, serviceParmList);

  return scheduleRC;
}

static int handleCommunicationPCSS(void) {

  AUXPCParmList *parmList = GET_R1();

  int key = auxutilGetCallersKey();
  if (key != ZISAUX_CALLER_KEY) {
    return RC_ZISAUX_BAD_CALLER_KEY;
  }

  PCRoutineEnvironment env;
  int envRC = INIT_PC_ENVIRONMENT(&env);
  if (envRC != RC_CMS_OK) {
    return RC_ZISAUX_PC_ENV_NOT_ESTABLISHED;
  }

  int returnCode = RC_CMS_OK;

  int pushRC = recoveryPush("AUX PC handler",
                            RCVR_FLAG_RETRY | RCVR_FLAG_DELETE_ON_RETRY | RCVR_FLAG_PRODUCE_DUMP,
                            "ZAUX", NULL, NULL, NULL, NULL);

  if (pushRC == RC_RCV_OK) {

    returnCode = handleUnsafePCSS(parmList);

    recoveryPop();

  } else if (pushRC == RC_RCV_ABENDED) {
    returnCode = RC_ZISAUX_PC_ABEND_DETECTED;
  } else {
    returnCode = RC_ZISAUX_PC_RECOVERY_ENV_FAILED;
  }

  envRC = TERMINATE_PC_ENVIRONMENT(&env);
  if (envRC != RC_CMS_OK) {
    if (returnCode != RC_ZISAUX_OK) {
      returnCode = RC_ZISAUX_PC_ENV_NOT_TERMINATED;
    }
  }

  return returnCode;
}

static int establishCommunicationPC(ZISAUXContext *context) {

  EntryTableDescriptor *etd = pcMakeEntryTableDescriptor();
  if (etd == NULL) {
    zowelog(NULL, LOG_COMP_STCBASE, ZOWE_LOG_SEVERE,
            ZISAUX_LOG_RES_ALLOC_FAILED_MSG, "PC entry table descriptor");
    return RC_ZISAUX_ERROR;
  }

  pcAddToEntryTableDescriptor(etd, handleCommunicationPCSS, (uint32_t)context, 0,
                              true, true, true, true, CROSS_MEMORY_SERVER_KEY);

  int rc = RC_PC_OK, rsn = 0;

  if (rc == RC_PC_OK) {
    rc = pcSetAllAddressSpaceAuthority(&rsn);
  }

  PCLinkageIndex linkageIndex = {0};
  if (rc == RC_PC_OK) {
    rc = pcReserveLinkageIndex(true, true, LX_SIZE_16, &linkageIndex, &rsn);
  }

  PCEntryTableToken entryTableToken = 0;
  if (rc == RC_PC_OK) {
    rc = pcCreateEntryTable(etd, &entryTableToken, &rsn);
  }

  if (rc == RC_PC_OK) {
    rc = pcConnectEntryTable(entryTableToken, linkageIndex, &rsn);
  }

  if (rc == RC_PC_OK) {
    context->masterCommArea->pcNumber = linkageIndex.pcNumber;
    context->masterCommArea->sequenceNumber = linkageIndex.sequenceNumber;
  } else {
    zowelog(NULL, LOG_COMP_STCBASE, ZOWE_LOG_SEVERE,
            ZISAUX_LOG_PCSETUP_FAILED_MSG, rc, rsn);
    return RC_ZISAUX_ERROR;
  }

  return RC_ZISAUX_OK;
}

static ZISAUXCommArea *getMasterCommAreaAddress(ZISParmSet *parms) {

  const char *addressString = zisGetParmValue(parms, ZISAUX_HOST_PARM_COMM_KEY);
  if (addressString == NULL) {
    zowelog(NULL, LOG_COMP_STCBASE, ZOWE_LOG_SEVERE,
            ZISAUX_LOG_COMM_FAILED_MSG" master comm area parm not found");
    return NULL;
  }

  ZISAUXCommArea *commArea = NULL;
  int sscanfRC = sscanf(addressString, "%p", &commArea);
  if (sscanfRC != 1) {
    zowelog(NULL, LOG_COMP_STCBASE, ZOWE_LOG_SEVERE,
            ZISAUX_LOG_COMM_FAILED_MSG" master comm area address string \'%s\' "
            "not parsed, sscanf RC = %d", addressString, sscanfRC);
    return NULL;
  }

  if (memcmp(commArea->eyecatcher, ZISAUX_COMM_EYECATCHER,
             sizeof(commArea->eyecatcher))) {
    zowelog(NULL, LOG_COMP_STCBASE, ZOWE_LOG_SEVERE,
            ZISAUX_LOG_COMM_FAILED_MSG" master comm area %p has bad eyecatcher",
            commArea);
    return NULL;
  }

  return commArea;
}

static int loadMasterParm(ZISAUXContext *context) {

  ASParm *parm = NULL;

  int wasProblemState = supervisorMode(TRUE);

  int extractRC = 0, extractRSN = 0;
  extractRC = addressSpaceExtractParm(&parm, &extractRSN);

  if (wasProblemState) {
    supervisorMode(FALSE);
  }

  if (extractRC == 0) {
    zowelog(NULL, LOG_COMP_STCBASE, ZOWE_LOG_DEBUG,
            ZIS_LOG_DEBUG_MSG_ID" Address space extract RC = %d, RSN = %d, "
            "parm = 0x%p", extractRC, extractRSN, parm);
    if (parm) {
      zowedump(NULL, LOG_COMP_STCBASE, ZOWE_LOG_DEBUG, parm->data, parm->length);
    }
  } else {
    zowelog(NULL, LOG_COMP_STCBASE, ZOWE_LOG_SEVERE,
            ZISAUX_LOG_ASXTR_FAILED_MSG, extractRC, extractRSN);
    return RC_ZISAUX_ERROR;
  }

  /* The address space parm has the same format as the main function parms,
   * we can use the corresponding ZIS parm function to parse it. */

  int readRC = zisReadMainParms(context->parms, (ZISMainFunctionParms *)parm);
  if (readRC != RC_ZISPARM_OK) {
    zowelog(NULL, LOG_COMP_STCBASE, ZOWE_LOG_SEVERE,
            ZISAUX_LOG_CONFIG_FAILURE_MSG, "master parms not read", readRC);
    return RC_ZISAUX_ERROR;
  }

  ZISAUXCommArea *commArea = getMasterCommAreaAddress(context->parms);
  if (commArea == NULL) {
    return RC_ZISAUX_ERROR;
  }
  context->masterCommArea = commArea;

  return RC_ZISAUX_OK;
}

static int loadConfig(ZISAUXContext *context,
                      const ZISMainFunctionParms *mainParms) {

  if (mainParms != NULL && mainParms->textLength > 0) {
    zowelog(NULL, LOG_COMP_STCBASE, ZOWE_LOG_INFO,
            ZISAUX_LOG_INPUT_PARM_MSG, mainParms);
    zowedump(NULL, LOG_COMP_STCBASE, ZOWE_LOG_INFO,
             (void *)mainParms, sizeof(mainParms) + mainParms->textLength);
  }

  int readMainParmsRC = zisReadMainParms(context->parms, mainParms);
  if (readMainParmsRC != RC_ZISPARM_OK) {
    zowelog(NULL, LOG_COMP_STCBASE, ZOWE_LOG_SEVERE,
            ZISAUX_LOG_CONFIG_FAILURE_MSG, "main parms not read",
            readMainParmsRC);
    return RC_ZISAUX_ERROR;
  }

  int masterParmRC = loadMasterParm(context);
  if (masterParmRC != RC_ZISAUX_OK) {
    return masterParmRC;
  }

  const char *parmlibMember = zisGetParmValue(context->parms, "PARM");
  if (parmlibMember) {

    ZISParmStatus readStatus = {0};
    int readParmRC = zisReadParmlib(context->parms, "PARMLIB",
                                    parmlibMember, &readStatus);
    if (readParmRC != RC_ZISPARM_OK) {
      if (readParmRC == RC_ZISPARM_MEMBER_NOT_FOUND) {
        zowelog(NULL, LOG_COMP_STCBASE, ZOWE_LOG_SEVERE,
                ZISAUX_LOG_CFG_MISSING_MSG, parmlibMember, readParmRC);
      } else {
        zowelog(NULL, LOG_COMP_STCBASE, ZOWE_LOG_SEVERE,
                ZISAUX_LOG_CFG_NOT_READ_MSG, parmlibMember, readParmRC,
                readStatus.internalRC, readStatus.internalRSN);
      }
    }

  }

 return RC_ZISAUX_OK;
}

typedef struct ABENDInfo_tag {
  char eyecatcher[8];
#define ABEND_INFO_EYECATCHER "AUXABEDI"
  int completionCode;
  int reasonCode;
} ABENDInfo;

static void extractABENDInfo(RecoveryContext * __ptr32 context,
                             SDWA * __ptr32 sdwa,
                             void * __ptr32 userData) {
  ABENDInfo *info = (ABENDInfo *)userData;
  recoveryGetABENDCode(sdwa, &info->completionCode, &info->reasonCode);
}

static int getUserModuleName(const ZISParmSet *parms,
                             EightCharString *name) {

  const char *moduleParmValue = zisGetParmValue(parms, ZISAUX_HOST_PARM_MOD_KEY);
  if (moduleParmValue == NULL) {
    zowelog(NULL, LOG_COMP_STCBASE, ZOWE_LOG_SEVERE,
            ZISAUX_LOG_USERMOD_FAIULRE_MSG" module name not provided");
    return RC_ZISAUX_ERROR;
  }

  size_t moduleParmValueSize = strlen(moduleParmValue);
  if (moduleParmValueSize > sizeof(name->text)) {
    zowelog(NULL, LOG_COMP_STCBASE, ZOWE_LOG_SEVERE,
            ZISAUX_LOG_USERMOD_FAIULRE_MSG" module name \'%s\' too long",
            moduleParmValue);
    return RC_ZISAUX_ERROR;
  }

  memcpy(name->text, moduleParmValue, moduleParmValueSize);

  return RC_ZISAUX_OK;
}

static int loadUserModule(ZISAUXContext *context) {

  EightCharString moduleName;
  int getNameRC = getUserModuleName(context->parms, &moduleName);
  if (getNameRC != RC_ZISAUX_OK) {
    return RC_ZISAUX_ERROR;
  }

  void *ep = NULL;

  int recoveryRC = RC_RCV_OK;
  ABENDInfo abendInfo = {ABEND_INFO_EYECATCHER};

  /* Try to load the user module. */
  recoveryRC = recoveryPush("loadUserModule():load",
                            RCVR_FLAG_RETRY | RCVR_FLAG_DELETE_ON_RETRY,
                            "ZIS AUX load module", extractABENDInfo, &abendInfo,
                            NULL, NULL);

  {
    if (recoveryRC == RC_RCV_OK) {

      int loadStatus = 0;
      ep = loadByNameLocally(moduleName.text, &loadStatus);
      if (ep == NULL || loadStatus != 0) {
        zowelog(NULL, LOG_COMP_STCBASE, ZOWE_LOG_SEVERE,
                ZISAUX_LOG_USERMOD_FAIULRE_MSG" module \'%8.8s\' not loaded, "
                "EP = %p, status = %d", moduleName.text, ep, loadStatus);
      }

    } else {
      zowelog(NULL, LOG_COMP_STCBASE, ZOWE_LOG_SEVERE,
              ZISAUX_LOG_USERMOD_FAIULRE_MSG" module \'%8.8s\' not loaded, "
              "ABEND S%03X-%02X (recovery RC = %d)",
              moduleName.text, abendInfo.completionCode,
              abendInfo.reasonCode, recoveryRC);
    }
  }

  if (recoveryRC == RC_RCV_OK) {
    recoveryPop();
  }

  if (ep == NULL) {
    return RC_ZISAUX_ERROR;
  }

  int status = RC_ZISAUX_OK;

  ZISAUXGetModuleDescriptor *getDescriptor =
      (ZISAUXGetModuleDescriptor *)((int)ep & 0xFFFFFFFE);
  ZISAUXModuleDescriptor *descriptor = NULL;

  /* Try executing module EP. */
  recoveryRC = recoveryPush("loadUserModule():exec",
                            RCVR_FLAG_RETRY | RCVR_FLAG_DELETE_ON_RETRY,
                            "ZIS AUX load module", extractABENDInfo, &abendInfo,
                            NULL, NULL);
  {
    if (recoveryRC == RC_RCV_OK) {

      descriptor = getDescriptor();
      if (descriptor == NULL) {
        zowelog(NULL, LOG_COMP_STCBASE, ZOWE_LOG_SEVERE,
                ZISAUX_LOG_USERMOD_FAIULRE_MSG" module descriptor not "
                "received from module \'%8.8s\'", moduleName);
        status = RC_ZISAUX_ERROR;
      }

    } else {
      zowelog(NULL, LOG_COMP_STCBASE, ZOWE_LOG_SEVERE,
              ZISAUX_LOG_USERMOD_FAIULRE_MSG" module EP not executed, "
              "module \'%8.8s\', recovery RC = %d, ABEND %03X-%02X",
              moduleName, recoveryRC, abendInfo.completionCode,
              abendInfo.reasonCode);
      status = RC_ZISAUX_ERROR;
    }
  }

  if (recoveryRC == RC_RCV_OK) {
    recoveryPop();
  }

  if (status == RC_ZISAUX_OK) {
    context->userModuleDescriptor = descriptor;
  }

  return status;
}


#define  CALL_UNSAFE_FUNCTION(nameStr, funcPtr, funcRCPtr, ...) ({ \
\
  int status = RC_ZISAUX_OK; \
  int recoveryRC = RC_RCV_OK; \
  ABENDInfo abendInfo = {ABEND_INFO_EYECATCHER}; \
\
  recoveryRC = recoveryPush(nameStr, \
                            RCVR_FLAG_RETRY | RCVR_FLAG_DELETE_ON_RETRY, \
                            "ZIS AUX unsafe function", \
                            extractABENDInfo, &abendInfo, \
                            NULL, NULL); \
\
  { \
    if (recoveryRC == RC_RCV_OK) { \
      *funcRCPtr = funcPtr(__VA_ARGS__); \
    } else { \
      zowelog(NULL, LOG_COMP_STCBASE, ZOWE_LOG_WARNING, \
              ZISAUX_LOG_UNSAFE_CALL_FAIULRE_MSG, \
              nameStr, abendInfo.completionCode, \
              abendInfo.reasonCode, recoveryRC); \
      status = RC_ZISAUX_ERROR; \
    } \
  } \
\
  if (recoveryRC == RC_RCV_OK) { \
    recoveryPop(); \
  } \
\
  status; \
})


static int callUserModuleInit(ZISAUXContext *context, int traceLevel) {

  ZISAUXInitFunction *init = context->userModuleDescriptor->init;
  void *moduleData = context->userModuleDescriptor->moduleData;

  if (init) {

    int initRC = RC_ZISAUX_OK;
    int callUnsafeCodeRC = CALL_UNSAFE_FUNCTION("module init function",
                                                init, &initRC,
                                                context, moduleData,
                                                traceLevel);
    if (callUnsafeCodeRC == RC_ZISAUX_OK) {

      if (initRC != RC_ZISAUX_GUEST_OK) {
        zowelog(NULL, LOG_COMP_STCBASE, ZOWE_LOG_SEVERE,
                ZISAUX_LOG_USERMOD_FAIULRE_MSG" init function RC = %d",
                initRC);
        return RC_ZISAUX_ERROR;
      }

    } else {
      return callUnsafeCodeRC;
    }

  }

  context->flags |= ZISAUX_CONTEXT_FLAG_GUEST_INITIALIZED;

  return RC_ZISAUX_OK;
}

static int callUserModuleTerm(ZISAUXContext *context, int traceLevel) {

  ZISAUXTermFunction *term = context->userModuleDescriptor->term;
  void *moduleData = context->userModuleDescriptor->moduleData;

  if (term) {

    int termRC = RC_ZISAUX_OK;
    int callUnsafeCodeRC = CALL_UNSAFE_FUNCTION("module term function",
                                                term, &termRC,
                                                context, moduleData,
                                                traceLevel);
    if (callUnsafeCodeRC == RC_ZISAUX_GUEST_OK) {

      if (termRC != RC_ZISAUX_OK) {
        zowelog(NULL, LOG_COMP_STCBASE, ZOWE_LOG_SEVERE,
                ZISAUX_LOG_USERMOD_FAIULRE_MSG" term function RC = %d",
                termRC);
        return RC_ZISAUX_ERROR;
      }

    } else {
      return callUnsafeCodeRC;
    }

  }

  return RC_ZISAUX_OK;
}


static int callUserModuleCommandHandler(ZISAUXContext *context,
                                        const ZISAUXCommand *rawCommand,
                                        const CMSModifyCommand *parsedCommand,
                                        ZISAUXCommandReponse *response,
                                        int *reasonCode, int traceLevel) {

  int rc = RC_ZISAUX_OK;
  int rsn = 0;

  ZISAUXCommandHandlerFunction *handleCommand =
      context->userModuleDescriptor->handleCommand;
  void *moduleData = context->userModuleDescriptor->moduleData;

  if (handleCommand) {

    int handleCommandRC = RC_ZISAUX_OK;
    int callUnsafeCodeRC = CALL_UNSAFE_FUNCTION("module command handler",
                                                handleCommand,
                                                &handleCommandRC,
                                                context,
                                                rawCommand,
                                                parsedCommand,
                                                response,
                                                traceLevel);
    if (callUnsafeCodeRC == RC_ZISAUX_GUEST_OK) {

      if (handleCommandRC != RC_ZISAUX_OK) {
        zowelog(NULL, LOG_COMP_STCBASE, ZOWE_LOG_SEVERE,
                ZISAUX_LOG_USERMOD_FAIULRE_MSG" command handler RC = %d",
                handleCommandRC);
        rc = RC_ZISAUX_USER_HANDLER_ERROR;
        rsn = handleCommandRC;
      }

    } else {
      rc = RC_ZISAUX_HANDLER_ERROR;
      rsn = callUnsafeCodeRC;
    }

  }

  *reasonCode = rsn;
  return rc;
}

static int callUserModuleWorkHandler(ZISAUXContext *context,
                                     const ZISAUXRequestPayload *payload,
                                     ZISAUXRequestResponse *response,
                                     int *reasonCode, int traceLevel) {
  int rc = RC_ZISAUX_OK;
  int rsn = 0;

  ZISAUXWorkHandlerFunction *handleWork =
      context->userModuleDescriptor->handleWork;
  void *moduleData = context->userModuleDescriptor->moduleData;

  if (handleWork) {

    int handleWorkRC = RC_ZISAUX_OK;
    int callUnsafeCodeRC = CALL_UNSAFE_FUNCTION("module work function",
                                                handleWork, &handleWorkRC,
                                                context,
                                                payload,
                                                response,
                                                traceLevel
    );
    if (callUnsafeCodeRC == RC_ZISAUX_GUEST_OK) {

      if (handleWorkRC != RC_ZISAUX_OK) {
        zowelog(NULL, LOG_COMP_STCBASE, ZOWE_LOG_SEVERE,
                ZISAUX_LOG_USERMOD_FAIULRE_MSG" work function RC = %d",
                handleWorkRC);
        rc = RC_ZISAUX_USER_HANDLER_ERROR;
        rsn = handleWorkRC;
      }

    } else {
      rc = RC_ZISAUX_HANDLER_ERROR;
      rsn = callUnsafeCodeRC;
    }

  }

  *reasonCode = rsn;
  return rc;
}

static void reportCommandRetrieval(const char *command,
                                   CMSWTORouteInfo *routeInfo) {

  unsigned int consoleID = routeInfo->consoleID;
  CART cart = routeInfo->cart;

  zowelog(NULL, LOG_COMP_STCBASE, ZOWE_LOG_INFO,
          ZISAUX_LOG_CMD_RECEIVED_MSG, command);
  wtoPrintf2(consoleID, cart,
             ZISAUX_LOG_CMD_RECEIVED_MSG, command);

}

static void reportCommandStatus(const char *command,
                                CMSWTORouteInfo *routeInfo,
                                CMSModifyCommandStatus status) {

  unsigned int consoleID = routeInfo->consoleID;
  CART cart = routeInfo->cart;

  if (status == CMS_MODIFY_COMMAND_STATUS_UNKNOWN) {
    zowelog(NULL, LOG_COMP_STCBASE, ZOWE_LOG_WARNING,
            ZISAUX_LOG_CMD_NOT_RECOGNIZED_MSG, command);
    wtoPrintf2(consoleID, cart, ZISAUX_LOG_CMD_NOT_RECOGNIZED_MSG, command);
  } else if (status != CMS_MODIFY_COMMAND_STATUS_CONSUMED &&
             status != CMS_MODIFY_COMMAND_STATUS_PROCESSED) {
    zowelog(NULL, LOG_COMP_STCBASE, ZOWE_LOG_WARNING,
            ZISAUX_LOG_CMD_REJECTED_MSG, command);
    wtoPrintf2(consoleID, cart,
               ZISAUX_LOG_CMD_REJECTED_MSG, command);
  }

}

static int handleCommandVerbLog(
    ZISAUXContext *context,
    const CMSModifyCommand *parsedCommand,
    const ZISAUXCommand *rawCommand,
    ZISAUXCommandReponse *response,
    int *reasonCode,
    int traceLevel
) {

  CART cart = parsedCommand->routeInfo.cart;
  int consoleID = parsedCommand->routeInfo.consoleID;

  unsigned int expectedArgCount = 2;
  if (parsedCommand->argCount != expectedArgCount) {
    zowelog(NULL, LOG_COMP_ID_CMS, ZOWE_LOG_WARNING,
            ZISAUX_LOG_INVALID_CMD_ARGS_MSG, "LOG", expectedArgCount,
            parsedCommand->argCount);
    wtoPrintf2(consoleID, cart, ZISAUX_LOG_INVALID_CMD_ARGS_MSG, "LOG",
               expectedArgCount, parsedCommand->argCount);
    response->status = CMS_MODIFY_COMMAND_STATUS_REJECTED;
    return RC_ZISAUX_OK;
  }

  const char *component = parsedCommand->args[0];
  uint64 logCompID = 0;
  if (strcmp(component, "STC") == 0) {
    logCompID = LOG_COMP_STCBASE;
  } else {
    zowelog(NULL, LOG_COMP_ID_CMS, ZOWE_LOG_WARNING, ZISAUX_LOG_BAD_LOG_COMP_MSG,
            component);
    wtoPrintf2(consoleID, cart, ZISAUX_LOG_BAD_LOG_COMP_MSG, component);
    response->status = CMS_MODIFY_COMMAND_STATUS_REJECTED;
    return RC_ZISAUX_OK;
  }

  const char *level = parsedCommand->args[1];
  int logLevel = ZOWE_LOG_NA;
  if (strcmp(level, "SEVERE") == 0) {
    logLevel = ZOWE_LOG_SEVERE;
  } else if (strcmp(level, "WARNING") == 0) {
    logLevel = ZOWE_LOG_WARNING;
  } else if (strcmp(level, "INFO") == 0) {
    logLevel = ZOWE_LOG_INFO;
  } else if (strcmp(level, "DEBUG") == 0) {
    logLevel = ZOWE_LOG_DEBUG;
  } else if (strcmp(level, "DEBUG2") == 0) {
    logLevel = ZOWE_LOG_DEBUG2;
  } else if (strcmp(level, "DEBUG3") == 0) {
    logLevel = ZOWE_LOG_DEBUG3;
  } else {
    zowelog(NULL, LOG_COMP_ID_CMS, ZOWE_LOG_WARNING, ZISAUX_LOG_BAD_LOG_LEVEL_MSG,
            level);
    wtoPrintf2(consoleID, cart, ZISAUX_LOG_BAD_LOG_LEVEL_MSG, level);
    response->status = CMS_MODIFY_COMMAND_STATUS_REJECTED;
  }

  logSetLevel(NULL, logCompID, logLevel);
  response->status = CMS_MODIFY_COMMAND_STATUS_CONSUMED;

  return RC_ZISAUX_OK;
}

static int handleUserModuleCommand(
    ZISAUXContext *context,
    const CMSModifyCommand *parsedCommand,
    const ZISAUXCommand *rawCommand,
    ZISAUXCommandReponse *response,
    int *reasonCode,
    int traceLevel
) {

  int commandHandlerRC = 0;
  int userModuleCallRC = RC_ZISAUX_OK;
  int userModuleCallRSN = 0;
  userModuleCallRC = callUserModuleCommandHandler(context,
                                                  rawCommand,
                                                  parsedCommand,
                                                  response,
                                                  &userModuleCallRSN,
                                                  traceLevel);

  if (userModuleCallRC != RC_ZISAUX_OK) {
    *reasonCode = userModuleCallRSN;
     return userModuleCallRC;
  }

  return RC_ZISAUX_OK;
}

static int handleAUXCommand(ZISAUXContext *context,
                            const ZISAUXCommand *rawCommand,
                            ZISAUXCommandReponse *response,
                            int *reasonCode, int traceLevel) {

  response->status = CMS_MODIFY_COMMAND_STATUS_UNKNOWN;

  int slhBlockSize = 4096;
  int slhMaxBlockNumber = 4;
  ShortLivedHeap *slh = makeShortLivedHeap(slhBlockSize, slhMaxBlockNumber);
  if (slh == NULL) {
    zowelog(NULL, LOG_COMP_STCBASE, ZOWE_LOG_SEVERE,
            ZISAUX_LOG_RES_ALLOC_FAILED_MSG, "command SLH block");
    return RC_ZISAUX_ALLOC_FAILED;
  }

  int rc = RC_ZISAUX_OK;
  int rsn = 0;

  char **tokens = NULL;
  unsigned tokenCount = 0;
  int tokenizeRC = auxutilTokenizeString(slh, rawCommand->text, rawCommand->length, ' ',
                                         &tokens, &tokenCount);
  if (tokenizeRC == 0) {

    ZISAUXCommandHandlerFunction *moduleCommandHandler =
        context->userModuleDescriptor->handleCommand;
    if (tokenCount > 0 && moduleCommandHandler) {

      CMSModifyCommand parsedCommand = {
          .routeInfo = rawCommand->routeInfo,
          .commandVerb = tokens[0],
          .target = NULL,
          .args = (const char* const*)&tokens[1], /* C can't infer that it's OK
                                                            to pass "char **" to
                                                            "const char * const *" */
          .argCount = tokenCount - 1,
      };

      if (!strcmp(parsedCommand.commandVerb, "LOG")) {
        rc = handleCommandVerbLog(context, &parsedCommand, rawCommand,
                                  response, reasonCode, traceLevel);
      } else {
        rc = handleUserModuleCommand(context, &parsedCommand, rawCommand,
                                     response, reasonCode, traceLevel);
      }

    }

  } else {
    zowelog(NULL, LOG_COMP_STCBASE, ZOWE_LOG_WARNING,
            ZISAUX_LOG_CMD_TKNZ_FAILURE_MSG);
  }


  SLHFree(slh);
  slh = NULL;

  if (reasonCode != NULL) {
    *reasonCode = rsn;
  }
  return rc;
}




static int handleModifyCommand(STCBase *base,
                               CIB *cib,
                               STCConsoleCommandType commandType,
                               const char *command,
                               unsigned short commandLength,
                               void *userData) {

  CIBX *cibx = (CIBX *)((char *)cib + cib->cibxoff);
  unsigned int consoleID = cibx->cibxcnid;
  CART cart = *(CART *)&cibx->cibxcart;
  CMSWTORouteInfo routeInfo = {cart, consoleID};

  ZISAUXContext *context = userData;

  zowelog(NULL, LOG_COMP_STCBASE, ZOWE_LOG_DEBUG,
          ZIS_LOG_DEBUG_MSG_ID" Command handler called, type=%d, command=0x%p, "
          "commandLength=%hu, userData=0x%p\n",
          commandType, command, commandLength, userData);
  zowedump(NULL, LOG_COMP_STCBASE, ZOWE_LOG_DEBUG,
           (void *)command, commandLength);

  if (commandLength > AUXUTILS_FIELD_SIZE(ZISAUXCommand, text)) {
    zowelog(NULL, LOG_COMP_STCBASE, ZOWE_LOG_WARNING,
            ZISAUX_LOG_CMD_TOO_LONG_MSG, commandLength);
    return 8;
  }

  if (commandType == STC_COMMAND_MODIFY) {

    ZISAUXCommand auxCommand = {.length = commandLength};
    memcpy(auxCommand.text, command, commandLength);
    auxCommand.routeInfo = routeInfo;
    ZISAUXCommandReponse response = {0};

    reportCommandRetrieval(command, &routeInfo);

    handleAUXCommand(context, &auxCommand, &response, NULL,
                     logGetLevel(NULL, LOG_COMP_STCBASE));

    if (response.length > 0) {
      zowelog(NULL, LOG_COMP_STCBASE, ZOWE_LOG_INFO,
              ZISAUX_LOG_USERMOD_CMD_RESP_MSG, response.length, response.text);
    }

    reportCommandStatus(command, &routeInfo, response.status);

  } else if (commandType == STC_COMMAND_START) {
    context->startRouteInfo = routeInfo;

  } else if (commandType == STC_COMMAND_STOP) {
    context->termRouteInfo = routeInfo;

    zowelog(NULL, LOG_COMP_STCBASE, ZOWE_LOG_INFO, ZISAUX_LOG_TERM_CMD_MSG);
    context->flags |= ZISAUX_CONTEXT_FLAG_TERM_COMMAND_RECEIVED;

  }

  return 0;
}

static void handleCommandRequest(ZISAUXContext *context,
                                 ZISAUXCommServiceParmList *parmList) {

  ZISAUXCommand *command = (ZISAUXCommand *)&parmList->payload.inlineData;
  ZISAUXCommandReponse *response =
      (ZISAUXCommandReponse *)&parmList->response.inlineData;

  parmList->auxRC = handleAUXCommand(context,
                                     command, response,
                                     &parmList->auxRSN,
                                     parmList->traceLevel);

}

static void handleWorkRequest(ZISAUXContext *context,
                              ZISAUXCommServiceParmList *parmList) {

  parmList->auxRC = callUserModuleWorkHandler(context,
                                              &parmList->payload,
                                              &parmList->response,
                                              &parmList->auxRSN,
                                              parmList->traceLevel);
}

static void terminateAUX(ZISAUXContext *context) {
  ZISAUXCommArea *commArea = context->masterCommArea;
  auxutilPost(&commArea->commECB, ZISAUX_COMM_SIGNAL_TERM);
}

static void handleTermRequest(ZISAUXContext *context,
                              ZISAUXCommServiceParmList *parmList) {
  terminateAUX(context);
  parmList->auxRC = RC_ZISAUX_OK;

}

static void handleUknownRequest(ZISAUXContext *context,
                                ZISAUXCommServiceParmList *parmList) {

  parmList->auxRC = RC_ZISAUX_BAD_FUNCTION_CODE;
  parmList->auxRSN = parmList->function;

}

static int handleCommRequest(RLETask *task) {

  WorkElementPrefix *workElementPrefix = task->userPointer;
  AUXWorkElement *workElement =
      (AUXWorkElement *)((char *)workElementPrefix + sizeof(WorkElementPrefix));
  ZISAUXCommServiceParmList *parmList = workElement->serviceParmList;
  ZISAUXContext *context = workElement->context;

  zowelog(NULL, LOG_COMP_STCBASE, ZOWE_LOG_DEBUG,
          ZIS_LOG_DEBUG_MSG_ID" Request received, work element @ 0x%p:\n",
          workElement);
  zowedump(NULL, LOG_COMP_STCBASE, ZOWE_LOG_DEBUG, workElement,
           sizeof(AUXWorkElement));

  switch (parmList->function) {
  case ZISAUX_COMM_SERVICE_FUNC_COMMAND:
    handleCommandRequest(context, parmList);
    break;
  case ZISAUX_COMM_SERVICE_FUNC_WORK:
    handleWorkRequest(context, parmList);
    break;
  case ZISAUX_COMM_SERVICE_FUNC_TERM:
    handleTermRequest(context, parmList);
    break;
  default:
    handleUknownRequest(context, parmList);
    break;
  }

  int releaseRC = peRelease(&workElement->completionPET,
                            (PEReleaseCode){ZISAUX_PE_OK},
                            false);
  if (releaseRC != 0) {
    zowelog(NULL, LOG_COMP_STCBASE, ZOWE_LOG_WARNING,
            ZISAUX_LOG_NOT_RELEASED_MSG, releaseRC);
    cleanServiceParmListCopy(parmList);
    parmList = NULL;
    workElement->serviceParmList = NULL;
  }

  zowelog(NULL, LOG_COMP_STCBASE, ZOWE_LOG_DEBUG,
          ZIS_LOG_DEBUG_MSG_ID" Request processed for work element @ 0x%p\n",
          workElement);
  zowedump(NULL, LOG_COMP_STCBASE, ZOWE_LOG_DEBUG, workElement,
           sizeof(AUXWorkElement));

  removeAUXWorkElement(workElementPrefix);
  workElementPrefix = NULL;
  workElement = NULL;

  return 0;
}

static int asyncRequestHanler(RLETask *task) {

  WorkElementPrefix *prefix = task->userPointer;
  AUXWorkElement *workElement =
      (AUXWorkElement *)((char *)prefix + sizeof(WorkElementPrefix));

  int pushRC = recoveryPush("AUX async request handler",
                            RCVR_FLAG_RETRY | RCVR_FLAG_DELETE_ON_RETRY | RCVR_FLAG_PRODUCE_DUMP,
                            "ZAUX", NULL, NULL, NULL, NULL);

  if (pushRC == RC_RCV_OK) {

    handleCommRequest(task);

    recoveryPop();

  } else if (pushRC == RC_RCV_ABENDED) {
    peRelease(&workElement->completionPET,
              (PEReleaseCode){ZISAUX_PE_RC_WORKER_ABEND},
              false);
  } else {
    peRelease(&workElement->completionPET,
              (PEReleaseCode){ZISAUX_PE_RC_ENV_ERROR},
              false);
  }

  return 0;
}

static int workElementHandler(STCBase *base, STCModule *module,
                              WorkElementPrefix *prefix) {

  AUXWorkElement *workElement =
      (AUXWorkElement *)((char *)prefix + sizeof(WorkElementPrefix));

  RLETask *asyncTask = makeRLETask(base->rleAnchor,
                                   RLE_TASK_TCB_CAPABLE | RLE_TASK_RECOVERABLE | RLE_TASK_DISPOSABLE,
                                   asyncRequestHanler);
  if (asyncTask == NULL) {
    int releaseRC = peRelease(&workElement->completionPET,
                              (PEReleaseCode){ZISAUX_PE_RC_SCHEDULE_ERROR},
                              false);
    if (releaseRC != 0) {
      zowelog(NULL, LOG_COMP_STCBASE, ZOWE_LOG_WARNING,
              ZISAUX_LOG_NOT_RELEASED_MSG, releaseRC);
    }
    return 8;
  }

  asyncTask->userPointer = prefix;
  startRLETask(asyncTask, NULL);

  return 0;
}

static bool isCommunicationPCEnabled(ZISAUXContext *context) {
  return context->masterCommArea->flag & ZISAUX_HOST_FLAG_COMM_PC_ON;
}

static int commTaskMain(RLETask *task) {

  ZISAUXContext *context = task->userPointer;
  ZISAUXCommArea *commArea = context->masterCommArea;

  auxutilWait(&commArea->commECB);

  // the only signal we handle is termination
  context->flags |= ZISAUX_CONTEXT_FLAG_TERM_COMMAND_RECEIVED;
  stcBaseShutdown(context->base);

  return 0;
}

static int launchCommTask(ZISAUXContext *context, STCBase *base) {


  int taskFlags = RLE_TASK_DISPOSABLE
                  | RLE_TASK_RECOVERABLE
                  | RLE_TASK_TCB_CAPABLE;
  RLETask *task = makeRLETask(base->rleAnchor, taskFlags, commTaskMain);
  if (task == NULL) {
    return RC_ZISAUX_ALLOC_FAILED;
  }

  task->userPointer = context;
  startRLETask(task, NULL);

  return RC_ZISAUX_OK;
}

#define MAIN_WAIT_MILLIS 10000
/* TODO This delay is used to give the console task time to terminate and
 * prevent A03. This should be addressed at some point. */
#define STCBASE_SHUTDOWN_DELAY_IN_SEC 5

static int run(STCBase *base, const ZISMainFunctionParms *mainParms) {

  initLogging();
  printStartMessage();

  int traceLevel = logGetLevel(NULL, LOG_COMP_STCBASE);

  int checkRC = checkEnv();
  if (checkRC != RC_ZISAUX_OK) {
    return checkRC;
  }

  ZISAUXContext *context = makeContext(base);
  if (context == NULL) {
    return RC_ZISAUX_ERROR;
  }

  ABENDInfo abendInfo = {ABEND_INFO_EYECATCHER};
  int rcvrPushRC = recoveryPush(
      "cmsStartMainLoop",
      RCVR_FLAG_RETRY | RCVR_FLAG_DELETE_ON_RETRY | RCVR_FLAG_PRODUCE_DUMP,
      "cmsStartMainLoop",
      extractABENDInfo, &abendInfo,
      NULL, NULL
  );

  if (rcvrPushRC != RC_RCV_OK) {
    zowelog(NULL, LOG_COMP_STCBASE, ZOWE_LOG_SEVERE,
            ZISAUX_LOG_HOST_ABEND_MSG,
            abendInfo.completionCode, abendInfo.reasonCode);
    notifyAllPendingCallers(context, (PEReleaseCode){ZISAUX_PE_RC_HOST_ABEND});
    return RC_ZISAUX_HOST_ABENDED;
  }

  stcRegisterModule(base, STC_MODULE_GENERIC, context,
                    NULL, NULL, workElementHandler, NULL);

  stcLaunchConsoleTask2(base, handleModifyCommand, context);

  int status = RC_ZISAUX_OK;

  do {

    int commTaskRC = launchCommTask(context, base);
    if (commTaskRC != RC_ZISAUX_OK) {
      status = commTaskRC;
      break;
    }

    int configRC = loadConfig(context, mainParms);
    if (configRC != RC_ZISAUX_OK) {
      status = configRC;
      break;
    }

    if (isCommunicationPCEnabled(context)) {
      int pcRC = establishCommunicationPC(context);
      if (pcRC != RC_ZISAUX_OK) {
        status = pcRC;
        break;
      }
    }

    int moduleLoadRC = loadUserModule(context);
    if (moduleLoadRC != RC_ZISAUX_OK) {
      status = moduleLoadRC;
      break;
    }

    int userInitRC = callUserModuleInit(context, traceLevel);
    if (userInitRC != RC_ZISAUX_OK) {
      status = userInitRC;
      break;
    }

    setReadyFlag(context, true);

    stcBaseMainLoop(base, MAIN_WAIT_MILLIS);

    setReadyFlag(context, false);

    if (!isNormalTermination(context)) {
      zowelog(NULL, LOG_COMP_STCBASE, ZOWE_LOG_SEVERE,
              ZISAUX_LOG_MAIN_LOOP_FAILURE_MSG);
      status = RC_ZISAUX_ERROR;
    }

    int userTermRC = callUserModuleTerm(context, traceLevel);
    if (userTermRC != RC_ZISAUX_OK) {
      status = (status != RC_ZISAUX_OK) ? status : userTermRC;
      break;
    }

    stcBaseShutdown(base);
    sleep(STCBASE_SHUTDOWN_DELAY_IN_SEC);

    setTermFlag(context, true);

  } while (0);

  recoveryPop();

  destroyContext(context);
  context = NULL;

  printStopMessage();

  return status;
}

int main() {

  const ZISMainFunctionParms *mainParms = GET_MAIN_FUNCION_PARMS();

  int rc = RC_ZISAUX_OK;

  STCBase *base = (STCBase *)safeMalloc31(sizeof(STCBase), "stcbase");
  stcBaseInit(base);
  {
    rc = run(base, mainParms);
  }
  stcBaseTerm(base);
  safeFree31((char *)base, sizeof(STCBase));
  base = NULL;

  return rc;
}

#define auxServerDESCTs AUXDSECT
void auxServerDESCTs(){

  __asm(

      "         DS    0D                                                       \n"
      "PCHSTACK DSECT ,                                                        \n"
      /* parameters on stack */
      "PCHPARM  DS    0D                   PC handle parm list                 \n"
      "PCHPLEYE DS    CL8                  eyecatcher (AUXPCPRM)               \n"
      "PCHPLLPL DS    D                    latent parm list                    \n"
      "PCHPLAPL DS    D                    parm from ZIS                       \n"
      "PCHPLPAD DS    0D                   padding                             \n"
      "PCHPARML EQU   *-PCHPARM                                                \n"
      /* 64-bit save area */
      "PCHSA    DS    0D                                                       \n"
      "PCHSARSV DS    CL4                                                      \n"
      "PCHSAEYE DS    CL4                  eyecatcher F1SA                     \n"
      "PCHSAGPR DS    15D                  GPRs                                \n"
      "PCHSAPRV DS    AD                   previous save area                  \n"
      "PCHSANXT DS    AD                   next save area                      \n"
      "PCHSTCKL EQU   *-PCHSTACK                                               \n"
      "         EJECT ,                                                        \n"

      "         CSECT ,                                                        \n"
      "         RMODE ANY                                                      \n"
  );

}


/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/
