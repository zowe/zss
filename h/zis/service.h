

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/

#ifndef ZIS_SERVICE_H_
#define ZIS_SERVICE_H_

#include "zowetypes.h"
#include "crossmemory.h"

struct ZISContext_tag;
typedef struct ZISService_tag ZISService;
typedef struct ZISServiceAnchor_tag ZISServiceAnchor;
typedef struct ZISServiceData_tag ZISServiceData;

typedef int (ZISServiceInitFunction)(struct ZISContext_tag *context,
                                     ZISService *service,
                                     ZISServiceAnchor *anchor);
typedef int (ZISServiceTermFunction)(struct ZISContext_tag *context,
                                     ZISService *service,
                                     ZISServiceAnchor *anchor);
typedef int (ZISServiceModifyCommandFunction)(struct ZISContext_tag *context,
                                              ZISService *service,
                                              ZISServiceAnchor *anchor,
                                              const CMSModifyCommand *command,
                                              CMSModifyCommandStatus *status);
typedef int (ZISServiceServeFunction)(const CrossMemoryServerGlobalArea *ga,
                                      ZISServiceAnchor *anchor,
                                      ZISServiceData *data,
                                      void *serviceParmList);

ZOWE_PRAGMA_PACK

typedef struct ZISServiceName_tag {
  char text[16];
} ZISServiceName;

typedef struct ZISServicePath_tag {
  char pluginName[16];
  ZISServiceName serviceName;
} ZISServicePath;

struct ZISServiceData_tag {
  char data[64];
};

struct ZISServiceAnchor_tag {

  char eyecatcher[8];
#define ZIS_SERVICE_ANCHOR_EYECATCHER "ZISSVCAN"
  int version;
#define ZIS_SERVICE_ANCHOR_VERSION 1
  int key : 8;
  unsigned int subpool : 8;
  unsigned short size;
  int flags;
#define ZIS_SERVICE_ANCHOR_FLAG_SPACE_SWITCH 0x00000001
  int state;
#define ZIS_SERVICE_ANCHOR_STATE_ACTIVE 0x00000001

  ZISServicePath path;
  PAD_LONG(0, struct ZISServiceAnchor_tag *next);

  PAD_LONG(1, struct ZISPluginAnchor_tag *pluginAnchor);

  PAD_LONG(2, ZISServiceServeFunction *serve);

  ZISServiceData serviceData;

};

struct ZISService_tag {

  char eyecatcher[8];
#define ZIS_SERVICE_EYECATCHER "ZISSRVCE"
  int version;
#define ZIS_SERVICE_VERSION 1
  int flags;
#define ZIS_SERVICE_FLAG_NONE           0x00000000
#define ZIS_SERVICE_FLAG_SPACE_SWITCH   0x00000001

  PAD_LONG(0, ZISServiceAnchor *anchor);

  ZISServiceName name;

  PAD_LONG(1, ZISServiceInitFunction *init);
  PAD_LONG(2, ZISServiceTermFunction *term);
  PAD_LONG(4, ZISServiceServeFunction *serve);

  char reserved[440];

};

ZOWE_PRAGMA_PACK_RESET

#pragma map(zisCreateService, "ZISCRSVC")
#pragma map(zisCreateSpaceSwitchService, "ZISCSWSV")
#pragma map(zisCreateCurrentPrimaryService, "ZISCCPSVC")
#pragma map(zisCreateServiceAnchor, "ZISSERVCA")
#pragma map(zisRemoveServiceAnchor, "ZISSERVRA")
#pragma map(zisUpdateServiceAnchor, "ZISSERVUA")

ZISService zisCreateService(ZISServiceName name, int flags,
                            ZISServiceInitFunction *initFunction,
                            ZISServiceTermFunction *termFunction,
                            ZISServiceServeFunction *serveFunction);

ZISService zisCreateSpaceSwitchService(
    ZISServiceName name,
    ZISServiceInitFunction *initFunction,
    ZISServiceTermFunction *termFunction,
    ZISServiceServeFunction *serveFunction
);

ZISService zisCreateCurrentPrimaryService(
    ZISServiceName name,
    ZISServiceInitFunction *initFunction,
    ZISServiceTermFunction *termFunction,
    ZISServiceServeFunction *serveFunction
);

struct ZISPlugin_tag;

ZISServiceAnchor *zisCreateServiceAnchor(const struct ZISPlugin_tag *plugin,
                                         const ZISService *service);
void zisRemoveServiceAnchor(ZISServiceAnchor **anchor);
void zisUpdateServiceAnchor(ZISServiceAnchor *anchor,
                            const struct ZISPlugin_tag *plugin,
                            const ZISService *service);

#define RC_ZIS_SRVC_OK                  0
#define RC_ZIS_SRVC_STATUS_NULL         2
#define RC_ZIS_SRVC_SERVICE_FAILED      4
#define RC_ZIS_SRVC_CMS_FAILED          8
#define RC_ZIS_SRVC_GLOBAL_AREA_NULL    9
#define RC_ZIS_SRVC_SEVER_ANCHOR_NULL   10
#define RC_ZIS_SRVC_SERVICE_TABLE_NULL  11
#define RC_ZIS_SRVC_SERVICE_NOT_FOUND   12
#define RC_ZIS_SRVC_SERVICE_INACTIVE    13

#define ZIS_MAX_GEN_SRVC_RC             32

#endif /* ZIS_SERVICE_H_ */


/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/

