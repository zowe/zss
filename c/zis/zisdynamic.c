
/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/

#include <metal/metal.h>
#include <metal/stddef.h>
#include <metal/stdint.h>
#include <metal/stdlib.h>
#include <metal/string.h>


#include "zvt.h"
#include "zowetypes.h"
#include "alloc.h"
#include "utils.h"
#include "qsam.h"
#include "metalio.h"
#include "collections.h"
#include "logging.h"
#include "zos.h"
#include "le.h"
#include "unixfile.h"
#include "recovery.h"
#include "shrmem64.h"
#include "cmutils.h"
#include "pause-element.h"
#include "scheduling.h"
#include "xlate.h"

#include "crossmemory.h"
#include "cpool64.h"
#include "zis/zisdynamic.h"
#include "zis/plugin.h"
#include "zis/server.h"
#include "zis/server-api.h"
#include "zis/service.h"
#include "zis/client.h"
#include "zis/zisstubs.h"
#include "zis/message.h"

#ifndef ZISDYN_MAJOR_VERSION
#define ZISDYN_MAJOR_VERSION 0
#endif

#ifndef ZISDYN_MINOR_VERSION
#define ZISDYN_MINOR_VERSION 0
#endif

#ifndef ZISDYN_REVISION
#define ZISDYN_REVISION 0
#endif

#ifndef ZISDYN_VERSION_DATE_STAMP
#define ZISDYN_VERSION_DATE_STAMP 0
#endif

#define ZISDYN_PLUGIN_NAME      "ZISDYNAMIC      "
#define ZISDYN_PLUGIN_NICKNAME  "ZDYN"
#define ZISDYN_PLUGIN_VERSION   1

#define RC_ZISDYN_OK 0
#define RC_ZISDYN_UNSUPPORTED_ZIS 8
#define RC_ZISDYN_ALLOC_FAILED 16
#define RC_ZISDYN_ENV_ERROR 17

#define STATIC_ASSERT($expr) typedef char p[($expr) ? 1 : -1]

ZOWE_PRAGMA_PACK

typedef struct ZISDynStubVector_tag {

#define ZISDYN_STUB_VEC_EYECATCHER    "ZWESISSV"
#define ZISDYN_STUB_VEC_VERSION       1
#define ZISDYN_STUB_VEC_KEY           ZVT_KEY
#define ZISDYN_STUB_VEC_SUBPOOL       ZVT_SUBPOOL
#define ZISDYN_STUB_VEC_HEADER_SIZE   0x28

  char eyecatcher[8];
  uint8_t version;
  uint8_t key;
  uint8_t subpool;
  char reserved1[1];
  uint16_t size;
  char reserved2[2];
  /* Offset 0x10 */
  uint64_t creationTime;
  char jobName[8];
  /* Offset 0x20 */
  uint16_t asid;
  char reserved22[4];

  /* Offset 0x26 */
  uint16_t stubsVersion;
  /* Offset 0x28 */
  void *slots[MAX_ZIS_STUBS];

} ZISDynStubVector;

typedef struct DynamicPluginData_tag {
  uint64_t initTime;
  ZISDynStubVector *stubVector;
  char unused[48];
} DynamicPluginData;

STATIC_ASSERT(sizeof(DynamicPluginData) <= sizeof(ZISPluginData));

ZOWE_PRAGMA_PACK_RESET

static void initStubVector(ZISDynStubVector *stubVector);

static ZISDynStubVector *makeStubVector(void) {

  ZISDynStubVector *vector = cmAlloc(sizeof(ZISDynStubVector),
                                     ZISDYN_STUB_VEC_SUBPOOL,
                                     ZISDYN_STUB_VEC_KEY);
  if (vector == NULL) {
    return NULL;
  }

  initStubVector(vector);

  return vector;
}

