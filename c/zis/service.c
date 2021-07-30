

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/

#include <metal/metal.h>
#include <metal/stddef.h>
#include <metal/stdlib.h>
#include <metal/string.h>

#include "zowetypes.h"
#include "cmutils.h"
#include "crossmemory.h"
#include "zis/plugin.h"
#include "zis/service.h"

ZISService zisCreateService(ZISServiceName name, int flags,
                            ZISServiceInitFunction *initFunction,
                            ZISServiceTermFunction *termFunction,
                            ZISServiceServeFunction *serveFunction,
                            unsigned int version) {

  ZISService service = {
      .eyecatcher = ZIS_SERVICE_EYECATCHER,
      .version = ZIS_SERVICE_VERSION,
      .flags = flags,
      .name = name,
      .init = initFunction,
      .term = termFunction,
      .serve = serveFunction,
      .serviceVersion = version,
  };

  return service;
}

ZISService zisCreateSpaceSwitchService(
    ZISServiceName name,
    ZISServiceInitFunction *initFunction,
    ZISServiceTermFunction *termFunction,
    ZISServiceServeFunction *serveFunction,
    unsigned int version
) {

  return zisCreateService(name, ZIS_SERVICE_FLAG_SPACE_SWITCH,
                          initFunction, termFunction, serveFunction,
                          version);

}

int zisServiceUseSpecificAuth(ZISService *service, 
			      char *className,
			      char *entityName){
  int cLen = strlen(className);
  int eLen = strlen(entityName);
  if (cLen > 8){
    return 12;
  }
  if (eLen > 255){
    return 12;
  }
  memset(service->safClassName,' ',8);
  memcpy(service->safClassName,className,cLen);
  strcpy(service->safEntityName,entityName);
  service->flags |= ZIS_SERVICE_FLAG_SPECIFIC_AUTH;
}

ZISService zisCreateCurrentPrimaryService(
    ZISServiceName name,
    ZISServiceInitFunction *initFunction,
    ZISServiceTermFunction *termFunction,
    ZISServiceServeFunction *serveFunction,
    unsigned int version
) {

  return zisCreateService(name, ZIS_SERVICE_FLAG_NONE,
                          initFunction, termFunction, serveFunction,
                          version);


}

ZISServiceAnchor *zisCreateServiceAnchor(const struct ZISPlugin_tag *plugin,
                                         const ZISService *service) {

  unsigned int size = sizeof(ZISServiceAnchor);
  int subpool = CROSS_MEMORY_SERVER_SUBPOOL;
  int key = CROSS_MEMORY_SERVER_KEY;

  ZISServiceAnchor *anchor = cmAlloc(size, subpool, key);
  if (anchor == NULL) {
    return NULL;
  }

  memset(anchor, 0, size);
  memcpy(anchor->eyecatcher, ZIS_SERVICE_ANCHOR_EYECATCHER,
         sizeof(anchor->eyecatcher));
  anchor->version = ZIS_SERVICE_ANCHOR_VERSION;
  anchor->key = key;
  anchor->subpool = subpool;
  anchor->size = size;

  if (service->flags & ZIS_SERVICE_FLAG_SPACE_SWITCH) {
    anchor->flags |= ZIS_SERVICE_ANCHOR_FLAG_SPACE_SWITCH;
  }
  if (service->flags & ZIS_SERVICE_FLAG_SPECIFIC_AUTH) {
    anchor->flags |= ZIS_SERVICE_ANCHOR_FLAG_SPECIFIC_AUTH;
  }

  *(ZISPluginName *)anchor->path.pluginName = plugin->name;
  anchor->path.serviceName = service->name;

  anchor->pluginAnchor = plugin->anchor;
  anchor->serve = service->serve;
  anchor->serviceVersion = service->serviceVersion;
  memcpy(anchor->safClassName,service->safClassName,8);
  memcpy(anchor->safEntityName,service->safEntityName,256);

  return anchor;
}

void zisRemoveServiceAnchor(ZISServiceAnchor **anchor) {

  unsigned int size = sizeof(ZISServiceAnchor);
  int subpool = CROSS_MEMORY_SERVER_SUBPOOL;
  int key = CROSS_MEMORY_SERVER_KEY;

  cmFree2((void **)anchor, size, subpool, key);

}

void zisUpdateServiceAnchor(ZISServiceAnchor *anchor,
                            const struct ZISPlugin_tag *plugin,
                            const ZISService *service) {

  anchor->flags = service->flags;
  anchor->serve = service->serve;
  anchor->serviceVersion = service->serviceVersion;
  memcpy(anchor->safClassName,service->safClassName,8);
  memcpy(anchor->safEntityName,service->safEntityName,256);


}


/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/

