

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/

#ifndef ZIS_SERVICES_SNARFER_H_
#define ZIS_SERVICES_SNARFER_H_

#define ZIS_SERVICE_ID_SNARFER_SRV                12

ZOWE_PRAGMA_PACK
typedef struct SnarferServiceParmList_tag {
  char eyecatcher[8];
#define ZIS_SNARFER_SERVICE_PARMLIST_EYECATCHER "RSSNRFSR"
  uint64 destinationAddress;
  uint64 sourceAddress;
  ASCB * __ptr32 sourceASCB;
  unsigned int sourceKey;
  unsigned int size;
} SnarferServiceParmList;
ZOWE_PRAGMA_PACK_RESET

#pragma map(zisSnarferServiceFunction, "ZISSSNRF")
int zisSnarferServiceFunction(CrossMemoryServerGlobalArea *globalArea,
                              CrossMemoryService *service, void *parm);

#define RC_ZIS_SNRFSRV_OK                         0
#define RC_ZIS_SNRFSRV_PARMLIST_NULL              8
#define RC_ZIS_SNRFSRV_BAD_EYECATCHER             9
#define RC_ZIS_SNRFSRV_BAD_DEST                   10
#define RC_ZIS_SNRFSRV_BAD_ASCB                   11
#define RC_ZIS_SNRFSRV_BAD_SIZE                   12
#define RC_ZIS_SNRFSRV_ECSA_ALLOC_FAILED          13
#define RC_ZIS_SNRFSRV_RECOVERY_ERROR             14
#define RC_ZIS_SNRFSRV_SRC_ASSB_ABEND             15
#define RC_ZIS_SNRFSRV_SRC_IEAMSCHD_ABEND         16
#define RC_ZIS_SNRFSRV_SRC_IEAMSCHD_FAILED        17
#define RC_ZIS_SNRFSRV_SHARED_OBJ_ALLOC_FAILED    18
#define RC_ZIS_SNRFSRV_SHARED_OBJ_SHARE_FAILED    19
#define RC_ZIS_SNRFSRV_SHARED_OBJ_DETACH_FAILED   20

#endif /* ZIS_SERVICES_SNARFER_H_ */


/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/