static void deleteStubVector(ZISDynStubVector *vectorAddr) {
  if (vectorAddr == NULL) {
    return;
  }
  cmFree(vectorAddr, vectorAddr->size, ZISDYN_STUB_VEC_SUBPOOL,
         ZISDYN_STUB_VEC_KEY);
}

static bool isStubVectorValid(const ZISDynStubVector *vector,
                              const char **reason) {
  if (memcmp(vector->eyecatcher, ZISDYN_STUB_VEC_EYECATCHER,
             sizeof(vector->eyecatcher))) {
    *reason = "bad eyecatcher";
    return false;
  }
  if (vector->version > ZISDYN_STUB_VEC_VERSION) {
    *reason = "incompatible version";
    return false;
  }
  if (vector->size < sizeof(ZISDynStubVector)) {
    *reason = "insufficient size";
    return false;
  }
  if (vector->key != ZISDYN_STUB_VEC_KEY) {
    *reason = "unexpected key";
    return false;
  }
  if (vector->subpool != ZISDYN_STUB_VEC_SUBPOOL) {
    *reason = "unexpected subpool";
    return false;
  }
  *reason = "";
  return true;
}

static int installDynamicLinkageVector(ZISContext *context,
                                       void *dynamicLinkageVector) {
  ZISServerAnchor *serverAnchor = context->zisAnchor;

  if (serverAnchor->flags & ZIS_SERVER_ANCHOR_FLAG_DYNLINK) {
    CrossMemoryServerGlobalArea *cmsGA = context->cmsGA;
    cmsGA->userServerDynLinkVector = dynamicLinkageVector;
    return RC_ZISDYN_OK;
  } else {
    return RC_ZISDYN_UNSUPPORTED_ZIS;
  }
}

static void uninstallDynamicLinkageVector(ZISContext *context) {
  CrossMemoryServerGlobalArea *cmsGA = context->cmsGA;
  cmsGA->userServerDynLinkVector = NULL;
}

