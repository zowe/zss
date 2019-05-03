

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

#include "services/getservice.h"

#ifndef __ZOWE_OS_ZOS
#error z/OS targets are supported only
#endif

#define RC_OK     0
#define RC_ERROR  8

int main(int argc, char **argv) {

  if (argc < 2) {
    printf("error: provide server name\n");
    return RC_ERROR;
  }

  char *serverNameArg = argv[1];
  printf("info: server name = '%s'\n", serverNameArg);

  CrossMemoryServerName serverName = cmsMakeServerName(serverNameArg);

  GetMNumServiceParmList parmlist = {
      .eyecatcher = GET_SERVICE_PARMLIST_EYECATCHER,
  };

  ZISServiceStatus status = {0};

  int rc = RC_ZIS_SRVC_OK;

  ZISServicePath path = {
      .pluginName = "MAGICNUMBER     ",
      .serviceName.text = "GET             ",
  };

  rc = zisCallService(&serverName, &path, &parmlist, &status);
  if (rc == RC_ZIS_SRVC_OK) {
    if (parmlist.magicNumber == 5587) {
      printf("info: magic data = %d\n", parmlist.magicNumber);
    } else {
      printf("info: wrong magic number %d\n", parmlist.magicNumber);
    }
  } else {
    printf("error: rc = %d, status:\n", rc);
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

