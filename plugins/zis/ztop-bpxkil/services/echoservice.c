

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
                                   
  int pid = localParmList.pidInt;
  int sig = localParmList.signalInt;
  if (pid <= 1) {
    cmsPrintf(&ga->serverName, "error: pad pid %d\n", pid);
    return RC_ECHOSVC_ERROR;
  }
  if (sig < 1) {
    cmsPrintf(&ga->serverName, "error: pad signal %d\n", sig);
    return RC_ECHOSVC_ERROR;
  }

  int returnValue = 0;
  int returnCode = 0;
  int reasonCode = 0;
  BPXKIL(pid,
         sig,
         0,
         &returnValue,
         &returnCode,
         &reasonCode);
  cmsPrintf(&ga->serverName, "info: BPXKIL pid=%d, signal=%d, RV=%d, RC=%d, RSN=0x%08X\n", pid, sig, returnValue, returnCode, reasonCode);
  if (returnValue) {
    cmsPrintf(&ga->serverName, "error: BPX cal failed\n");
    return RC_ECHOSVC_ERROR;
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

