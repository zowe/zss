

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

#include "zowetypes.h"
#include "cmutils.h"
#include "crossmemory.h"
#include "zis/plugin.h"
#include "zis/service.h"

#include "services/echoservice.h"

int serveEchoedMessage(const CrossMemoryServerGlobalArea *ga,
                       struct ZISServiceAnchor_tag *anchor,
                       struct ZISServiceData_tag *data,
                       void *serviceParmList) {

  EchoServiceParmList localParmList;
  cmCopyFromSecondaryWithCallerKey(&localParmList, serviceParmList,
                                   sizeof(localParmList));

  char echoedMessage[256] = {0};
  sprintf(echoedMessage, "ECHO: %s\n", localParmList.nullTermMessage);

  cmsPrintf(&ga->serverName, echoedMessage);

  return RC_ECHOSVC_OK;
}

static void reverseString(char *s) {
  size_t length = strlen(s);
  for (size_t i = 0; i < length / 2; i++) {
    char tmp = s[i];
    s[i] = s[length - 1 - i];
    s[length - 1 - i] = tmp;
  }
}

int serveReversedEchoedMessage(const CrossMemoryServerGlobalArea *ga,
                               struct ZISServiceAnchor_tag *anchor,
                               struct ZISServiceData_tag *data,
                               void *serviceParmList) {

  EchoServiceParmList localParmList;
  cmCopyFromSecondaryWithCallerKey(&localParmList, serviceParmList,
                                   sizeof(localParmList));

  char echoedMessage[256] = {0};
  reverseString(localParmList.nullTermMessage);
  sprintf(echoedMessage, "ECHO: %s\n", localParmList.nullTermMessage);

  cmsPrintf(&ga->serverName, echoedMessage);

  return RC_ECHOSVC_OK;
}



/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/

