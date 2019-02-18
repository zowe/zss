

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

#include <metal/metal.h>
#include <metal/limits.h>
#include <metal/stddef.h>
#include <metal/stdio.h>
#include <metal/stdlib.h>
#include <metal/string.h>

#ifndef METTLE
#error Non-metal C code is not supported
#endif

#include "zowetypes.h"
#include "alloc.h"
#include "cmutils.h"
#include "crossmemory.h"
#include "logging.h"
#include "metalio.h"
#include "zos.h"
#include "qsam.h"
#include "stcbase.h"
#include "utils.h"
#include "recovery.h"

#include "zis/message.h"
#include "zis/parm.h"
#include "zis/plugin.h"
#include "zis/server.h"
#include "zis/service.h"

#include "zis/services/auth.h"
#include "zis/services/nwm.h"
#include "zis/services/snarfer.h"

/*

BUILD INSTRUCTIONS:

Build using build.sh

DEPLOYMENT:

See details in the ZSS Cross Memory Server installation guide

*/


#define ZIS_PARM_DDNAME "PARMLIB"
#define ZIS_PARM_MEMBER_PREFIX                CMS_PROD_ID"IP"
#define ZIS_PARM_MEMBER_DEFAULT_SUFFIX        "00"
#define ZIS_PARM_MEMBER_SUFFIX_SIZE            2
#define ZIS_PARM_MEMBER_NAME_MAX_SIZE          8

#define ZIS_PARM_MEMBER_SUFFIX                "MEM"
#define ZIS_PARM_SERVER_NAME                  "NAME"
#define ZIS_PARM_COLD_START                   "COLD"
#define ZIS_PARM_DEBUG_MODE                   "DEBUG"

#define ZIS_PARMLIB_PARM_SERVER_NAME          CMS_PROD_ID".NAME"

static int zisRouteService(struct CrossMemoryServerGlobalArea_tag *globalArea,
                           struct CrossMemoryService_tag *service,
                           void *cmsServiceParm) {

  ZISServiceRouterParm *routerParm = cmsServiceParm;
  ZISServiceRouterParm localParm = {0};
  cmCopyFromSecondaryWithCallerKey(&localParm, routerParm, sizeof(localParm));

  ZISServerAnchor *serverAnchor = globalArea->userServerAnchor;
  ZISServicePath *servicePath = &localParm.targetServicePath;
  void *serviceParm = localParm.targetServiceParm;

  ZISServiceAnchor *serviceAnchor =
      crossMemoryMapGet(serverAnchor->serviceTable, servicePath);
  if (serviceAnchor == NULL) {
    return RC_ZIS_SRVC_SERVICE_NOT_FOUND;
  }

  if (!(serviceAnchor->state & ZIS_SERVICE_ANCHOR_STATE_ACTIVE)) {
    return RC_ZIS_SRVC_SERVICE_INACTIVE;
  }

  return serviceAnchor->serve(globalArea,
                              serviceAnchor,
                              &serviceAnchor->serviceData,
                              serviceParm);

}


static int registerZISServiceRouter(CrossMemoryServer *cms) {

  int regRC = RC_CMS_OK;

  int pccpRouterFlags = CMS_SERVICE_FLAG_RELOCATE_TO_COMMON;
  regRC = cmsRegisterService(cms, ZIS_SERVICE_ID_SRVC_ROUTER_CP,
                             zisRouteService, NULL, pccpRouterFlags);
  if (regRC != RC_CMS_OK) {
    return RC_ZIS_ERROR;
  }

  int pcssRouterFlags = CMS_SERVICE_FLAG_RELOCATE_TO_COMMON |
                        CMS_SERVICE_FLAG_SPACE_SWITCH;
  regRC = cmsRegisterService(cms, ZIS_SERVICE_ID_SRVC_ROUTER_SS,
                             zisRouteService, NULL, pcssRouterFlags);
  if (regRC != RC_CMS_OK) {
    return RC_ZIS_ERROR;
  }

  return RC_ZIS_OK;
}

static int registerCoreServices(ZISContext *context) {

  CrossMemoryServer *server = context->cmServer;

  int regRC = RC_CMS_OK;

  regRC = cmsRegisterService(server, ZIS_SERVICE_ID_AUTH_SRV,
                             zisAuthServiceFunction, NULL,
                             CMS_SERVICE_FLAG_RELOCATE_TO_COMMON);
  if (regRC != RC_CMS_OK) {
    return RC_ZIS_ERROR;
  }

  regRC = cmsRegisterService(server, ZIS_SERVICE_ID_SNARFER_SRV,
                             zisSnarferServiceFunction, NULL,
                             CMS_SERVICE_FLAG_SPACE_SWITCH |
                             CMS_SERVICE_FLAG_RELOCATE_TO_COMMON);
  if (regRC != RC_CMS_OK) {
    return RC_ZIS_ERROR;
  }

  regRC = cmsRegisterService(server, ZIS_SERVICE_ID_NWM_SRV,
                             zisNWMServiceFunction, NULL,
                             CMS_SERVICE_FLAG_RELOCATE_TO_COMMON);
  if (regRC != RC_CMS_OK) {
    return RC_ZIS_ERROR;
  }


  regRC = registerZISServiceRouter(server);
  if (regRC != RC_CMS_OK) {
    return RC_ZIS_ERROR;
  }

  return RC_ZIS_OK;
}

#define GET_MAIN_FUNCION_PARMS() \
({ \
  unsigned int r1; \
  __asm(" ST    1,%0 "  : "=m"(r1) : : "r1"); \
  ZISMainFunctionParms * __ptr32 parms = NULL; \
  if (r1 != 0) { \
    parms = (ZISMainFunctionParms *)((*(unsigned int *)r1) & 0x7FFFFFFF); \
  } \
  parms; \
})

