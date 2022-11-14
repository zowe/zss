/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/

#include <stdio.h>
#include <stdlib.h>

#include "zis/client.h"

#include "sample_plugin.h"

int main(int argc, char **argv) {

  if (argc < 2) {
    printf("error: too few parameters\n");
    printf("usage: > sample_client <zis_name>\n");
    exit(EXIT_FAILURE);
  }

  GetCRServiceParm parmList = {
      .eyecatcher = GETCR_SERVICE_PARM_EYECATCHER,
      .version = GETCR_SERVICE_PARM_VERSION,
  };

  CrossMemoryServerName xmemName = cmsMakeServerName(argv[1]);
  ZISServicePath servicePath = {
      .pluginName = SAMPLE_PLUGIN_NAME,
      .serviceName = GETCR_SERVICE_NAME,
  };
  ZISServiceStatus status;

  int callRC = zisCallService(&xmemName, &servicePath, &parmList, &status);
  if (callRC == RC_GETCR_OK) {
    printf("info: successfully called the \'%s\' service:\n",
           GETCR_SERVICE_NAME);
    size_t crCount = sizeof(parmList.result.reg) / sizeof(parmList.result.reg[0]);
    for (size_t regIdx = 0; regIdx < crCount; regIdx++) {
      printf("  CR%-2zu = 0x%016llX\n", regIdx, parmList.result.reg[regIdx]);
    }
    exit(EXIT_SUCCESS);
  } else {
    printf("error: service call failed - call RC = %d, "
           "service RC = %d, CMS RC = %d\n", callRC,
           status.serviceRC, status.cmsRC);
    exit(EXIT_FAILURE);
  }

}

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/