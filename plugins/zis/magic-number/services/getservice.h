

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/

#ifndef GETSERVICE_H_
#define GETSERVICE_H_

#include "zis/service.h"

typedef struct GetMNumServiceParmList_tag {
  char eyecatcher[8];
#define GET_SERVICE_PARMLIST_EYECATCHER "GETSRVC"
  int magicNumber;
} GetMNumServiceParmList;

int initMagicNumberService(struct ZISContext_tag *context,
                           ZISService *service,
                           ZISServiceAnchor *anchor);

int termMagicNumberService(struct ZISContext_tag *context,
                           ZISService *service,
                           ZISServiceAnchor *anchor);

int serveMagicNumber(const CrossMemoryServerGlobalArea *ga,
                     struct ZISServiceAnchor_tag *anchor,
                     struct ZISServiceData_tag *data,
                     void *serviceParmList);

#define RC_GETMNUMSVC_OK       0
#define RC_GETMNUMSVC_ERROR    8

#endif /* GETSERVICE_H_ */


/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/

