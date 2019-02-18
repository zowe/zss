

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

#include "services/getservice.h"

typedef struct ServiceData_tag {
  int magicNumber;
} ServiceData;

#define MAGIC_NUMBER 5587

int initMagicNumberService(struct ZISContext_tag *context,
                           ZISService *service,
                           ZISServiceAnchor *anchor) {

  ServiceData *serviceData = (ServiceData *)&anchor->serviceData;

  serviceData->magicNumber += MAGIC_NUMBER;

  return 0;
}

int termMagicNumberService(struct ZISContext_tag *context,
                           ZISService *service,
                           ZISServiceAnchor *anchor) {

  ServiceData *serviceData = (ServiceData *)&anchor->serviceData;

  serviceData->magicNumber -= MAGIC_NUMBER;

  return 0;
}

int serveMagicNumber(const CrossMemoryServerGlobalArea *ga,
                     struct ZISServiceAnchor_tag *anchor,
                     struct ZISServiceData_tag *data,
                     void *serviceParmList) {

  GetMNumServiceParmList localParmList;
  cmCopyFromSecondaryWithCallerKey(&localParmList, serviceParmList,
                                   sizeof(localParmList));

  ServiceData *serviceData = (ServiceData *)data;

  localParmList.magicNumber = serviceData->magicNumber;

  cmCopyToSecondaryWithCallerKey(serviceParmList, &localParmList,
                                 sizeof(localParmList));

  return RC_GETMNUMSVC_OK;
}

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
 */

