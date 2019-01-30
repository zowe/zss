

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

#include <metal/metal.h>
#include <metal/limits.h>
#include <metal/stddef.h>
#include <metal/stdio.h>
#include <metal/stdlib.h>
#include <metal/string.h>

#ifndef METTLE
#error Non-metal C code is not supported
#endif

#include "zowetypes.h"
#include "alloc.h"
#include "crossmemory.h"
#include "logging.h"
#include "metalio.h"
#include "zos.h"
#include "qsam.h"
#include "stcbase.h"
#include "utils.h"
#include "recovery.h"

#include "zis/message.h"
#include "zis/parm.h"
#include "zis/server.h"

/*

BUILD INSTRUCTIONS:

Build using build.sh

DEPLOYMENT:

See details in the ZSS Cross Memory Server installation guide

*/

#define ZIS_PARM_DDNAME "PARMLIB"
#define ZIS_PARM_MEMBER_PREFIX                CMS_PROD_ID"IP"
#define ZIS_PARM_MEMBER_DEFAULT_SUFFIX        "00"
#define ZIS_PARM_MEMBER_SUFFIX_SIZE            2
#define ZIS_PARM_MEMBER_NAME_MAX_SIZE          8

#define ZIS_PARM_MEMBER_SUFFIX                "MEM"
#define ZIS_PARM_SERVER_NAME                  "NAME"
#define ZIS_PARM_COLD_START                   "COLD"
#define ZIS_PARM_DEBUG_MODE                   "DEBUG"

#define ZIS_PARMLIB_PARM_SERVER_NAME          CMS_PROD_ID".NAME"

#define CMS_DEBUG($globalArea, $fmt, ...) do {\
  if ($globalArea->pcLogLevel >= ZOWE_LOG_DEBUG) {\
    cmsPrintf(&$globalArea->serverName, CMS_LOG_DEBUG_MSG_ID" " $fmt, \
        ##__VA_ARGS__);\
  }\
} while (0)


#define CMS_DEBUG2($globalArea, $level, $fmt, ...) do {\
  if ($level >= ZOWE_LOG_DEBUG) {\
    cmsPrintf(&$globalArea->serverName, CMS_LOG_DEBUG_MSG_ID" " $fmt, \
        ##__VA_ARGS__);\
  }\
} while (0)

