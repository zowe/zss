

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
#include "zos.h"

#include "zis/utils.h"
#include "zis/services/nwm.h"

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

int zisNWMServiceFunction(CrossMemoryServerGlobalArea *globalArea,
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


/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/