static int initServiceTable(ZISContext *context) {

  ZISServerAnchor *anchor = context->zisAnchor;

  if (anchor->serviceTable != NULL) {
#ifdef ZIS_DEV_MODE
    removeCrossMemoryMap(&anchor->serviceTable);
#endif
  }

  if (anchor->serviceTable == NULL) {

    anchor->serviceTable = makeCrossMemoryMap(sizeof(ZISServicePath));
    if (anchor->serviceTable == NULL) {
      if (anchor == NULL) {
        zowelog(NULL, LOG_COMP_STCBASE, ZOWE_LOG_SEVERE,
                ZIS_LOG_CXMS_RES_ALLOC_FAILED_MSG, "service table");
        return RC_ZIS_ERROR;
      }
    }

  }

  return RC_ZIS_OK;
}

static void destroyServiceTable(ZISContext *context) {

  ZISServerAnchor *anchor = context->zisAnchor;

  if (anchor != NULL && anchor->serviceTable != NULL) {
    removeCrossMemoryMap(&anchor->serviceTable);
  }

}

static ZISPluginAnchor *findPluginAnchor(const ZISServerAnchor *serverAnchor,
                                         const ZISPluginName *name) {

  ZISPluginAnchor *currAnchor = serverAnchor->firstPlugin;

  while (currAnchor) {
    if (memcmp(name, &currAnchor->name, sizeof(ZISPluginName)) == 0 &&
        currAnchor->version <= ZIS_PLUGIN_ANCHOR_VERSION) {
      return currAnchor;
    }
    currAnchor = currAnchor->next;
  }

  return NULL;
}

static ZISServiceAnchor *findServiceAnchor(const ZISPluginAnchor *pluginAnchor,
                                           const ZISServiceName *name) {

  ZISServiceAnchor *currAnchor = pluginAnchor->firstService;

  while (currAnchor) {
    if (!memcmp(name, &currAnchor->path.serviceName, sizeof(ZISServiceName)) &&
        currAnchor->version <= ZIS_SERVER_ANCHOR_VERSION) {
      return currAnchor;
    }
    currAnchor = currAnchor->next;
  }

  return NULL;
}


static int cleanPluginAnchor(ZISPluginAnchor *anchor) {

  ZISServiceAnchor *currService = anchor->firstService;
  while (currService != NULL) {
    currService->state &= ~ZIS_SERVICE_ANCHOR_STATE_ACTIVE;
    currService->serve = NULL;
    currService = currService->next;
  }

  return RC_ZIS_OK;
}

static void cleanPlugins(ZISContext *context) {

  ZISServerAnchor *serverAnchor = context->zisAnchor;
  ZISPluginAnchor *currPlugin = serverAnchor->firstPlugin;
  while (currPlugin != NULL) {
    cleanPluginAnchor(currPlugin);
    currPlugin = currPlugin->next;
  }

}


typedef struct ABENDInfo_tag {
  char eyecatcher[8];
#define ABEND_INFO_EYECATCHER "ZISABEDI"
  int completionCode;
  int reasonCode;
} ABENDInfo;


static void extractABENDInfo(RecoveryContext * __ptr32 context,
                             SDWA * __ptr32 sdwa,
                             void * __ptr32 userData) {
  ABENDInfo *info = (ABENDInfo *)userData;
  recoveryGetABENDCode(sdwa, &info->completionCode, &info->reasonCode);
}

static int callPluginInit(ZISContext *context, ZISPlugin *plugin,
                          ZISPluginAnchor *anchor) {

  int status = RC_ZIS_OK;
  int recoveryRC = RC_RCV_OK;
  ABENDInfo abendInfo = {ABEND_INFO_EYECATCHER};

  recoveryRC = recoveryPush("callPluginInit",
                            RCVR_FLAG_RETRY | RCVR_FLAG_DELETE_ON_RETRY,
                            "callPluginInit", extractABENDInfo, &abendInfo,
                            NULL, NULL);
  {
    if (recoveryRC == RC_RCV_OK) {

      if (plugin->init != NULL) {
        int initRC = plugin->init(context, plugin, anchor);
        if (initRC != RC_ZIS_OK) {
          zowelog(NULL, LOG_COMP_ID_CMS, ZOWE_LOG_WARNING,
                  ZIS_LOG_PLUGIN_FAILURE_MSG_PREFIX" plug-in init RC = %d",
                  plugin->name.text, initRC);
          status = RC_ZIS_ERROR;
        }
      }

    } else {
      zowelog(NULL, LOG_COMP_STCBASE, ZOWE_LOG_WARNING,
              ZIS_LOG_PLUGIN_FAILURE_MSG_PREFIX" plug-in init failed, "
              "ABEND S%03X-%02X (recovery RC = %d)",
              plugin->name.text, abendInfo.completionCode,
              abendInfo.reasonCode, recoveryRC);
      status = RC_ZIS_ERROR;
    }
  }

  if (recoveryRC == RC_RCV_OK) {
    recoveryPop();
  }

  return status;
}

static int callPluginTerm(ZISContext *context, ZISPlugin *plugin,
                          ZISPluginAnchor *anchor) {

  int status = RC_ZIS_OK;
  int recoveryRC = RC_RCV_OK;
  ABENDInfo abendInfo = {ABEND_INFO_EYECATCHER};

  recoveryRC = recoveryPush("callPluginTerm",
                            RCVR_FLAG_RETRY | RCVR_FLAG_DELETE_ON_RETRY,
                            "callPluginTerm", extractABENDInfo, &abendInfo,
                            NULL, NULL);
  {
    if (recoveryRC == RC_RCV_OK) {

      if (plugin->term != NULL) {
        int termRC = plugin->term(context, plugin, anchor);
        if (termRC != RC_ZIS_OK) {
          zowelog(NULL, LOG_COMP_ID_CMS, ZOWE_LOG_WARNING,
                  ZIS_LOG_PLUGIN_FAILURE_MSG_PREFIX" plug-in '%16.16s' term "
                  "RC = %d",
                  plugin->name.text, termRC);
          status = RC_ZIS_ERROR;
        }
      }


    } else {
      zowelog(NULL, LOG_COMP_STCBASE, ZOWE_LOG_WARNING,
              ZIS_LOG_PLUGIN_FAILURE_MSG_PREFIX" plug-in term failed, "
              "ABEND S%03X-%02X (recovery RC = %d)",
              plugin->name.text, abendInfo.completionCode,
              abendInfo.reasonCode, recoveryRC);
      status = RC_ZIS_ERROR;
    }
  }

  if (recoveryRC == RC_RCV_OK) {
    recoveryPop();
  }

  return status;
}