static int handleVerifyPassword(AuthServiceParmList *parmList,
                                const CrossMemoryServerGlobalArea *globalArea) {
  ACEE *acee = NULL;
  int safRC = 0, racfRC = 0, racfRsn = 0;
  int deleteSAFRC = 0, deleteRACFRC = 0, deleteRACFRsn = 0;
  int rc = RC_ZIS_AUTHSRV_OK;

  CMS_DEBUG(globalArea, "handleVerifyPassword(): username = %s, password = %s\n",
      parmList->userIDNullTerm, "******");
  safRC = safVerify(VERIFY_CREATE, parmList->userIDNullTerm,
      parmList->passwordNullTerm, &acee, &racfRC, &racfRsn);
  CMS_DEBUG(globalArea, "safVerify(VERIFY_CREATE) safStatus = %d, RACF RC = %d, "
      "RSN = %d, ACEE=0x%p\n", safRC, racfRC, racfRsn, acee);
  if (safRC != 0) {
    rc = RC_ZIS_AUTHSRV_SAF_ERROR;
    goto acee_deleted;
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
  return rc;
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

  CMS_DEBUG(globalArea, "handleEntityCheck(): user = %s, entity = %s, class = %s,"
      " access = %x\n", parmList->userIDNullTerm, parmList->entityNullTerm,
      parmList->classNullTerm, parmList->access);
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
  safRC = safAuth(0, parmList->classNullTerm, parmList->entityNullTerm,
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
                         SAFStatus *safStatus,
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

  CMS_DEBUG2(globalArea, traceLevel,
             "handleAccessRetrieval(): user=\'%s\', entity=\'%s\', "
             "class=\'%s\', access = 0x%p\n",
             parmList->userIDNullTerm, parmList->entityNullTerm,
             parmList->classNullTerm, parmList->access);

  rc = getAccessType(globalArea,
                     parmList->classNullTerm,
                     parmList->userIDNullTerm,
                     parmList->entityNullTerm,
                     &parmList->access,
                     &parmList->safStatus,
                     &parmList->abendInfo,
                     traceLevel);

  CMS_DEBUG2(globalArea, traceLevel, "handleAccessRetrieval() done\n");

  return rc;
}

static int authServiceFunction(CrossMemoryServerGlobalArea *globalArea, CrossMemoryService *service, void *parm) {

  int logLevel = globalArea->pcLogLevel;

  if (logLevel >= ZOWE_LOG_DEBUG) {
    cmsPrintf(&globalArea->serverName, CMS_LOG_DEBUG_MSG_ID" in authServiceFunction, parm = 0x%p\n", parm);
  }

  void *clientParmAddr = parm;
  if (clientParmAddr == NULL) {
    return RC_ZIS_AUTHSRV_PARMLIST_NULL;
  }

  AuthServiceParmList localParmList;
  cmCopyFromPrimaryWithCallerKey(&localParmList, clientParmAddr, sizeof(AuthServiceParmList));

  if (memcmp(localParmList.eyecatcher, ZIS_AUTH_SERVICE_PARMLIST_EYECATCHER, sizeof(localParmList.eyecatcher))) {
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
  cmCopyToPrimaryWithCallerKey(clientParmAddr, &localParmList, sizeof(AuthServiceParmList));
  return handlerRC;
}

typedef struct SToken_tag {
  char token[8];
} SToken;

__asm("SCHGLBPL IEAMSCHD MF=(L,SCHGLBPL)" : "DS"(SCHGLBPL));

static int scheduleSynchSRB(Addr31 entryPoint, Addr31 parm, SToken token, Addr31 frrAddress,
                            int * __ptr32 compCode, int * __ptr32 srbRC,  int * __ptr32 srbRSN) {

  __asm("SCHGLBPL IEAMSCHD MF=(L,SCHGLBPL)" : "DS"(parmList));
  parmList = SCHGLBPL;

  int returnCode = 0;

  unsigned int localEP = ((int)entryPoint) | 0x80000000;
  unsigned int localParm = (int)parm;
  unsigned int localFRRAddress = (frrAddress != NULL) ? (((int)frrAddress) | 0x80000000) : 0;
  SToken localSToken = token;

  __asm(
      ASM_PREFIX

#ifdef _LP64
      "         SAM31                                                          \n"
      "         SYSSTATE AMODE64=NO                                            \n"
#endif

      "         IEAMSCHD EPADDR=(%4)"
      ",ENV=STOKEN"
      ",PRIORITY=LOCAL"
      ",SYNCH=YES"
      ",PARM=(%5)"
      ",TARGETSTOKEN=(%6)"
      ",FRRADDR=(%7)"
      ",SYNCHCOMPADDR=%1"
      ",SYNCHCODEADDR=%2"
      ",SYNCHRSNADDR=%3"
      ",RETCODE=%0"
      ",MF=(E,(%8),COMPLETE)                                                   \n"

#ifdef _LP64
      "         SAM64                                                          \n"
      "         SYSSTATE AMODE64=YES                                           \n"
#endif

      : "=m"(returnCode), "=m"(compCode), "=m"(srbRC), "=m"(srbRSN)
      : "r"(&localEP), "r"(&localParm), "r"(&localSToken), "r"(&localFRRAddress), "r"(&parmList)
      : "r0", "r1", "r14", "r15"
  );

  return returnCode;
}

static Addr31 getSRBSnarfer() {

  Addr31 routineAddress = NULL;

  __asm(
      ASM_PREFIX
      "         LARL  1,L$SNRRTN                                               \n"
      "         ST    1,%0                                                     \n"
      "         J     L$SNREX                                                  \n"
      "L$SNRRTN DS    0H                                                       \n"

      /* establish addressability */
      "         SAM64                     go AMODE64                           \n"
      "         PUSH  USING                                                    \n"
      "         DROP                                                           \n"
      "         BAKR  14,0                                                     \n"
      "         LARL  10,L$SNRRTN                                              \n"
      "         USING L$SNRRTN,10                                              \n"

      "         LLGFR 8,1                 copy our transfer buffer address     \n"
      "         USING SNFTBUF,8                                                \n"

      "         CLC   SNFTBEYE,=C'RSTRNBUF' eyecatcher okay?                   \n"
      "         BE    L$SNCP10            yes, proceed                         \n"
      "         LA    15,X'08'            set RC                               \n"
      "         LA    0,X'01'             set RSN                              \n"
      "         B     L$SNRRET            leave                                \n"

      "L$SNCP10 DS    0H                                                       \n"
#ifdef ZIS_SNARFER_USE_SHARED_MEM_OBJ
      "         IARV64 REQUEST=SHAREMEMOBJ"
      ",USERTKN=SNFTBTKN"
      ",RANGLIST=SNFTBRLA"
      ",NUMRANGE=1"
      ",COND=YES"
      ",RETCODE=SNFTBRC4"
      ",RSNCODE=SNFTBRS4"
      ",MF=(E,SNFTBPL4)                                                        \n"
      "         LTR   15,15                                                    \n"
      "         BZ    L$SNCP20                                                 \n"
      "         LA    15,X'08'            set RC                               \n"
      "         LA    0,X'02'             set RSN                              \n"
      "         B     L$SNRRET            leave                                \n"
#endif /* ZIS_SNARFER_USE_SHARED_MEM_OBJ */

      "L$SNCP20 DS    0H                                                       \n"
      "         L     1,SNFTBSRK          source key                           \n"
      "         LG    2,SNFTBDNA          destination address                  \n"
      "         LG    4,SNFTBSRA          source address                       \n"
      "         LLGT  5,SNFTBSZ           size                                 \n"

      "L$SNCP30 DS    0H                                                       \n"
      "         LTGR  5,5                 size > 0?                            \n"
      "         BP    L$SNCP40            yes, copy                            \n"
      "         LA    15,X'777'           set RC                               \n"
      "         LA    0,X'00'             set RSN                              \n"
      "         B     L$SNRRET            <=0, leave                           \n"

      "L$SNCP40 DS    0H                                                       \n"
      "         LA    0,256                                                    \n"
      "         CGR   5,0                 size left > 256?                     \n"
      "         BH    L$SNCP50            yes, go subtract 256                 \n"
      "         LGR   0,5                 no, load bytes left to r0            \n"
      "L$SNCP50 DS    0H                                                       \n"
      "         SGR   5,0                 subtract 256                         \n"
      "         BCTR  0,0                 bytes to copy - 1                    \n"
      "         MVCSK 0(2),0(4)           copy                                 \n"
      "         LA    2,256(,2)           bump destination address             \n"
      "         LA    4,256(,4)           bump source address                  \n"
      "         B     L$SNCP30            repeat                               \n"

      "L$SNCP60 DS    0H                                                       \n"
#ifdef ZIS_SNARFER_USE_SHARED_MEM_OBJ
      "         IARV64 REQUEST=DETACH"
      ",USERTKN=SNFTBTKN"
      ",MEMOBJSTART=SNFTBDNA"
      ",OWNER=YES"
      ",COND=YES"
      ",RETCODE=SNFTBRC4"
      ",RSNCODE=SNFTBRS4"
      ",MF=(E,SNFTBPL4)                                                        \n"
      "         LTR   15,15                                                    \n"
      "         BZ    L$SNRRET                                                 \n"
      "         LA    15,X'08'            set RC                               \n"
      "         LA    0,X'03'             set RSN                              \n"
      "         B     L$SNRRET            leave                                \n"
#endif /* ZIS_SNARFER_USE_SHARED_MEM_OBJ */

      /* return to caller */
      "L$SNRRET DS    0H                                                       \n"
      "         PR    ,                   return                               \n"
      /* non executable code */
      "         LTORG                                                          \n"
      "         POP   USING                                                    \n"

      "L$SNREX  DS    0H                                                       \n"
      : "=m"(routineAddress)
      :
      : "r1"
  );

  return routineAddress;
}

__asm("IARV64GL IARV64 MF=(L,IARV64GL)" : "DS"(IARV64GL));

static uint64 getSharedMemObject(uint64 segmentCount, uint64 token, int *iarv64RC, int *iarv64RSN) {

  uint64 data = 0;

  int localRC = 0;
  int localRSN = 0;

  __asm(" IARV64 MF=L" : "DS"(getSharedMemObjPlist));
  getSharedMemObjPlist = IARV64GL;

  __asm(
      "         IARV64 REQUEST=GETSHARED"
      ",USERTKN=(%2)"
      ",COND=YES"
      ",SEGMENTS=(%3)"
      ",ORIGIN=(%4)"
      ",RETCODE=%0"
      ",RSNCODE=%1"
      ",MF=(E,(%5))                                                            \n"
      : "=m"(localRC), "=m"(localRSN)
      : "r"(&token), "r"(&segmentCount), "r"(&data), "r"(&getSharedMemObjPlist)
      : "r0", "r1", "r14", "r15"
  );

  if (iarv64RC) {
    *iarv64RC = localRC;
  }
  if (iarv64RSN) {
    *iarv64RSN = localRSN;
  }

  return data;
}

static void shareMemObject(uint64 object, uint64 token, int *iarv64RC, int *iarv64RSN) {

  int localRC = 0;
  int localRSN = 0;

  struct {
    uint64 vsa;
    uint64 reserved;
  } rangeList = {object, 0};
  uint64 rangeListAddress = (uint64)&rangeList;

  __asm(" IARV64 MF=L" : "DS"(shareMemObjPlist));
  shareMemObjPlist = IARV64GL;

  __asm(
      "         IARV64 REQUEST=SHAREMEMOBJ"
      ",USERTKN=(%2)"
      ",RANGLIST=(%3)"
      ",NUMRANGE=1"
      ",COND=YES"
      ",RETCODE=%0"
      ",RSNCODE=%1"
      ",MF=(E,(%4))                                                            \n"
      : "=m"(localRC), "=m"(localRSN)
      : "r"(&token), "r"(&rangeListAddress), "r"(&shareMemObjPlist)
      : "r0", "r1", "r14", "r15"
  );

}

static void detachSharedMemObject(uint64 object, uint64 token, int *iarv64RC, int *iarv64RSN) {

  int localRC = 0;
  int localRSN = 0;

  __asm(" IARV64 MF=L" : "DS"(detachPlist));
  detachPlist = IARV64GL;

  __asm(
      "         IARV64 REQUEST=DETACH"
      ",USERTKN=(%2)"
      ",MEMOBJSTART=(%3)"
      ",OWNER=YES"
      ",COND=YES"
      ",RETCODE=%0"
      ",RSNCODE=%1"
      ",MF=(E,(%4))                                                            \n"
      : "=m"(localRC), "=m"(localRSN)
      : "r"(&token), "r"(&object), "r"(&detachPlist)
      : "r0", "r1", "r14", "r15"
  );

  if (iarv64RC){
    *iarv64RC = localRC;
  }
  if (iarv64RSN){
    *iarv64RSN = localRSN;
  }

}

static void detachSharedMemObjectFromSystem(uint64 object, uint64 token, int *iarv64RC, int *iarv64RSN) {

  int localRC = 0;
  int localRSN = 0;

  __asm(" IARV64 MF=L" : "DS"(detachPlist));
  detachPlist = IARV64GL;

  __asm(
      "         IARV64 REQUEST=DETACH"
      ",MATCH=MOTOKEN"
      ",USERTKN=(%2)"
      ",AFFINITY=SYSTEM"
      ",COND=YES"
      ",RETCODE=%0"
      ",RSNCODE=%1"
      ",MF=(E,(%4))                                                            \n"
      : "=m"(localRC), "=m"(localRSN)
      : "r"(&token), "r"(&object), "r"(&detachPlist)
      : "r0", "r1", "r14", "r15"
  );

  if (iarv64RC){
    *iarv64RC = localRC;
  }
  if (iarv64RSN){
    *iarv64RSN = localRSN;
  }

}

ZOWE_PRAGMA_PACK
typedef struct TransferBuffer_tag {
  char eyecatcher[8];
#define TRANSFER_BUFFER_EYECATCHER  "RSTRNBUF"

  unsigned int copySize;
  unsigned int sourceKey;
  uint64 sourceAddress;
  uint64 destinationAddress;
#ifdef ZIS_SNARFER_USE_SHARED_MEM_OBJ
  uint64 sharedMemObjSegmentCount;
  uint64 sharedMemObjToken;

  struct {
    uint64 vsa;
    uint64 reserved;
  } rangeList;
  uint64 rangeListAddress;
  int iarv64RC;
  int iarv64RSN;
  char iarv64Plist[128];
#else
  char data[0];
#endif /* ZIS_SNARFER_USE_SHARED_MEM_OBJ */
} TransferBuffer;
ZOWE_PRAGMA_PACK_RESET

static int allocateTransferBuffer(CrossMemoryServerGlobalArea *globalArea, unsigned int copySize, TransferBuffer **buffer, int traceLevel) {

#ifdef ZIS_SNARFER_USE_SHARED_MEM_OBJ
  unsigned int commonStorageSize = sizeof(TransferBuffer);
#else
  unsigned int commonStorageSize = sizeof(TransferBuffer) + copySize;
#endif /* ZIS_SNARFER_USE_SHARED_MEM_OBJ */

  cmsAllocateECSAStorage2(globalArea, commonStorageSize, (void **)buffer);
  TransferBuffer *transferBuffer = *buffer;
  if (transferBuffer == NULL) {
    return RC_ZIS_SNRFSRV_ECSA_ALLOC_FAILED;
  }
  memset(transferBuffer, 0, sizeof(TransferBuffer));
  memcpy(transferBuffer->eyecatcher, TRANSFER_BUFFER_EYECATCHER, sizeof(transferBuffer->eyecatcher));
  transferBuffer->copySize = copySize;

  if (traceLevel >= ZOWE_LOG_DEBUG) {
    cmsPrintf(&globalArea->serverName, CMS_LOG_DEBUG_MSG_ID" allocateTransferBuffer: transferBuffer=0x%p\n", transferBuffer);
  }

  int status = RC_ZIS_SNRFSRV_OK;

#ifdef ZIS_SNARFER_USE_SHARED_MEM_OBJ
  int iarv64RC = 0, iarv64RSN = 0;
  unsigned int sharedMemObjSegmentCount = (copySize >> 20) + (copySize & 0xFFFFF ? 1 : 0);
  uint64 sharedMemObjToken = (uint64)getASCB() << 32 | (uint64)transferBuffer;
  uint64 sharedMemObj = 0;

  if (status == RC_ZIS_SNRFSRV_OK) {
    sharedMemObj = getSharedMemObject(sharedMemObjSegmentCount, sharedMemObjToken, &iarv64RC, &iarv64RSN);
    if (iarv64RC >= 8) {
      status = RC_ZIS_SNRFSRV_SHARED_OBJ_ALLOC_FAILED;
    }

    if (traceLevel >= ZOWE_LOG_DEBUG) {
      cmsPrintf(&globalArea->serverName, CMS_LOG_DEBUG_MSG_ID" allocateTransferBuffer: getSharedObject rc=%d, rsn=%d (%llu, 0x%016llX, 0x%016llX)\n",
          iarv64RC, iarv64RSN, sharedMemObjSegmentCount, sharedMemObjToken, sharedMemObj);
    }
  }

  if (status == RC_ZIS_SNRFSRV_OK) {
    shareMemObject(sharedMemObj, sharedMemObjToken, &iarv64RC, &iarv64RSN);
    if (iarv64RC >= 8) {
      status = RC_ZIS_SNRFSRV_SHARED_OBJ_SHARE_FAILED;
    }

    if (traceLevel >= ZOWE_LOG_DEBUG) {
      cmsPrintf(&globalArea->serverName, CMS_LOG_DEBUG_MSG_ID" shareObject: getSharedObject rc=%d, rsn=%d (0x%016llX, 0x%016llX)\n",
          iarv64RC, iarv64RSN, sharedMemObjToken, sharedMemObj);
    }
  }

  if (status == RC_ZIS_SNRFSRV_OK) {
    transferBuffer->destinationAddress = sharedMemObj;
    transferBuffer->sharedMemObjSegmentCount = sharedMemObjSegmentCount;
    transferBuffer->sharedMemObjToken = sharedMemObjToken;
    transferBuffer->rangeList.vsa = sharedMemObj;
    transferBuffer->rangeList.reserved = 0;
    transferBuffer->rangeListAddress = (uint64)&transferBuffer->rangeList;
    transferBuffer->iarv64RC = 0;
    transferBuffer->iarv64RSN = 0;
  } else {
    if (sharedMemObj != 0) {
      detachSharedMemObject(sharedMemObj, sharedMemObjToken, NULL, NULL);
      sharedMemObj = 0;
    }
    if (transferBuffer != NULL) {
      cmsFreeECSAStorage(globalArea, transferBuffer, commonStorageSize);
      transferBuffer = NULL;
    }
  }
#else
  transferBuffer->destinationAddress = (uint64)&transferBuffer->data;
#endif /* ZIS_SNARFER_USE_SHARED_MEM_OBJ */

  return status;
}

static int freeTransferBuffer(CrossMemoryServerGlobalArea *globalArea, TransferBuffer **buffer, int traceLevel) {

  int status = RC_ZIS_SNRFSRV_OK;

#ifdef ZIS_SNARFER_USE_SHARED_MEM_OBJ
  unsigned int commonStorageSize = sizeof(TransferBuffer);
#else
  unsigned int commonStorageSize = sizeof(TransferBuffer) + (*buffer)->copySize;
#endif /* ZIS_SNARFER_USE_SHARED_MEM_OBJ */


#ifdef ZIS_SNARFER_USE_SHARED_MEM_OBJ

  int iarv64RC = 0, iarv64RSN = 0;
  detachSharedMemObjectFromSystem(buffer->destinationAddress, buffer->sharedMemObjToken, &iarv64RC, &iarv64RSN);
  if (iarv64RC >= 8) {
    status = (status != RC_ZIS_SNRFSRV_OK) ? status : RC_ZIS_SNRFSRV_SHARED_OBJ_DETACH_FAILED;
  }

  if (traceLevel >= ZOWE_LOG_DEBUG) {
    cmsPrintf(&globalArea->serverName, CMS_LOG_DEBUG_MSG_ID" freeTransferBuffer: detachSharedObject(system) rc=%d, rsn=%d (0x%016llX, 0x%016llX)\n",
        iarv64RC, iarv64RSN, buffer->destinationAddress, buffer->sharedMemObjToken);
  }

  iarv64RC = 0, iarv64RSN = 0;
  detachSharedMemObject(buffer->destinationAddress, buffer->sharedMemObjToken, &iarv64RC, &iarv64RSN);
  if (iarv64RC >= 8) {
    status = RC_ZIS_SNRFSRV_SHARED_OBJ_DETACH_FAILED;
  }

  if (traceLevel >= ZOWE_LOG_DEBUG) {
    cmsPrintf(&globalArea->serverName, CMS_LOG_DEBUG_MSG_ID" freeTransferBuffer: detachSharedObject rc=%d, rsn=%d (0x%016llX, 0x%016llX)\n",
        iarv64RC, iarv64RSN, buffer->destinationAddress, buffer->sharedMemObjToken);
  }

#endif /* ZIS_SNARFER_USE_SHARED_MEM_OBJ */

  cmsFreeECSAStorage2(globalArea, (void **)buffer, commonStorageSize);

  return status;
}

#pragma pack(packed)
typedef struct SnarferContext_tag {
  char eyecatcher[8];
#define SNARFER_CONTEXT_EYECATCHER  "RSSNRFCX"
  CrossMemoryServerGlobalArea *globalArea;
  TransferBuffer *transferBuffer;
  int traceLevel;
} SnarferContext;
#pragma pack(reset)

static void cleanupSnarfer(struct RecoveryContext_tag * __ptr32 context,
                           SDWA * __ptr32 sdwa,
                           void * __ptr32 userData) {

  int cc = 0, rsn = 0;
  recoveryGetABENDCode(sdwa, &cc, &rsn);

  /* Percolate if we haven't been cancelled, the global PC recovery will
   * report the ABEND. */
  if (cc != 0x222 && cc != 0x522) {
    return;
  }

  SnarferContext *snarferContext = userData;
  if (memcmp(snarferContext->eyecatcher, SNARFER_CONTEXT_EYECATCHER,
             sizeof(snarferContext->eyecatcher))) {
    return;
  }

  CrossMemoryServerGlobalArea *globalArea = snarferContext->globalArea;

  if (snarferContext->traceLevel >= ZOWE_LOG_DEBUG) {
    cmsPrintf(&globalArea->serverName, CMS_LOG_DEBUG_MSG_ID" we got canceled - "
              "ga=%p, tb=%p\n", globalArea, snarferContext->transferBuffer);
  }

  if (snarferContext->transferBuffer != NULL) {
    freeTransferBuffer(globalArea, &snarferContext->transferBuffer,
                       snarferContext->traceLevel);
  }

}

static int snarferServiceFunction(CrossMemoryServerGlobalArea *globalArea, CrossMemoryService *service, void *parm) {

  int logLevel = globalArea->pcLogLevel;

  void *clientParmAddr = parm;
  if (clientParmAddr == NULL) {
    return RC_ZIS_SNRFSRV_PARMLIST_NULL;
  }

  SnarferServiceParmList localParmList;
  cmCopyFromSecondaryWithCallerKey(&localParmList, clientParmAddr, sizeof(localParmList));

  if (memcmp(localParmList.eyecatcher, ZIS_SNARFER_SERVICE_PARMLIST_EYECATCHER, sizeof(localParmList.eyecatcher))) {
    return RC_ZIS_SNRFSRV_BAD_EYECATCHER;
  }

  void *dest = (void *)localParmList.destinationAddress;
  void *src = (void *)localParmList.sourceAddress;
  ASCB *srcASCB = localParmList.sourceASCB;
  int srcKey = localParmList.sourceKey;
  unsigned int copySize = localParmList.size;

  if (logLevel >= ZOWE_LOG_DEBUG) {
    cmsPrintf(&globalArea->serverName, CMS_LOG_DEBUG_MSG_ID" snarferServiceFunction: dest=0x%p, src=0x%p, srcASCB=0x%p, srcKey=0x%X, copySize=%u\n",
        dest, src, srcASCB, srcKey, copySize);
  }

  if ((uint64)dest < 0x6000) {
    return RC_ZIS_SNRFSRV_BAD_DEST;
  }

  if (srcASCB == NULL) {
    return RC_ZIS_SNRFSRV_BAD_ASCB;
  }

  if (copySize > CMS_MAX_ECSA_BLOCK_SIZE) {
    return RC_ZIS_SNRFSRV_BAD_SIZE;
  }

  int status = RC_ZIS_SNRFSRV_OK;

  SnarferContext cntx = {
      .eyecatcher = SNARFER_CONTEXT_EYECATCHER,
      .globalArea = globalArea,
      .transferBuffer = NULL,
      .traceLevel = logLevel,
  };

  /* global snarfer recovery - it'll always percolate */
  int pushRC = recoveryPush("snarferServiceFunction", RCVR_FLAG_NONE, NULL,
                            cleanupSnarfer, &cntx, NULL, NULL);
  if (pushRC != RC_RCV_OK) {
    return RC_ZIS_SNRFSRV_RECOVERY_ERROR;
  }

  ASSB *sourceASSB = NULL;
  if (status == RC_ZIS_SNRFSRV_OK) {
    int pushRC = recoveryPush("snarferServiceFunction->getASSB", RCVR_FLAG_RETRY,
                              NULL, cleanupSnarfer, &cntx, NULL, NULL);
    if (pushRC == RC_RCV_OK) {
      sourceASSB = getASSB(srcASCB);
    } else if (pushRC == RC_RCV_ABENDED) {
      status = RC_ZIS_SNRFSRV_SRC_ASSB_ABEND;
    } else {
      status = RC_ZIS_SNRFSRV_RECOVERY_ERROR;
    }
    recoveryPop();
  }

  if (status == RC_ZIS_SNRFSRV_OK) {
    int allocTransferBufRC = allocateTransferBuffer(globalArea, copySize,
                                                    &cntx.transferBuffer,
                                                    logLevel);
    if (allocTransferBufRC == RC_ZIS_SNRFSRV_OK) {
      cntx.transferBuffer->sourceAddress = localParmList.sourceAddress;
      cntx.transferBuffer->sourceKey = localParmList.sourceKey;
      if (logLevel >= ZOWE_LOG_DEBUG) {
        cmsPrintf(&globalArea->serverName,
                  CMS_LOG_DEBUG_MSG_ID" transfer buffer allocated @ "
                  "0x%p, %u, %u, 0%llX\n",
                  cntx.transferBuffer, cntx.transferBuffer->copySize,
                  cntx.transferBuffer->sourceKey,
                  cntx.transferBuffer->sourceAddress);
      }
    } else {
      status = allocTransferBufRC;
    }
  }

  if (status == RC_ZIS_SNRFSRV_OK) {
    int pushRC = recoveryPush("snarferServiceFunction->SRB", RCVR_FLAG_RETRY,
                              NULL, cleanupSnarfer, &cntx, NULL, NULL);
    if (pushRC == RC_RCV_OK) {

      Addr31 srbRoutine = getSRBSnarfer();
      SToken stoken;
      memcpy(stoken.token, sourceASSB->assbstkn, sizeof(stoken.token));

      if (logLevel >= ZOWE_LOG_DEBUG) {
        cmsPrintf(&globalArea->serverName, CMS_LOG_DEBUG_MSG_ID" srbRoutine=0x%p, stoken=%llX\n", srbRoutine, *(uint64 *)&stoken);
      }

      int compCode = 0, srbRC = 0, srbRSN = 0;
      int ieamschdRC = scheduleSynchSRB(srbRoutine, cntx.transferBuffer, stoken, NULL, &compCode, &srbRC, &srbRSN);
      if (logLevel >= ZOWE_LOG_DEBUG) {
        cmsPrintf(&globalArea->serverName, CMS_LOG_DEBUG_MSG_ID" ieamschdRC=0x%08X, compCode=0x%08X, srbRC=0x%08X, srbRSN=0x%08X\n", ieamschdRC, compCode, srbRC, srbRSN);
      }
      if (ieamschdRC != 0) {
        status = RC_ZIS_SNRFSRV_SRC_IEAMSCHD_FAILED;
      }

    } else if (pushRC == RC_RCV_ABENDED) {
      status = RC_ZIS_SNRFSRV_SRC_IEAMSCHD_ABEND;
    } else {
      status = RC_ZIS_SNRFSRV_RECOVERY_ERROR;
    }
    recoveryPop();
  }

  if (status == RC_ZIS_SNRFSRV_OK) {
    cmCopyToSecondaryWithCallerKey((void *)localParmList.destinationAddress,
                                   (void *)cntx.transferBuffer->destinationAddress,
                                   localParmList.size);
  }

  if (cntx.transferBuffer != NULL) {
    int freeTransferBufRC = freeTransferBuffer(globalArea, &cntx.transferBuffer, logLevel);
    if (freeTransferBufRC != RC_ZIS_SNRFSRV_OK) {
      status = (status != RC_ZIS_SNRFSRV_OK) ? status : freeTransferBufRC;
    }
  }

  recoveryPop(); /* global snarfer recovery */

  return status;
}

static int callNWMService(char jobName[],
                          char *nmiBuffer,
                          unsigned int nmiBufferSize,
                          int *rc,
                          int *rsn) {

  int alet = 0;
  int returnValue = 0;
  char callParmListBuffer[64];

  __asm(
      ASM_PREFIX
      "         CALL EZBNMIFR"
      ",("
      "(%[jobName]),"
      "(%[buffer]),"
      "(%[bufferALET]),"
      "(%[bufferSize]),"
      "(%[rv]),(%[rc]),(%[rsn])"
      "),MF=(E,(%[parmList]))"
      "                                                                        \n"
      :
      : [jobName]"r"(jobName),
        [buffer]"r"(nmiBuffer),
        [bufferALET]"r"(&alet),
        [bufferSize]"r"(&nmiBufferSize),
        [rv]"r"(&returnValue),
        [rc]"r"(rc),
        [rsn]"r"(rsn),
        [parmList]"r"(&callParmListBuffer)
      : "r0", "r1", "r14", "r15"
  );

  return returnValue;
}

static int nwmServiceFunction(CrossMemoryServerGlobalArea *globalArea,
                              CrossMemoryService *service,
                              void *parm) {

  int traceLevel = globalArea->pcLogLevel;

  void *clientParmAddr = parm;
  if (clientParmAddr == NULL) {
    return RC_ZIS_NWMSRV_PARMLIST_NULL;
  }

  ZISNWMServiceParmList localParmList = {0};
  cmCopyFromSecondaryWithCallerKey(&localParmList, clientParmAddr,
                                   sizeof(localParmList));

  if (memcmp(localParmList.eyecatcher, ZIS_NWM_SERVICE_PARMLIST_EYECATCHER,
             sizeof(localParmList.eyecatcher))) {
    return RC_ZIS_NWMSRV_BAD_EYECATCHER;
  }

  if (localParmList.nmiBuffer == NULL) {
    return RC_ZIS_NWMSRV_BUFFER_NULL;
  }

  if (traceLevel < localParmList.traceLevel) {
    traceLevel = localParmList.traceLevel;
  }

  unsigned int localNMIBUfferSize = localParmList.nmiBufferSize;
  char *localNMIBuffer = safeMalloc64(localNMIBUfferSize, "localNMIBuffer");
  if (localNMIBuffer == NULL) {
    return RC_ZIS_NWMSRV_BUFFER_NOT_ALLOCATED;
  }

  CMS_DEBUG2(globalArea, traceLevel,
             "NWMSRV: job = \'%8.8s\', buffer = %p, local buffer = %p, "
             "size = %u\n",
             localParmList.nmiJobName,
             localParmList.nmiBuffer,
             localNMIBuffer,
             localParmList.nmiBufferSize);

  int status = RC_ZIS_NWMSRV_OK;
  int pushRC = recoveryPush("NWM recovery",
                            RCVR_FLAG_NONE | RCVR_FLAG_DELETE_ON_RETRY,
                            NULL, NULL, NULL, NULL, NULL);
  if (pushRC == RC_RCV_OK) {

    cmCopyFromSecondaryWithCallerKey(localNMIBuffer, localParmList.nmiBuffer,
                                     localParmList.nmiBufferSize);

    localParmList.nmiReturnValue = callNWMService(localParmList.nmiJobName,
                                                  localNMIBuffer,
                                                  localNMIBUfferSize,
                                                  &localParmList.nmiReturnCode,
                                                  &localParmList.nmiReasonCode);

  } else {

    if (pushRC == RC_RCV_ABENDED) {
      status = RC_ZIS_NWMSRV_NWM_ABENDED;
    } else {
      status = RC_ZIS_NWMSRV_RECOVERY_ERROR;
    }

  }

  if (pushRC == RC_RCV_OK) {
    recoveryPop();
  }

  if (status == RC_ZIS_NWMSRV_OK) {

    cmCopyToSecondaryWithCallerKey(localParmList.nmiBuffer, localNMIBuffer,
                                   localParmList.nmiBufferSize);

    cmCopyToSecondaryWithCallerKey(clientParmAddr, &localParmList,
                                   sizeof(localParmList));

    if (localParmList.nmiReturnValue < 0) {
      status = RC_ZIS_NWMSRV_NWM_FAILED;
    }

  }

  safeFree64(localNMIBuffer, localNMIBUfferSize);
  localNMIBuffer = NULL;

  CMS_DEBUG2(globalArea, traceLevel,
             "NWMSRV: status = %d, NMI RV = %d, RC = %d, RSN = 0x%08X\n",
             status,
             localParmList.nmiReturnValue,
             localParmList.nmiReturnCode,
             localParmList.nmiReasonCode);

  return status;
}

ZOWE_PRAGMA_PACK
typedef struct MainFunctionParms_tag {
  unsigned short textLength;
  char text[0];
} MainFunctionParms;
ZOWE_PRAGMA_PACK_RESET

#define GET_MAIN_FUNCION_PARMS() \
({ \
  unsigned int r1; \
  __asm(" ST    1,%0 "  : "=m"(r1) : : "r1"); \
  MainFunctionParms * __ptr32 parms = NULL; \
  if (r1 != 0) { \
    parms = (MainFunctionParms *)((*(unsigned int *)r1) & 0x7FFFFFFF); \
  } \
  parms; \
})

static bool isKeywordInPassedParms(const char *keyword, const MainFunctionParms *parms) {

  if (parms == NULL) {
    return false;
  }

  unsigned int keywordLength = strlen(keyword);
  for (int startIdx = 0; startIdx < parms->textLength; startIdx++) {
    int endIdx;
    for (endIdx = startIdx; endIdx < parms->textLength; endIdx++) {
      if (parms->text[endIdx] == ',' || parms->text[endIdx] == ' ') {
        break;
      }
    }
    unsigned int tokenLength = endIdx - startIdx;
    if (tokenLength == keywordLength) {
      if (memcmp(keyword, &parms->text[startIdx], tokenLength) == 0) {
        return true;
      }
    }
    startIdx = endIdx;
  }

  return false;
}

static char *getPassedParmValue(const char *parmName, const MainFunctionParms *parms, char *buffer, unsigned int bufferSize) {

  if (buffer == NULL || bufferSize == 0 || parms == NULL) {
    return NULL;
  }

  buffer[0] = '\0';

  unsigned int parmNameLength = strlen(parmName);
  for (int startIdx = 0; startIdx < parms->textLength; startIdx++) {
    int endIdx;
    for (endIdx = startIdx; endIdx < parms->textLength; endIdx++) {
      if (parms->text[endIdx] == ',' || parms->text[endIdx] == ' ') {
        break;
      }
    }
    unsigned int tokenLength = endIdx - startIdx;
    if (tokenLength > parmNameLength + 1) {
      if (memcmp(parmName, &parms->text[startIdx], parmNameLength) == 0 &&
          parms->text[startIdx + parmNameLength] == '=') {
        snprintf(buffer, bufferSize, "%.*s", tokenLength - parmNameLength - 1, &parms->text[startIdx + parmNameLength + 1]);
        return buffer;
      }
    }
    startIdx = endIdx;
  }

  return NULL;
}

int main() {

  const MainFunctionParms * __ptr32 passedParms = GET_MAIN_FUNCION_PARMS();

  int status = RC_ZIS_OK;

  STCBase *base = (STCBase *)safeMalloc31(sizeof(STCBase), "stcbase");
  stcBaseInit(base);
  {

    cmsInitializeLogging();

    wtoPrintf(ZIS_LOG_STARTUP_MSG);
    zowelog(NULL, LOG_COMP_STCBASE, ZOWE_LOG_INFO, ZIS_LOG_STARTUP_MSG);
    /* TODO refactor consolidation of passed and parmlib parms */
    bool isColdStart = isKeywordInPassedParms(ZIS_PARM_COLD_START, passedParms);
    bool isDebug = isKeywordInPassedParms(ZIS_PARM_DEBUG_MODE, passedParms);
    if (passedParms != NULL && passedParms->textLength > 0) {
      zowelog(NULL, LOG_COMP_STCBASE, ZOWE_LOG_INFO,
              ZIS_LOG_INPUT_PARM_MSG, passedParms);
      zowedump(NULL, LOG_COMP_STCBASE, ZOWE_LOG_INFO,
               (void *)passedParms,
               sizeof(MainFunctionParms) + passedParms->textLength);
    }

    unsigned int serverFlags = CMS_SERVER_FLAG_NONE;
    if (isColdStart) {
      serverFlags |= CMS_SERVER_FLAG_COLD_START;
    }
    if (isDebug) {
      serverFlags |= CMS_SERVER_FLAG_DEBUG;
    }

    ZISParmSet *parmSet = zisMakeParmSet();
    if (parmSet == NULL) {
      zowelog(NULL, LOG_COMP_STCBASE, ZOWE_LOG_SEVERE,
              ZIS_LOG_CXMS_RES_ALLOC_FAILED_MSG, "parameter set");
      status = RC_ZIS_ERROR;
    }

    /* find out if MEM has been specified in the JCL and use it to create
     * the parmlib member name */
    char parmlibMember[ZIS_PARM_MEMBER_NAME_MAX_SIZE + 1] = {0};
    if (status == RC_ZIS_OK) {

      /* max suffix length + null-terminator + additional byte to determine
       * whether the passed value is too big */
      char parmlibMemberSuffixBuffer[ZIS_PARM_MEMBER_SUFFIX_SIZE + 1 + 1];
      const char *parmlibMemberSuffix = getPassedParmValue(
          ZIS_PARM_MEMBER_SUFFIX, passedParms,
          parmlibMemberSuffixBuffer,
          sizeof(parmlibMemberSuffixBuffer)
      );
      if (parmlibMemberSuffix == NULL) {
        parmlibMemberSuffix = ZIS_PARM_MEMBER_DEFAULT_SUFFIX;
      }

      if (strlen(parmlibMemberSuffix) == ZIS_PARM_MEMBER_SUFFIX_SIZE) {
        strcat(parmlibMember, ZIS_PARM_MEMBER_PREFIX);
        strcat(parmlibMember, parmlibMemberSuffix);
      } else {
        zowelog(NULL, LOG_COMP_STCBASE, ZOWE_LOG_SEVERE,
                ZIS_LOG_CXMS_BAD_PMEM_SUFFIX_MSG, parmlibMemberSuffix);
        status = RC_ZIS_ERROR;
      }
    }

    if (status == RC_ZIS_OK) {
      ZISParmStatus readStatus = {0};
      int readParmRC = zisReadParmlib(parmSet, ZIS_PARM_DDNAME,
                                      parmlibMember, &readStatus);
      if (readParmRC != RC_ZISPARM_OK) {
        if (readParmRC == RC_ZISPARM_MEMBER_NOT_FOUND) {
          zowelog(NULL, LOG_COMP_STCBASE, ZOWE_LOG_WARNING,
                  ZIS_LOG_CXMS_CFG_MISSING_MSG, parmlibMember, readParmRC);
          status = RC_ZIS_ERROR;
        } else {
          zowelog(NULL, LOG_COMP_STCBASE, ZOWE_LOG_SEVERE,
                  ZIS_LOG_CXMS_CFG_NOT_READ_MSG, parmlibMember, readParmRC,
                  readStatus.internalRC, readStatus.internalRSN);
          status = RC_ZIS_ERROR;
        }
      }
    }

    CrossMemoryServerName serverName;
    char serverNameBuffer[sizeof(serverName.nameSpacePadded) + 1];
    const char *serverNameNullTerm = NULL;
    serverNameNullTerm = getPassedParmValue(ZIS_PARM_SERVER_NAME, passedParms,
                                            serverNameBuffer,
                                            sizeof(serverNameBuffer));
    if (serverNameNullTerm == NULL) {
      serverNameNullTerm =
          zisGetParmValue(parmSet, ZIS_PARMLIB_PARM_SERVER_NAME);
    }

    if (serverNameNullTerm != NULL) {
      serverName = cmsMakeServerName(serverNameNullTerm);
      zowelog(NULL, LOG_COMP_STCBASE, ZOWE_LOG_INFO,
              ZIS_LOG_CXMS_NAME_MSG, serverName.nameSpacePadded);
    } else {
      serverName = CMS_DEFAULT_SERVER_NAME;
      zowelog(NULL, LOG_COMP_STCBASE, ZOWE_LOG_INFO, ZIS_LOG_CXMS_NO_NAME_MSG, serverName.nameSpacePadded);
    }

    int makeServerRSN = 0;
    CrossMemoryServer *server = makeCrossMemoryServer(base, &serverName, serverFlags, &makeServerRSN);
    if (server == NULL) {
      zowelog(NULL, LOG_COMP_STCBASE, ZOWE_LOG_SEVERE, ZIS_LOG_CXMS_NOT_CREATED_MSG, makeServerRSN);
      status = RC_ZIS_ERROR;
    }

    if (status == RC_ZIS_OK) {
      int loadParmRSN = 0;
      int loadParmRC = zisLoadParmsToServer(server, parmSet, &loadParmRSN);
      if (loadParmRC != RC_ZISPARM_OK) {
        zowelog(NULL, LOG_COMP_STCBASE, ZOWE_LOG_SEVERE,
                ZIS_LOG_CXMS_CFG_NOT_LOADED_MSG, loadParmRC, loadParmRSN);
        status = RC_ZIS_ERROR;
      }
    }

    if (status == RC_ZIS_OK) {
      cmsRegisterService(server, ZIS_SERVICE_ID_AUTH_SRV, authServiceFunction, NULL, CMS_SERVICE_FLAG_RELOCATE_TO_COMMON);
      cmsRegisterService(server, ZIS_SERVICE_ID_SNARFER_SRV, snarferServiceFunction, NULL, CMS_SERVICE_FLAG_SPACE_SWITCH | CMS_SERVICE_FLAG_RELOCATE_TO_COMMON);
      cmsRegisterService(server, ZIS_SERVICE_ID_NWM_SRV, nwmServiceFunction, NULL, CMS_SERVICE_FLAG_RELOCATE_TO_COMMON);
      int startRC = cmsStartMainLoop(server);
      if (startRC != RC_CMS_OK) {
        zowelog(NULL, LOG_COMP_STCBASE, ZOWE_LOG_SEVERE, ZIS_LOG_CXMS_NOT_STARTED_MSG, startRC);
        status = RC_ZIS_ERROR;
      }
    }

    if (server != NULL) {
      removeCrossMemoryServer(server);
      server = NULL;
    }

    if (parmSet != NULL) {
      zisRemoveParmSet(parmSet);
      parmSet = NULL;
    }

    if (status == RC_ZIS_OK) {
      wtoPrintf(ZIS_LOG_CXMS_TERM_OK_MSG);
      zowelog(NULL, LOG_COMP_STCBASE, ZOWE_LOG_INFO, ZIS_LOG_CXMS_TERM_OK_MSG);
    } else {
      wtoPrintf(ZIS_LOG_CXMS_TERM_FAILURE_MSG, status);
      zowelog(NULL, LOG_COMP_STCBASE, ZOWE_LOG_SEVERE, ZIS_LOG_CXMS_TERM_FAILURE_MSG, status);
    }

  }
  stcBaseTerm(base);
  safeFree31((char *)base, sizeof(STCBase));
  base = NULL;

  return status;
}
#define cmsDSECTs SRVDSECT
void cmsDSECTs() {

  __asm(

      "         DS    0D                                                       \n"
      "SNFTBUF  DSECT ,                                                        \n"
      "SNFTBEYE DS    CL8                                                      \n"
      "SNFTBSZ  DS    F                                                        \n"
      "SNFTBSRK DS    F                                                        \n"
      "SNFTBSRA DS    AD                                                       \n"
#ifdef ZIS_SNARFER_USE_SHARED_MEM_OBJ
      "SNFTBDNA DS    AD                                                       \n"
      "SNFTBSCT DS    D                                                        \n"
      "SNFTBTKN DS    D                                                        \n"
      "SNFTBRLS DS    2D                                                       \n"
      "SNFTBRLA DS    AD                                                       \n"
      "SNFTBRC4 DS    F                                                        \n"
      "SNFTBRS4 DS    F                                                        \n"
      "SNFTBPL4 DS    CL128                                                    \n"
#else
      "SNFTBDNA DS    0D                                                       \n"
#endif /* ZIS_SNARFER_USE_SHARED_MEM_OBJ */
      "SNFTBUFL EQU   *-SNFTBUF                                                \n"
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

