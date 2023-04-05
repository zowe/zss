/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

#ifndef ZIS_SERVER_H_
#define ZIS_SERVER_H_

#include "zowetypes.h"
#include "collections.h"
#include "crossmemory.h"
#include "zos.h"

#include "zis/parm.h"
#include "zis/service.h"

ZOWE_PRAGMA_PACK

typedef struct ZISContext_tag {
  char eyecatcher[8];
#define ZIS_CONTEXT_EYECATCHER "ZISCNTXT"
  struct STCBase_tag *stcBase;
  ZISParmSet *parms;
  CrossMemoryServer *cmServer;
  CrossMemoryServerGlobalArea *cmsGA;
  struct ZISServerAnchor_tag *zisAnchor;
  struct ZISPlugin_tag *firstPlugin;
  int cmsFlags;
  EightCharString zisModuleName;
  char dynlinkModuleNameNullTerm[9];
  char reserved0[3];

  int16_t version;
#define ZIS_CONTEXT_VERSION 2
#define ZIS_CONTEXT_VERSION_ZIS_VERSION_SUPPORT 2
  uint16_t size;
  uint32_t flags;
  struct {
    int32_t major;
    int32_t minor;
    int32_t revision;
  } zisVersion;

} ZISContext;

#define ZIS_SERVER_ANCHOR_FLAG_DYNLINK 0x0001    /* supports dynamic linkage */
#define ZIS_SERVER_ANCHOR_VERSIONED_CONTEXT 0x0002

typedef struct ZISServerAnchor_tag {
  char eyecatcher[8];
#define ZIS_SERVER_ANCHOR_EYECATCHER "ZISSRVAN"
  int version;
#define ZIS_SERVER_ANCHOR_VERSION 1
  int key : 8;
  unsigned int subpool : 8;
  unsigned short size;
  int flags;

  char reservred0[4];

  PAD_LONG(0, struct CrossMemoryMap_tag *serviceTable);

  PAD_LONG(1, struct ZISPluginAnchor_tag *firstPlugin);

  char reservred2[24];

} ZISServerAnchor;

typedef _Packed struct ZISServiceRouterData_tag {
  char eyecatcher[8];
#define ZIS_SERVICE_ROUTER_EYECATCHER "ZISSREYE"
  unsigned int version;
#define ZIS_SERVICE_ROUTER_VERSION 1
  unsigned int size;
  char reserved0[16];
  ZISServicePath targetServicePath;
  PAD_LONG(0x00, void *targetServiceParm);
  unsigned int serviceVersion;
  char reserved1[52];
} ZISServiceRouterParm;

#define ZIS_SERVICE_ID_SRVC_ROUTER_SS 64
#define ZIS_SERVICE_ID_SRVC_ROUTER_CP 65

ZOWE_PRAGMA_PACK_RESET

#define RC_ZIS_OK       0
#define RC_ZIS_ERROR    8

#endif /* ZIS_SERVER_H_ */