static int callServiceInit(ZISContext *context, ZISPlugin *plugin,
                           ZISService *service, ZISServiceAnchor *anchor) {

  int status = RC_ZIS_OK;
  int recoveryRC = RC_RCV_OK;
  ABENDInfo abendInfo = {ABEND_INFO_EYECATCHER};

  recoveryRC = recoveryPush("callServiceInit",
                            RCVR_FLAG_RETRY | RCVR_FLAG_DELETE_ON_RETRY,
                            "callServiceInit", extractABENDInfo, &abendInfo,
                            NULL, NULL);
  {
    if (recoveryRC == RC_RCV_OK) {

      if (service->init != NULL) {
        int initRC = service->init(context, service, anchor);
        if (initRC != RC_ZIS_OK) {
          zowelog(NULL, LOG_COMP_ID_CMS, ZOWE_LOG_WARNING,
                  ZIS_LOG_PLUGIN_FAILURE_MSG_PREFIX" service '%16.16s' "
                  "init RC = %d",
                  plugin->name.text, service->name.text, initRC);
          status = RC_ZIS_ERROR;
        }
      }

    } else {
      zowelog(NULL, LOG_COMP_STCBASE, ZOWE_LOG_WARNING,
              ZIS_LOG_PLUGIN_FAILURE_MSG_PREFIX" service %16.16s' init failed, "
              "ABEND S%03X-%02X (recovery RC = %d)",
              plugin->name.text, service->name.text, abendInfo.completionCode,
              abendInfo.reasonCode, recoveryRC);
      status = RC_ZIS_ERROR;
    }
  }

  if (recoveryRC == RC_RCV_OK) {
    recoveryPop();
  }

  return status;
}

static int callServiceTerm(ZISContext *context, ZISPlugin *plugin,
                           ZISService *service, ZISServiceAnchor *anchor) {

  int status = RC_ZIS_OK;
  int recoveryRC = RC_RCV_OK;
  ABENDInfo abendInfo = {ABEND_INFO_EYECATCHER};

  recoveryRC = recoveryPush("callServiceTerm",
                            RCVR_FLAG_RETRY | RCVR_FLAG_DELETE_ON_RETRY,
                            "callServiceTerm", extractABENDInfo, &abendInfo,
                            NULL, NULL);
  {
    if (recoveryRC == RC_RCV_OK) {

      if (service->term != NULL) {
        int termRC = service->term(context, service, anchor);
        if (termRC != RC_ZIS_OK) {
          zowelog(NULL, LOG_COMP_ID_CMS, ZOWE_LOG_WARNING,
                  ZIS_LOG_PLUGIN_FAILURE_MSG_PREFIX" service '%16.16s' term "
                  "RC = %d",
                  plugin->name.text, service->name.text, termRC);
          status = RC_ZIS_ERROR;
        }
      }

    } else {
      zowelog(NULL, LOG_COMP_STCBASE, ZOWE_LOG_WARNING,
              ZIS_LOG_PLUGIN_FAILURE_MSG_PREFIX" service '%16.16s' term failed, "
              "ABEND S%03X-%02X (recovery RC = %d)",
              plugin->name.text, service->name.text, abendInfo.completionCode,
              abendInfo.reasonCode, recoveryRC);
      status = RC_ZIS_ERROR;
    }
  }

  if (recoveryRC == RC_RCV_OK) {
    recoveryPop();
  }

  return status;
}

static int installServices(ZISContext *context, ZISPlugin *plugin,
                           ZISPluginAnchor *pluginAnchor) {

  CrossMemoryMap *serviceTable = context->zisAnchor->serviceTable;

  for (unsigned int i = 0; i < plugin->serviceCount; i++) {

    ZISService *service = &plugin->services[i];

    ZISServiceAnchor *anchor = findServiceAnchor(pluginAnchor, &service->name);
    if (anchor == NULL) {
      anchor = zisCreateServiceAnchor(plugin, service);
      if (anchor == NULL) {
        zowelog(NULL, LOG_COMP_ID_CMS, ZOWE_LOG_SEVERE,
                ZIS_LOG_CXMS_RES_ALLOC_FAILED_MSG, "service anchor");
        return RC_ZIS_ERROR;
      }
      anchor->next = pluginAnchor->firstService;
      pluginAnchor->firstService = anchor;
    } else {
      zisUpdateServiceAnchor(anchor, plugin, service);
    }

    service->anchor = anchor;

    /* Service init call. */
    int initRC = callServiceInit(context, plugin, service, anchor);
    if (initRC != RC_ZIS_OK) {
      continue;
    }

    int putRC = crossMemoryMapPut(serviceTable, &anchor->path, anchor);
    if (putRC == 1) {
      *crossMemoryMapGetHandle(serviceTable, &anchor->path) = anchor;
    } else if (putRC == -1) {
      zowelog(NULL, LOG_COMP_ID_CMS, ZOWE_LOG_SEVERE,
              ZIS_LOG_CXMS_RES_ALLOC_FAILED_MSG, "service map entry");
      return RC_ZIS_ERROR;
    }

    anchor->state |= ZIS_SERVICE_ANCHOR_STATE_ACTIVE;

    zowelog(NULL, LOG_COMP_ID_CMS, ZOWE_LOG_WARNING, ZIS_LOG_SERVICE_ADDED_MSG,
            anchor->path.pluginName, anchor->path.serviceName.text,
            plugin->pluginVersion);
  }

  return RC_ZIS_OK;
}

