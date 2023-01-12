

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
#include <metal/string.h>
// #include <unistd.h>

#include "zowetypes.h"
#include "cmutils.h"
#include "crossmemory.h"
#include "zis/plugin.h"
#include "zis/service.h"

#include "services/echoservice.h"

#ifdef __ZOWE_OS_ZOS

#ifdef _LP64
#pragma linkage(BPX4KIL,OS)
#define BPXKIL BPX4KIL
#else
#pragma linkage(BPX1KIL,OS)
#define BPXKIL BPX1KIL
#endif

#else
//unsupported
#endif


int serveKill(const CrossMemoryServerGlobalArea *ga,
                               struct ZISServiceAnchor_tag *anchor,
                               struct ZISServiceData_tag *data,
                               void *serviceParmList) {

  EchoServiceParmList localParmList;
  cmCopyFromSecondaryWithCallerKey(&localParmList, serviceParmList,
                                   sizeof(localParmList));

  int returnValue = 0;
  int returnCode = 0;
  int reasonCode = 0;
  BPXKIL(localParmList.pidInt,
         localParmList.signalInt,
         0,
         &returnValue,
         &returnCode,
         &reasonCode);
  // char signalArg[40];
  // cmsPrintf(&ga->serverName, "ABOUT TO CALL KILL VIA SYSTEM FUNC");
  // sprintf(signalArg, "kill -%d %d", localParmList.signalInt, localParmList.pidInt);
  // returnValue = system(signalArg);
  if (returnValue) {
    char result[48];
    sprintf(result, "Unsuccessful call to BPXKIL, BPXKIL return value=%d, RC=%d, RSN=%d", returnValue, returnCode, reasonCode);
    cmsPrintf(&ga->serverName, result);
    return reasonCode;
  }
  cmsPrintf(&ga->serverName, "SUCCESS");
  return RC_ECHOSVC_OK;
}



/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/

