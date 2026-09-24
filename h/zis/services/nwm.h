

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/

#ifndef ZIS_SERVICES_NWM_H_
#define ZIS_SERVICES_NWM_H_

#include "zowetypes.h"

#define ZIS_SERVICE_ID_NWM_SRV                    13

#define ZIS_SERVICE_SAF_PN_NWM_SRV CMS_PROD_ID".IS.SRV.NWM"
#define ZIS_SERVICE_SAF_AL_NWM_SRV CMS_SAF_ACCESS_LEVEL_READ

#define ZIS_SERVICE_NWM_PARM_SAF CMS_PROD_ID".SRV.NWM.SAF"
  #define ZIS_SERVICE_NWM_PARM_VALUE_SAF_ON "ON"
  #define ZIS_SERVICE_NWM_PARM_VALUE_SAF_OFF "OFF"

ZOWE_PRAGMA_PACK
typedef struct ZISNWMServiceParmList_tag {

  /* input fields */
  char eyecatcher[8];
#define ZIS_NWM_SERVICE_PARMLIST_EYECATCHER "RSZISNWM"
  int version;
  int flags;
  int traceLevel;

  /* output fields */
  char nmiJobName[8];
  PAD_LONG(0, char *nmiBuffer);
  unsigned int nmiBufferSize;

  /* output fields */
  int nmiReturnValue;
  int nmiReturnCode;
  int nmiReasonCode;

} ZISNWMServiceParmList;
ZOWE_PRAGMA_PACK_RESET

#pragma map(zisNWMServiceFunction, "ZISNWMSF")
int zisNWMServiceFunction(CrossMemoryServerGlobalArea *globalArea,
                          CrossMemoryService *service,
                          void *parm);

struct ZISParmSet_tag;

#pragma map(zisNWMServiceGetServiceData, "ZISDNWMS")
void *zisNWMServiceGetServiceData(const struct ZISParmSet_tag *parms);

#define RC_ZIS_NWMSRV_OK                         0
#define RC_ZIS_NWMSRV_NO_ACCESS                  4
#define RC_ZIS_NWMSRV_PARMLIST_NULL              8
#define RC_ZIS_NWMSRV_BAD_EYECATCHER             9
#define RC_ZIS_NWMSRV_BUFFER_NULL                10
#define RC_ZIS_NWMSRV_BUFFER_NOT_ALLOCATED       11
#define RC_ZIS_NWMSRV_RECOVERY_ERROR             12
#define RC_ZIS_NWMSRV_NWM_ABENDED                13
#define RC_ZIS_NWMSRV_NWM_FAILED                 14

#endif /* ZIS_SERVICES_NWM_H_ */

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/