static int relocatePluginToLPAIfNeeded(ZISPlugin **pluginAddr,
                                       ZISPluginAnchor *anchor,
                                       EightCharString moduleName) {

  ZISPlugin *plugin = *pluginAddr;
  bool lpaNeeded = plugin->flags & ZIS_PLUGIN_FLAG_LPA ? true : false;
  bool lpaPresent = anchor->flags & ZIS_PLUGIN_ANCHOR_FLAG_LPA ? true : false;

  if (lpaPresent) {

    if (anchor->pluginVersion != plugin->pluginVersion) {

      zowelog(NULL, LOG_COMP_ID_CMS, ZOWE_LOG_WARNING,
              ZIS_LOG_PLUGIN_VER_MISMATCH_MSG, plugin->name.text,
              plugin->pluginVersion, anchor->pluginVersion);
      zowedump(NULL, LOG_COMP_ID_CMS, ZOWE_LOG_WARNING,
               (char *)&anchor->moduleInfo, sizeof(LPMEA));

#ifdef ZIS_LPA_DEV_MODE

      zowelog(NULL, LOG_COMP_ID_CMS, ZOWE_LOG_INFO, ZIS_LOG_DEBUG_MSG_ID
              " Plugin LPA dev mode enabled, issuing CSVDYLPA DELETE\n");

      int lpaRSN = 0;
      int lpaRC = lpaDelete(&anchor->moduleInfo, &lpaRSN);
      if (lpaRC != 0) {
        zowelog(NULL, LOG_COMP_ID_CMS, ZOWE_LOG_SEVERE, ZIS_LOG_LPA_FAILURE_MSG,
                "DELETE", anchor->moduleInfo.inputInfo.name, lpaRC, lpaRSN);
        return RC_ZIS_ERROR;
      }
#endif
      memset(&anchor->moduleInfo, 0, sizeof(anchor->moduleInfo));
      anchor->flags &= ~ZIS_PLUGIN_ANCHOR_FLAG_LPA;
      lpaPresent = false;
    }  else {
      lpaPresent = true;
    }

  }

  /* Check if LPA, and load if needed */
  if (lpaNeeded) {

    if (!lpaPresent) {

      EightCharString ddname = {"STEPLIB "};
      int lpaRSN = 0;
      int lpaRC = lpaAdd(&anchor->moduleInfo, &ddname, &moduleName, &lpaRSN);
      if (lpaRC != 0) {
        zowelog(NULL, LOG_COMP_ID_CMS, ZOWE_LOG_SEVERE, ZIS_LOG_LPA_FAILURE_MSG,
                "ADD", anchor->moduleInfo.inputInfo.name, lpaRC, lpaRSN);
        return RC_ZIS_ERROR;
      }

      anchor->flags |= ZIS_PLUGIN_ANCHOR_FLAG_LPA;
    }

    /* Invoke EP to get relocated services */
    LPMEA *lpaInfo = &anchor->moduleInfo;
    void *ep =
        *(void * __ptr32 *)&lpaInfo->outputInfo.stuff.successInfo.entryPointAddr;
    ZISPluginDescriptorFunction *getPluginDescriptor =
        (ZISPluginDescriptorFunction *)((int)ep & 0xFFFFFFFE);

    *pluginAddr = getPluginDescriptor();

  }

  return RC_ZIS_OK;
}

static ZISPlugin *getPluginByName(ZISContext *context,
                                  const ZISPluginName *name) {

  /* TODO use a hashtable when the number of plugins grows, no point in that
   * right now. */

  ZISPlugin *currPlugin = context->firstPlugin;
  while (currPlugin != NULL) {

    if (!memcmp(&currPlugin->name, name, sizeof(ZISPluginName))) {
      return currPlugin;
    }

    currPlugin = currPlugin->next;
  }

  return NULL;
}

static ZISPlugin *getPluginByNickname(ZISContext *context,
                                      const ZISPluginNickname *nickname) {

  /* TODO use a hashtable when the number of plugins grows, no point in that
   * right now. */

  ZISPlugin *currPlugin = context->firstPlugin;
  while (currPlugin != NULL) {

    if (!memcmp(&currPlugin->nickname, nickname, sizeof(ZISPluginNickname))) {
      return currPlugin;
    }

    currPlugin = currPlugin->next;
  }

  return NULL;
}

static void addPlugin(ZISContext *context, ZISPlugin *plugin) {
  plugin->next = context->firstPlugin;
  context->firstPlugin = plugin;
}

static int installPlugin(ZISContext *context, ZISPlugin *plugin,
                         EightCharString moduleName) {

  ZISPluginAnchor *anchor = findPluginAnchor(context->zisAnchor, &plugin->name);
  if (anchor == NULL) {
    anchor = zisCreatePluginAnchor(plugin);
    if (anchor == NULL) {
      zowelog(NULL, LOG_COMP_ID_CMS, ZOWE_LOG_WARNING,
              ZIS_LOG_CXMS_RES_ALLOC_FAILED_MSG, "plug-in anchor");
      return RC_ZIS_ERROR;
    }
    anchor->next = context->zisAnchor->firstPlugin;
    context->zisAnchor->firstPlugin = anchor;
  }

  int relocateRC = relocatePluginToLPAIfNeeded(&plugin, anchor, moduleName);
  if (relocateRC != RC_ZIS_OK) {
    return relocateRC;
  }

  /* Save plugin definition for later use by server. */
  addPlugin(context, plugin);
  plugin->anchor = anchor;

  /* Update anchor */
  anchor->pluginVersion = plugin->pluginVersion;

  /* Plugin init call. */
  int initRC = callPluginInit(context, plugin, anchor);
  if (initRC != RC_ZIS_OK) {
    return initRC;
  }

  /* Install services. */
  installServices(context, plugin, anchor);

  return RC_ZIS_OK;
}