static int initZISDynamic(struct ZISContext_tag *context,
                          ZISPlugin *plugin,
                          ZISPluginAnchor *anchor) {

  zowelog(NULL, LOG_COMP_ID_CMS, ZOWE_LOG_INFO, ZISDYN_LOG_STARTUP_MSG,
          ZISDYN_MAJOR_VERSION, ZISDYN_MINOR_VERSION, ZISDYN_REVISION,
          ZISDYN_VERSION_DATE_STAMP, ZIS_STUBS_VERSION);

  bool isLPADevMode = zisIsLPADevModeOn(context);
  if (isLPADevMode) {
    zowelog(NULL, LOG_COMP_ID_CMS, ZOWE_LOG_INFO, ZISDYN_LOG_DEV_MODE_MSG);
  }

  DynamicPluginData *pluginData = (DynamicPluginData *)&anchor->pluginData;
  int installStatus = RC_ZISDYN_OK;
  zowelog(NULL, LOG_COMP_STCBASE, ZOWE_LOG_DEBUG,
          ZIS_LOG_DEBUG_MSG_ID" Initializing ZIS Dynamic Base plugin "
          "anchor=0x%p pluginData=0x%p\n", anchor, pluginData);

  __stck((void *)&pluginData->initTime);

  CAA *caa = (CAA *) getCAA();
  if (caa == NULL) {
    zowelog(NULL, LOG_COMP_STCBASE, ZOWE_LOG_SEVERE,
            ZISDYN_LOG_INIT_ERROR_MSG" CAA not found");
    installStatus = RC_ZISDYN_ENV_ERROR;
    goto out;
  }

  ZISDynStubVector *stubVector = pluginData->stubVector;
  pluginData->stubVector = NULL;
  if (stubVector != NULL) {
    const char *reasonInvalid = "";
    if (!isStubVectorValid(stubVector, &reasonInvalid)) {
      zowelog(NULL, LOG_COMP_ID_CMS, ZOWE_LOG_WARNING,
              ZISDYN_LOG_STUB_DISCARDED_MSG, stubVector, reasonInvalid);
      zowedump(NULL, LOG_COMP_ID_CMS, ZOWE_LOG_WARNING,
               stubVector, ZISDYN_STUB_VEC_HEADER_SIZE);
      stubVector = NULL;
    } else if (isLPADevMode) {
      deleteStubVector(stubVector);
      zowelog(NULL, LOG_COMP_ID_CMS, ZOWE_LOG_INFO,
              ZISDYN_LOG_STUB_DELETED_MSG, stubVector);
      stubVector = NULL;
    }
  }

  if (stubVector == NULL) {
    stubVector = makeStubVector();
    if (stubVector == NULL) {
      zowelog(NULL, LOG_COMP_ID_CMS, ZOWE_LOG_SEVERE,
              ZISDYN_LOG_INIT_ERROR_MSG" stub vector not created");
      installStatus = RC_ZISDYN_ALLOC_FAILED;
      goto out;
    }
    zowelog(NULL, LOG_COMP_ID_CMS, ZOWE_LOG_INFO,
            ZISDYN_LOG_STUB_CREATED_MSG, stubVector);
  } else {
    initStubVector(stubVector);
    zowelog(NULL, LOG_COMP_ID_CMS, ZOWE_LOG_INFO,
            ZISDYN_LOG_STUB_REUSED_MSG, stubVector);
  }

  zowelog(NULL, LOG_COMP_STCBASE, ZOWE_LOG_DEBUG,
          ZIS_LOG_DEBUG_MSG_ID" stub vector at 0x%p\n", stubVector);
  zowedump(NULL, LOG_COMP_ID_CMS, ZOWE_LOG_DEBUG,
           stubVector, sizeof(ZISDynStubVector));

  zowelog(NULL, LOG_COMP_STCBASE, ZOWE_LOG_DEBUG,
          ZIS_LOG_DEBUG_MSG_ID" ZIS Dynamic in SUP, "
          "ZISServerAnchor at 0x%p caa=0x%p\n", context->zisAnchor, caa);

  RLETask *rleTask = caa->rleTask;
  RLEAnchor *rleAnchor = rleTask->anchor;
  rleAnchor->metalDynamicLinkageVector = stubVector;
  installStatus = installDynamicLinkageVector(context, stubVector);

  pluginData->stubVector = stubVector;

  if (installStatus) {
    zowelog(NULL, LOG_COMP_STCBASE, ZOWE_LOG_WARNING,
            "Could not install dynamic linkage stub vector, rc=%d\n",
            installStatus);
  }

  out:

  if (installStatus == RC_ZISDYN_OK) {
    zowelog(NULL, LOG_COMP_ID_CMS, ZOWE_LOG_INFO, ZISDYN_LOG_STARTED_MSG);
  } else {
    zowelog(NULL, LOG_COMP_ID_CMS, ZOWE_LOG_INFO, ZISDYN_LOG_STARTUP_FAILED_MSG,
            installStatus);
  }

  /* jam this into our favorite zvte */

  return installStatus;
}

static int termZISDynamic(struct ZISContext_tag *context,
                          ZISPlugin *plugin,
                          ZISPluginAnchor *anchor) {

  zowelog(NULL, LOG_COMP_ID_CMS, ZOWE_LOG_INFO, ZISDYN_LOG_TERM_MSG);

  DynamicPluginData *pluginData = (DynamicPluginData *) &anchor->pluginData;
  pluginData->initTime = -1;

  if (zisIsLPADevModeOn(context)) {
    uninstallDynamicLinkageVector(context);
  }

  zowelog(NULL, LOG_COMP_ID_CMS, ZOWE_LOG_INFO, ZISDYN_LOG_TERMED_MSG);

  return RC_ZISDYN_OK;
}

