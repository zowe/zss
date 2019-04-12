

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
#include "alloc.h"
#include "crossmemory.h"
#include "recovery.h"
#include "zos.h"

#include "zis/utils.h"
#include "zis/services/snarfer.h"

typedef struct SToken_tag {
  char token[8];
} SToken;

__asm("SCHGLBPL IEAMSCHD MF=(L,SCHGLBPL)" : "DS"(SCHGLBPL));

static int scheduleSynchSRB(Addr31 entryPoint,
                            Addr31 parm,
                            SToken token,
                            Addr31 frrAddress,
                            int * __ptr32 compCode,
                            int * __ptr32 srbRC,
                            int * __ptr32 srbRSN) {

  __asm("SCHGLBPL IEAMSCHD MF=(L,SCHGLBPL)" : "DS"(parmList));
  parmList = SCHGLBPL;

  int returnCode = 0;

  unsigned int localEP = ((int)entryPoint) | 0x80000000;
  unsigned int localParm = (int)parm;
  unsigned int localFRRAddress =
      (frrAddress != NULL) ? (((int)frrAddress) | 0x80000000) : 0;
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
      : "r"(&localEP), "r"(&localParm), "r"(&localSToken),
        "r"(&localFRRAddress), "r"(&parmList)
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

ZOWE_PRAGMA_PACK
typedef struct TransferBuffer_tag {
  char eyecatcher[8];
#define TRANSFER_BUFFER_EYECATCHER  "RSTRNBUF"

  unsigned int copySize;
  unsigned int sourceKey;
  uint64 sourceAddress;
  uint64 destinationAddress;
  char data[0];

} TransferBuffer;
ZOWE_PRAGMA_PACK_RESET

static int allocateTransferBuffer(CrossMemoryServerGlobalArea *globalArea,
                                  unsigned int copySize,
                                  TransferBuffer **buffer,
                                  int traceLevel) {

  unsigned int commonStorageSize = sizeof(TransferBuffer) + copySize;

  cmsAllocateECSAStorage2(globalArea, commonStorageSize, (void **)buffer);
  TransferBuffer *transferBuffer = *buffer;
  if (transferBuffer == NULL) {
    return RC_ZIS_SNRFSRV_ECSA_ALLOC_FAILED;
  }
  memset(transferBuffer, 0, sizeof(TransferBuffer));
  memcpy(transferBuffer->eyecatcher, TRANSFER_BUFFER_EYECATCHER,
         sizeof(transferBuffer->eyecatcher));
  transferBuffer->copySize = copySize;

  if (traceLevel >= ZOWE_LOG_DEBUG) {
    cmsPrintf(&globalArea->serverName,
              CMS_LOG_DEBUG_MSG_ID" allocateTransferBuffer: transferBuffer=0x%p\n",
              transferBuffer);
  }

  transferBuffer->destinationAddress = (uint64)&transferBuffer->data;

  return RC_ZIS_SNRFSRV_OK;
}

static int freeTransferBuffer(CrossMemoryServerGlobalArea *globalArea,
                              TransferBuffer **buffer, int traceLevel) {

  int status = RC_ZIS_SNRFSRV_OK;

  unsigned int commonStorageSize = sizeof(TransferBuffer) + (*buffer)->copySize;

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

int zisSnarferServiceFunction(CrossMemoryServerGlobalArea *globalArea,
                              CrossMemoryService *service, void *parm) {

  int logLevel = globalArea->pcLogLevel;

  void *clientParmAddr = parm;
  if (clientParmAddr == NULL) {
    return RC_ZIS_SNRFSRV_PARMLIST_NULL;
  }

  SnarferServiceParmList localParmList;
  cmCopyFromSecondaryWithCallerKey(&localParmList, clientParmAddr,
                                   sizeof(localParmList));

  if (memcmp(localParmList.eyecatcher, ZIS_SNARFER_SERVICE_PARMLIST_EYECATCHER,
             sizeof(localParmList.eyecatcher))) {
    return RC_ZIS_SNRFSRV_BAD_EYECATCHER;
  }

  void *dest = (void *)localParmList.destinationAddress;
  void *src = (void *)localParmList.sourceAddress;
  ASCB *srcASCB = localParmList.sourceASCB;
  int srcKey = localParmList.sourceKey;
  unsigned int copySize = localParmList.size;

  if (logLevel >= ZOWE_LOG_DEBUG) {
    cmsPrintf(&globalArea->serverName,
              CMS_LOG_DEBUG_MSG_ID" snarferServiceFunction: "
              "dest=0x%p, src=0x%p, srcASCB=0x%p, srcKey=0x%X, copySize=%u\n",
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
        cmsPrintf(&globalArea->serverName,
                  CMS_LOG_DEBUG_MSG_ID" srbRoutine=0x%p, stoken=%llX\n",
                  srbRoutine, *(uint64 *)&stoken);
      }

      int compCode = 0, srbRC = 0, srbRSN = 0;
      int ieamschdRC = scheduleSynchSRB(srbRoutine, cntx.transferBuffer, stoken,
                                        NULL, &compCode, &srbRC, &srbRSN);
      if (logLevel >= ZOWE_LOG_DEBUG) {
        cmsPrintf(
            &globalArea->serverName, CMS_LOG_DEBUG_MSG_ID
            " ieamschdRC=0x%08X, compCode=0x%08X, srbRC=0x%08X, srbRSN=0x%08X\n",
            ieamschdRC, compCode, srbRC, srbRSN
        );
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
    cmCopyToSecondaryWithCallerKey(
        (void *)localParmList.destinationAddress,
        (void *)cntx.transferBuffer->destinationAddress,
        localParmList.size
    );
  }

  if (cntx.transferBuffer != NULL) {
    int freeTransferBufRC = freeTransferBuffer(globalArea, &cntx.transferBuffer,
                                               logLevel);
    if (freeTransferBufRC != RC_ZIS_SNRFSRV_OK) {
      status = (status != RC_ZIS_SNRFSRV_OK) ? status : freeTransferBufRC;
    }
  }

  recoveryPop(); /* global snarfer recovery */

  return status;
}

#define snarferDSECTs SNFDSECT
void snarferDSECTs() {

  __asm(

      "         DS    0D                                                       \n"
      "SNFTBUF  DSECT ,                                                        \n"
      "SNFTBEYE DS    CL8                                                      \n"
      "SNFTBSZ  DS    F                                                        \n"
      "SNFTBSRK DS    F                                                        \n"
      "SNFTBSRA DS    AD                                                       \n"
      "SNFTBDNA DS    0D                                                       \n"
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