static ZISPlugin *tryLoadingPlugin(const char *pluginName,
                                   EightCharString moduleName) {

  void *ep = NULL;

  int recoveryRC = RC_RCV_OK;
  ABENDInfo abendInfo = {ABEND_INFO_EYECATCHER};

  /* Try to load the plugin module. */
  recoveryRC = recoveryPush("tryLoadingPlugin():load",
                            RCVR_FLAG_RETRY | RCVR_FLAG_DELETE_ON_RETRY,
                            "ZIS load plugin", extractABENDInfo, &abendInfo,
                            NULL, NULL);

  {
    if (recoveryRC == RC_RCV_OK) {

      int loadStatus = 0;
      ep = loadByNameLocally(moduleName.text, &loadStatus);
      if (ep == NULL || loadStatus != 0) {
        zowelog(NULL, LOG_COMP_STCBASE, ZOWE_LOG_WARNING,
                ZIS_LOG_PLUGIN_FAILURE_MSG_PREFIX" module '%8.8s' not loaded, "
                "EP = %p, status = %d",
                pluginName, moduleName.text, ep, loadStatus);
      }

    } else {
      zowelog(NULL, LOG_COMP_STCBASE, ZOWE_LOG_WARNING,
              ZIS_LOG_PLUGIN_FAILURE_MSG_PREFIX" module '%8.8s' not loaded, "
              "ABEND S%03X-%02X (recovery RC = %d)",
              pluginName, moduleName.text, abendInfo.completionCode,
              abendInfo.reasonCode, recoveryRC);
    }
  }

  if (recoveryRC == RC_RCV_OK) {
    recoveryPop();
  }

  if (ep == NULL) {
    return NULL;
  }

  ZISPluginDescriptorFunction *getPluginDescriptor =
      (ZISPluginDescriptorFunction *)((int)ep & 0xFFFFFFFE);
  ZISPlugin *plugin = NULL;

  /* Try executing plugin EP. */
  recoveryRC = recoveryPush("tryLoadingPlugin():exec",
                            RCVR_FLAG_RETRY | RCVR_FLAG_DELETE_ON_RETRY,
                            "ZIS get plugin", extractABENDInfo, &abendInfo,
                            NULL, NULL);
  {
    if (recoveryRC == RC_RCV_OK) {

      plugin = getPluginDescriptor();
      if (plugin == NULL) {
        zowelog(NULL, LOG_COMP_STCBASE, ZOWE_LOG_WARNING,
                ZIS_LOG_PLUGIN_FAILURE_MSG_PREFIX" plug-in descriptor not "
                "received from module '%8.8s'",
                pluginName, moduleName);
      }

    } else {
      zowelog(NULL, LOG_COMP_STCBASE, ZOWE_LOG_WARNING,
              ZIS_LOG_PLUGIN_FAILURE_MSG_PREFIX" plug-in EP not executed, "
              "module '%8.8s', recovery RC = %d, ABEND %03X-%02X",
              pluginName, moduleName, recoveryRC, abendInfo.completionCode,
              abendInfo.reasonCode);
    }
  }

  if (recoveryRC == RC_RCV_OK) {
    recoveryPop();
  }

  return plugin;
}

static bool isServiceValid(const ZISService *service) {

  if (memcmp(service->eyecatcher, ZIS_SERVICE_EYECATCHER,
             sizeof(service->eyecatcher))) {
    return false;
  }

  if (service->version > ZIS_SERVICE_VERSION) {
    return false;
  }

  return true;
}

static bool isPluginValid(const char *name, const ZISPlugin *plugin) {

  bool isValid = false;
  ABENDInfo abendInfo = {ABEND_INFO_EYECATCHER};

  int recoveryRC = recoveryPush("isPluginValid()",
                                RCVR_FLAG_RETRY | RCVR_FLAG_DELETE_ON_RETRY,
                                "ZIS validate plugin", extractABENDInfo,
                                &abendInfo, NULL, NULL);
  do {

    if (recoveryRC == RC_RCV_OK) {

      if (memcmp(plugin->eyecatcher, ZIS_PLUGIN_EYECATCHER,
                 sizeof(plugin->eyecatcher))) {
        zowelog(NULL, LOG_COMP_STCBASE, ZOWE_LOG_WARNING,
                ZIS_LOG_PLUGIN_FAILURE_MSG_PREFIX" bad eyecatcher",
                name);
        break;
      }

      if (plugin->version > ZIS_PLUGIN_VERSION) {
        zowelog(NULL, LOG_COMP_STCBASE, ZOWE_LOG_WARNING,
                ZIS_LOG_PLUGIN_FAILURE_MSG_PREFIX" unsupported version %d "
                "(vs %d)",
                name, plugin->version, ZIS_PLUGIN_VERSION);
        break;
      }

      if (plugin->serviceCount > plugin->maxServiceCount) {
        zowelog(NULL, LOG_COMP_STCBASE, ZOWE_LOG_WARNING,
                ZIS_LOG_PLUGIN_FAILURE_MSG_PREFIX" bad service count, "
                "current = %d, max = %d",
                name, plugin->serviceCount, plugin->maxServiceCount);
        break;
      }

      for (unsigned i = 0; i < plugin->serviceCount; i++) {
        if (isServiceValid(&plugin->services[i]) == false) {
          zowelog(NULL, LOG_COMP_STCBASE, ZOWE_LOG_WARNING,
                  ZIS_LOG_PLUGIN_FAILURE_MSG_PREFIX" service #%d invalid",
                  name, i);
          zowedump(NULL, LOG_COMP_STCBASE, ZOWE_LOG_INFO,
                   (void *)&plugin->services[i], sizeof(ZISService));
          break;
        }
      }

      isValid = true;

    } else {
      zowelog(NULL, LOG_COMP_STCBASE, ZOWE_LOG_WARNING,
              ZIS_LOG_PLUGIN_FAILURE_MSG_PREFIX" installation failed, "
              "recovery RC = %d, ABEND %03X-%02X",
              name, recoveryRC, abendInfo.completionCode, abendInfo.reasonCode);
      break;
    }

  } while (0);

  if (recoveryRC == RC_RCV_OK) {
    recoveryPop();
  }

  return isValid;
}

