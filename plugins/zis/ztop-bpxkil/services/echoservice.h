
/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/

#ifndef ECHOSERVICE_H_
#define ECHOSERVICE_H_

typedef struct EchoServiceParmList_tag {
  char eyecatcher[8];
#define ECHO_SERVICE_PARMLIST_EYECATCHER "COMRSZTOP"
  int pidInt;
  int signalInt;
} EchoServiceParmList;


int serveKill(const CrossMemoryServerGlobalArea *ga,
                               struct ZISServiceAnchor_tag *anchor,
                               struct ZISServiceData_tag *data,
                               void *serviceParmList);

#define RC_ECHOSVC_OK       0
#define RC_ECHOSVC_ERROR    8

#endif /* ECHOSERVICE_H_ */


/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/


