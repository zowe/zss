

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "zowetypes.h"
#include "alloc.h"
#include "qsam.h"
#include "utils.h"
#include "zos.h"

#include "zis/client.h"
#include "zis/service.h"

#include "services/echoservice.h"

#ifndef __ZOWE_OS_ZOS
#error z/OS targets are supported only
#endif

#define RC_OK     0
#define RC_ERROR  8

 
static bool isPositiveNum(const char *str) {
  const char *currChar = str;
  while (*currChar != '\0') {
    if (!isdigit(*currChar)) {
      return false;
    }
    currChar++;
  }
  return true;
}


 int main(int argc, char **argv) {
 
  if (argc < 3) {
    printf("error: bad args\n");
    printf("info: use > test <zis_name> <pid> <signal>\n");
    return RC_ERROR;
  }

  char *serverNameArg = argv[1];
  char *pidStr = argv[2];
  char *signalStr = argv[3];
  if (!isPositiveNum(pidStr)) {
    printf("error: bad pid arg\n");
    return RC_ERROR;
  }
  if (!isPositiveNum(signalStr)) {
    printf("error: bad signal arg\n");
    return RC_ERROR;
  }
  int pid = atoi(pidStr);
  int sig = atoi(signalStr);

  printf("info: server name = '%s', pid = %d, signal = %d'\n", serverNameArg, pid, sig);

  if (pid <= 1) {
    printf("error: pid too low, use pids > 1\n");
    return RC_ERROR;
  }
  if (sig < 1) {
    printf("error: bad signal value, use signals >= 1\n");
    return RC_ERROR;
  }

  CrossMemoryServerName serverName = cmsMakeServerName(serverNameArg);

  EchoServiceParmList parmlist = {
      .eyecatcher = ECHO_SERVICE_PARMLIST_EYECATCHER,
      .pidInt = pid,
      .signalInt = sig,
  };

  ZISServiceStatus status = {0};

  int rc = RC_ZIS_SRVC_OK;

  ZISServicePath path1 = {
      .pluginName = "ZTOP_ZIS        ",
      .serviceName.text = "ZTOP_KILL       ",
  };

  rc = zisCallService(&serverName, &path1, &parmlist, &status);
  if (rc != RC_ZIS_SRVC_OK) {
    printf("error: cross-memory rc = %d, status:\n", rc);
    dumpbuffer((char *)&status, sizeof(status));
    return RC_ERROR;
  }
  printf("info: the test has completed successfully\n");

   return RC_OK;
 }


/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/