static bool isDuplicatePlugin(ZISContext *context,
                              ZISPlugin *plugin) {

  if (getPluginByName(context, &plugin->name) != NULL) {
    zowelog(NULL, LOG_COMP_STCBASE, ZOWE_LOG_WARNING,
            ZIS_LOG_PLUGIN_FAILURE_MSG_PREFIX" plug-in with this name already "
            "exists ", plugin->name.text);
    return true;
  }

  if (getPluginByNickname(context, &plugin->nickname) != NULL) {
    zowelog(NULL, LOG_COMP_STCBASE, ZOWE_LOG_WARNING,
            ZIS_LOG_PLUGIN_FAILURE_MSG_PREFIX" plug-in with nickname '%s' "
            "already exists ", plugin->name.text, plugin->nickname.text);
    return true;
  }

  return false;
}

static void visitPluginParm(const char *name, const char *value,
                            void *userData) {

  if (strstr(name, "ZWES.PLUGIN.") != name) {
    return;
  }
  const char *pluginName = name + strlen("ZWES.PLUGIN.");

  if (value == NULL) {
    zowelog(NULL, LOG_COMP_STCBASE, ZOWE_LOG_WARNING,
            ZIS_LOG_PLUGIN_FAILURE_MSG_PREFIX" module name not provided",
            pluginName);
    return;
  }


  EightCharString moduleName = {"        "};
  size_t moduleNameLength = strlen(value);
  if (moduleNameLength > sizeof(moduleName)) {
    zowelog(NULL, LOG_COMP_STCBASE, ZOWE_LOG_WARNING,
            ZIS_LOG_PLUGIN_FAILURE_MSG_PREFIX" module name too long '%s'",
            pluginName, value);
    return;
  }
  memcpy(moduleName.text, value, moduleNameLength);

  ZISPlugin *plugin = tryLoadingPlugin(pluginName, moduleName);
  if (plugin == NULL) {
    return;
  }

  if (isPluginValid(pluginName, plugin) == false) {
    return;
  }

  ZISContext *context = userData;

  if (isDuplicatePlugin(context, plugin)) {
    return;
  }

  installPlugin(context, plugin, moduleName);

}

ZISServerAnchor *createZISServerAnchor() {

  unsigned int size = sizeof(ZISServerAnchor);
  int subpool = CROSS_MEMORY_SERVER_SUBPOOL;
  int key = CROSS_MEMORY_SERVER_KEY;

  ZISServerAnchor *anchor = cmAlloc(size, subpool, key);
  if (anchor == NULL) {
    return NULL;
  }

  memset(anchor, 0, size);
  memcpy(anchor->eyecatcher, ZIS_SERVER_ANCHOR_EYECATCHER,
         sizeof(anchor->eyecatcher));
  anchor->version = ZIS_SERVER_ANCHOR_VERSION;
  anchor->key = key;
  anchor->subpool = subpool;
  anchor->size = size;

  return anchor;
}

static void removeZISServerAnchor(ZISServerAnchor **anchor) {

  unsigned int size = sizeof(ZISServerAnchor);
  int subpool = CROSS_MEMORY_SERVER_SUBPOOL;
  int key = CROSS_MEMORY_SERVER_KEY;

  cmFree2((void **)anchor, size, subpool, key);

}

static int initGlobalResources(ZISContext *context) {

  CrossMemoryServerGlobalArea *cmsGA = context->cmsGA;
  ZISServerAnchor *anchor = cmsGA->userServerAnchor;
  if (anchor == NULL) {
    anchor = createZISServerAnchor();
    if (anchor == NULL) {
      zowelog(NULL, LOG_COMP_STCBASE, ZOWE_LOG_SEVERE,
              ZIS_LOG_CXMS_RES_ALLOC_FAILED_MSG, "server anchor");
      return RC_ZIS_ERROR;
    }
    cmsGA->userServerAnchor = anchor;
  }
  context->zisAnchor = anchor;

  int initServiceTableRC = initServiceTable(context);
  if (initServiceTableRC != RC_ZIS_OK) {
    return initServiceTableRC;
  }

  return RC_ZIS_OK;
}

static ZISContext makeContext(STCBase *base) {

  ZISContext cntx = {
      .eyecatcher = ZIS_CONTEXT_EYECATCHER,
      .stcBase = base,
      .parms = NULL,
      .cmServer = NULL,
      .cmsGA = NULL,
      .zisAnchor = NULL,
  };

  return cntx;
}

static void cleanContext(ZISContext *context) {

  context->zisAnchor = NULL;
  context->cmsGA = NULL;

  if (context->cmServer != NULL) {
    removeCrossMemoryServer(context->cmServer);
    context->cmServer = NULL;
  }

  if (context->parms != NULL) {
    zisRemoveParmSet(context->parms);
    context->parms = NULL;
  }

  context->stcBase = NULL;

}

static void initLoggin() {
  cmsInitializeLogging();
}

static void printStartMessage() {
  wtoPrintf(ZIS_LOG_STARTUP_MSG);
  zowelog(NULL, LOG_COMP_STCBASE, ZOWE_LOG_INFO, ZIS_LOG_STARTUP_MSG);
}