static int handleZISDynamicCommands(struct ZISContext_tag *context,
                                    ZISPlugin *plugin,
                                    ZISPluginAnchor *anchor,
                                    const CMSModifyCommand *command,
                                    CMSModifyCommandStatus *status) {
  
  if (command->commandVerb == NULL) {
    return RC_ZISDYN_OK;
  }

  if (command->argCount != 1) {
    return RC_ZISDYN_OK;
  }

  if (!strcmp(command->commandVerb, "D") ||
      !strcmp(command->commandVerb, "DIS") ||
      !strcmp(command->commandVerb, "DISPLAY")) {

    if (!strcmp(command->args[0], "STATUS")) {

      DynamicPluginData *pluginData = (DynamicPluginData *)&anchor->pluginData;

      zowelog(NULL, LOG_COMP_ID_CMS, ZOWE_LOG_INFO,
              ZISDYN_LOG_CMD_RESP_MSG" Plug-in v%d - anchor = 0x%p, "
                                     "init TOD = %16.16llX\n",
              plugin->pluginVersion, anchor, pluginData->initTime);

      *status = CMS_MODIFY_COMMAND_STATUS_CONSUMED;
    }

  }

  return RC_ZISDYN_OK;
}

static void zisdynUndefinedStub(void) {
  __asm(" ABEND 777,REASON=8 ":::);
}

int zisdynGetStubVersion(void) {
  return ZIS_STUBS_VERSION;
}

void zisdynGetPluginVersion(int *major, int *minor, int *revision) {
  *major = ZISDYN_MAJOR_VERSION;
  *minor = ZISDYN_MINOR_VERSION;
  *revision = ZISDYN_REVISION;
}

ZISPlugin *getPluginDescriptor(void) {

  ZISPluginName pluginName = {.text = ZISDYN_PLUGIN_NAME};
  ZISPluginNickname pluginNickname = {.text = ZISDYN_PLUGIN_NICKNAME};

  ZISPlugin *plugin = zisCreatePlugin(pluginName, pluginNickname,
                                      initZISDynamic,  /* function */
                                      termZISDynamic,
                                      handleZISDynamicCommands,
                                      ZISDYN_PLUGIN_VERSION,
                                      0,  /* service count */
                                      ZIS_PLUGIN_FLAG_LPA);
  if (plugin == NULL) {
    return NULL;
  }

  return plugin;
}

static void assignSlots(void **stubVector);

static void initStubVector(ZISDynStubVector *vector) {

  int wasProblem = supervisorMode(TRUE);
  int oldKey = setKey(0);

  memset(vector, 0, sizeof(ZISDynStubVector));
  memcpy(vector->eyecatcher, ZISDYN_STUB_VEC_EYECATCHER,
         sizeof(vector->eyecatcher));
  vector->version = ZISDYN_STUB_VEC_VERSION;
  vector->key = ZISDYN_STUB_VEC_KEY;
  vector->subpool = ZISDYN_STUB_VEC_SUBPOOL;
  vector->size = sizeof(ZISDynStubVector);
  __stck((void *)&vector->creationTime);

  ASCB *ascb = getASCB();

  char *jobName = getASCBJobname(ascb);
  if (jobName) {
    memcpy(vector->jobName, jobName, sizeof(vector->jobName));
  } else {
    memset(vector->jobName, ' ', sizeof(vector->jobName));
  }

  vector->asid = ascb->ascbasid;

  /* special initialization for slot 0 and all unused slots */
  for (int s = 0; s < sizeof(vector->slots) / sizeof(vector->slots[0]); s++) {
    vector->slots[s] = (void *) zisdynUndefinedStub;
  }

  vector->stubsVersion = ZIS_STUBS_VERSION;
  assignSlots(vector->slots);

  setKey(oldKey);
  if (wasProblem) {
    supervisorMode(FALSE);
  }

}

static void assignSlots(void **stubVector) {
  /* generated code */
#include "../../c/zis/stubinit.c"
}

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/