static void printStopMessage(int status) {
  if (status == RC_ZIS_OK) {
    wtoPrintf(ZIS_LOG_CXMS_TERM_OK_MSG);
    zowelog(NULL, LOG_COMP_STCBASE, ZOWE_LOG_INFO, ZIS_LOG_CXMS_TERM_OK_MSG);
  } else {
    wtoPrintf(ZIS_LOG_CXMS_TERM_FAILURE_MSG, status);
    zowelog(NULL, LOG_COMP_STCBASE, ZOWE_LOG_SEVERE,
           ZIS_LOG_CXMS_TERM_FAILURE_MSG, status);
  }
}

static void deployPlugins(ZISContext *context) {
  zisIterateParms(context->parms, visitPluginParm, context);
}

static void terminatePlugins(ZISContext *context) {

  ZISPlugin *currPlugin = context->firstPlugin;
  while (currPlugin != NULL) {

    for (unsigned int i = 0; i < currPlugin->serviceCount; i++) {

      ZISService *currService = &currPlugin->services[i];

      callServiceTerm(context, currPlugin, currService, currService->anchor);

    }

    callPluginTerm(context, currPlugin, currPlugin->anchor);

    currPlugin = currPlugin->next;
  }

}

static int initServer(CrossMemoryServerGlobalArea *ga, void *userData) {

  ZISContext *context = userData;
  context->cmsGA = ga;
  context->cmServer = ga->localServerAddress;

  int initRC = initGlobalResources(context);
  if (initRC != RC_ZIS_OK) {
    return initRC;
  }

  cleanPlugins(context);
  deployPlugins(context);

  return RC_CMS_OK;
}

static int cleanupServer(CrossMemoryServerGlobalArea *ga, void *userData) {

  ZISContext *context = userData;

  terminatePlugins(context);

#ifdef ZIS_DEV_MODE
  destroyServiceTable(context);
#endif

  return RC_CMS_OK;
}

static int handleModifyCommands(CrossMemoryServerGlobalArea *globalArea,
                                const CMSModifyCommand *command,
                                CMSModifyCommandStatus *status,
                                void *userData) {

  ZISContext *context = userData;

  ZISPluginNickname nickname;
  memset(&nickname.text, ' ', sizeof(nickname.text));

  size_t targetLength = command->target ? strlen(command->target) : 0;
  if (targetLength <= 0 && sizeof(nickname.text) < targetLength) {
    return RC_CMS_OK;
  }
  memcpy(nickname.text, command->target, targetLength);

  ZISPlugin *plugin = getPluginByNickname(context, &nickname);
  if (plugin != NULL && plugin->handleCommand != NULL) {
    plugin->handleCommand(context, plugin, plugin->anchor,
                          command, status);
    if (*status == CMS_MODIFY_COMMAND_STATUS_PROCESSED) {
      return RC_CMS_OK;
    }
  }

  return RC_CMS_OK;
}

typedef struct PARMBLIBMember_tag {
  char nameNullTerm[16];
} PARMLIBMember;

static int extractPARMLIBMemberName(ZISParmSet *parms,
                                    PARMLIBMember *member) {

  /* find out if MEM has been specified in the JCL and use it to create
   * the parmlib member name */

  const char *memberSuffix = zisGetParmValue(parms, ZIS_PARM_MEMBER_SUFFIX);
  if (memberSuffix == NULL) {
    memberSuffix = ZIS_PARM_MEMBER_DEFAULT_SUFFIX;
  }

  member->nameNullTerm[0] = '\0';
  if (strlen(memberSuffix) == ZIS_PARM_MEMBER_SUFFIX_SIZE) {
    strcat(member->nameNullTerm, ZIS_PARM_MEMBER_PREFIX);
    strcat(member->nameNullTerm, memberSuffix);
  } else {
    zowelog(NULL, LOG_COMP_STCBASE, ZOWE_LOG_SEVERE,
            ZIS_LOG_CXMS_BAD_PMEM_SUFFIX_MSG, memberSuffix);
    return RC_ZIS_ERROR;
  }

  return RC_ZIS_OK;
}

static int loadConfig(ZISContext *context,
                      const ZISMainFunctionParms *mainParms) {

  if (mainParms != NULL && mainParms->textLength > 0) {
    zowelog(NULL, LOG_COMP_STCBASE, ZOWE_LOG_INFO,
            ZIS_LOG_INPUT_PARM_MSG, mainParms);
    zowedump(NULL, LOG_COMP_STCBASE, ZOWE_LOG_INFO,
             (void *)mainParms,
             sizeof(mainParms) + mainParms->textLength);
  }

  context->parms = zisMakeParmSet();
  if (context->parms == NULL) {
    zowelog(NULL, LOG_COMP_STCBASE, ZOWE_LOG_SEVERE,
            ZIS_LOG_CXMS_RES_ALLOC_FAILED_MSG, "parameter set");
    return RC_ZIS_ERROR;
  }

  int readMainParmsRC = zisReadMainParms(context->parms, mainParms);
  if (readMainParmsRC != RC_ZISPARM_OK) {
    zowelog(NULL, LOG_COMP_STCBASE, ZOWE_LOG_SEVERE, ZIS_LOG_CONFIG_FAILURE_MSG,
            "main parms not read", readMainParmsRC);
    return RC_ZIS_ERROR;
  }

  PARMLIBMember parmlibMember;
  int parmlibRC = extractPARMLIBMemberName(context->parms, &parmlibMember);
  if (parmlibRC != RC_ZIS_OK) {
    zowelog(NULL, LOG_COMP_STCBASE, ZOWE_LOG_SEVERE, ZIS_LOG_CONFIG_FAILURE_MSG,
            "PARMLIB member not extracted", parmlibRC);
    return RC_ZIS_ERROR;
  }

  ZISParmStatus readStatus = {0};
  int readParmRC = zisReadParmlib(context->parms, ZIS_PARM_DDNAME,
                                  parmlibMember.nameNullTerm, &readStatus);
  if (readParmRC != RC_ZISPARM_OK) {
    if (readParmRC == RC_ZISPARM_MEMBER_NOT_FOUND) {
      zowelog(NULL, LOG_COMP_STCBASE, ZOWE_LOG_SEVERE,
              ZIS_LOG_CXMS_CFG_MISSING_MSG,
              parmlibMember.nameNullTerm, readParmRC);
      return RC_ZIS_ERROR;
    } else {
      zowelog(NULL, LOG_COMP_STCBASE, ZOWE_LOG_SEVERE,
              ZIS_LOG_CXMS_CFG_NOT_READ_MSG,
              parmlibMember.nameNullTerm, readParmRC,
              readStatus.internalRC, readStatus.internalRSN);
      return RC_ZIS_ERROR;
    }
  }

 return RC_ZIS_OK;
}

static int getCMSConfigFlags(const ZISParmSet *zisParms) {

  int flags = CMS_SERVER_FLAG_NONE;

  const char *coldStartValue = zisGetParmValue(zisParms, ZIS_PARM_COLD_START);
  if (coldStartValue && strlen(coldStartValue) == 0) {
    flags |= CMS_SERVER_FLAG_COLD_START;
  }

  const char *debugValue = zisGetParmValue(zisParms, ZIS_PARM_DEBUG_MODE);
  if (coldStartValue && strlen(coldStartValue) == 0) {
    flags |= CMS_SERVER_FLAG_DEBUG;
  }

  return flags;
}

static CrossMemoryServerName getCMServerName(const ZISParmSet *zisParms) {

  CrossMemoryServerName serverName;
  const char *serverNameNullTerm = NULL;

  /* Check JCL parm */
  serverNameNullTerm = zisGetParmValue(zisParms, ZIS_PARM_SERVER_NAME);

  if (serverNameNullTerm == NULL) {
    /* Check PARMLIB member if JCL one has not been found */
    serverNameNullTerm = zisGetParmValue(zisParms, ZIS_PARMLIB_PARM_SERVER_NAME);
  }

  if (serverNameNullTerm != NULL) {
    serverName = cmsMakeServerName(serverNameNullTerm);
    zowelog(NULL, LOG_COMP_STCBASE, ZOWE_LOG_INFO,
            ZIS_LOG_CXMS_NAME_MSG, serverName.nameSpacePadded);
  } else {
    serverName = CMS_DEFAULT_SERVER_NAME;
    zowelog(NULL, LOG_COMP_STCBASE, ZOWE_LOG_INFO, ZIS_LOG_CXMS_NO_NAME_MSG,
            serverName.nameSpacePadded);
  }

  return serverName;
}

static int initCoreServer(ZISContext *context) {

  CrossMemoryServerName serverName = getCMServerName(context->parms);
  unsigned int cmsFlags = getCMSConfigFlags(context->parms);
  int makeServerRSN = 0;
  CrossMemoryServer *cmServer = makeCrossMemoryServer2(context->stcBase,
                                                       &serverName,
                                                       cmsFlags,
                                                       initServer,
                                                       cleanupServer,
                                                       handleModifyCommands,
                                                       context,
                                                       &makeServerRSN);
  if (cmServer == NULL) {
    zowelog(NULL, LOG_COMP_STCBASE, ZOWE_LOG_SEVERE,
            ZIS_LOG_CXMS_NOT_CREATED_MSG, makeServerRSN);
    return RC_ZIS_ERROR;
  }

  context->cmServer = cmServer;

  int loadParmRSN = 0;
  int loadParmRC = zisLoadParmsToCMServer(cmServer, context->parms,
                                          &loadParmRSN);
  if (loadParmRC != RC_ZISPARM_OK) {
    zowelog(NULL, LOG_COMP_STCBASE, ZOWE_LOG_SEVERE,
            ZIS_LOG_CXMS_CFG_NOT_LOADED_MSG, loadParmRC, loadParmRSN);
    return RC_ZIS_ERROR;
  }

  return RC_ZIS_OK;
}

static int runCoreServer(ZISContext *context) {

  int startRC = cmsStartMainLoop(context->cmServer);
  if (startRC != RC_CMS_OK) {
    zowelog(NULL, LOG_COMP_STCBASE, ZOWE_LOG_SEVERE,
            ZIS_LOG_CXMS_NOT_STARTED_MSG, startRC);
    return RC_ZIS_ERROR;
  }

  return RC_ZIS_OK;
}

static int run(STCBase *base, const ZISMainFunctionParms *mainParms) {

  initLoggin();

  printStartMessage();

  ZISContext context = makeContext(base);

  int status = RC_ZIS_OK;
  do {

    status = loadConfig(&context, mainParms);
    if (status != RC_ZIS_OK) {
      break;
    }

    status = initCoreServer(&context);
    if (status != RC_ZIS_OK) {
      break;
    }

    status = registerCoreServices(&context);
    if (status != RC_ZIS_OK) {
      break;
    }

    status = runCoreServer(&context);
    if (status != RC_ZIS_OK) {
      break;
    }

  } while (0);

  cleanContext(&context);

  printStopMessage(status);

  return status;
}

int main() {

  const ZISMainFunctionParms *mainParms = GET_MAIN_FUNCION_PARMS();

  int rc = RC_ZIS_OK;

  STCBase *base = (STCBase *)safeMalloc31(sizeof(STCBase), "stcbase");
  stcBaseInit(base);
  {
    rc = run(base, mainParms);
  }
  stcBaseTerm(base);
  safeFree31((char *)base, sizeof(STCBase));
  base = NULL;

  return rc;
}


/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/
