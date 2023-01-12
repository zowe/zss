

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/

#include <metal/metal.h>
#include <metal/stddef.h>
#include <metal/string.h>

#include "zowetypes.h"
#include "collections.h"
#include "crossmemory.h"
#include "zis/plugin.h"
#include "zis/server.h"
#include "zis/service.h"

#include "services/echoservice.h"

typedef struct EchoPluginData_tag {
  int pidInt;
  int signalInt;
} EchoPluginData;

static int init(struct ZISContext_tag *context,
                ZISPlugin *plugin,
                ZISPluginAnchor *anchor) {

  EchoPluginData *pluginData = (EchoPluginData *)&anchor->pluginData;

  pluginData->pidInt = -1;
  pluginData->signalInt = -1;

  return RC_ZIS_PLUGIN_OK;
}

static int term(struct ZISContext_tag *context,
                ZISPlugin *plugin,
                ZISPluginAnchor *anchor) {

  EchoPluginData *pluginData = (EchoPluginData *)&anchor->pluginData;

  pluginData->pidInt = -1;
  pluginData->signalInt = -1;

  return RC_ZIS_PLUGIN_OK;
}

static int handleCommands(struct ZISContext_tag *context,
                          ZISPlugin *plugin,
                          ZISPluginAnchor *anchor,
                          const CMSModifyCommand *command,
                          CMSModifyCommandStatus *status) {

  if (command->commandVerb == NULL) {
    return RC_ZIS_PLUGIN_OK;
  }

  if (command->argCount != 1) {
    return RC_ZIS_PLUGIN_OK;
  }

  if (!strcmp(command->commandVerb, "D") ||
      !strcmp(command->commandVerb, "DIS") ||
      !strcmp(command->commandVerb, "DISPLAY")) {

    if (!strcmp(command->args[0], "STATUS")) {

      EchoPluginData *pluginData = (EchoPluginData *)&anchor->pluginData;

      cmsPrintf(&context->cmServer->name,
                "Echo plug-in v%d - anchor = 0x%p, init TOD = %16.16llX\n",
                plugin->pluginVersion, anchor, pluginData->pidInt);

      *status = CMS_MODIFY_COMMAND_STATUS_CONSUMED;
    }

  }

  return RC_ZIS_PLUGIN_OK;
}

/*
ZISPlugin *getPluginDescriptor() {

  ZISPluginName pluginName = {.text = "ZWE_ZTOP        "};
  ZISPluginNickname pluginNickname = {.text = "ZTOP"};
  ZISPlugin *plugin = zisCreatePlugin(pluginName, pluginNickname,
                                      init, term, handleCommands,
                                      1, 1, ZIS_PLUGIN_FLAG_LPA);

  wtoPrintf("here!\n");

  if (plugin == NULL) {
    return NULL;
  }

  int rc = RC_ZIS_PLUGIN_OK;

  ZISServiceName serviceName = {.text = "KILSRV"};
  ZISService service = zisCreateCurrentPrimaryService(serviceName, NULL, NULL,
                                                       serveKill,
                                                       1);

  rc = zisPluginAddService(plugin, service);
  if (rc != RC_ZIS_PLUGIN_OK) {
    return NULL;
  }

  return plugin;
}
*/

#define ZTOP_PLUGIN_NAME           "ZTOP_ZIS        "
#define ZTOP_PLUGIN_NICKNAME       "ZTOP"
#define ZTOP_PLUGIN_VERSION        1 
#define ZTOP_PLUGIN_SERVICE_COUNT  1

#define ZTOP_PLUGIN_AUTH           "ZTOP_KILL       "

ZISPlugin *getPluginDescriptor() {

  ZISPluginName pluginName = { .text = ZTOP_PLUGIN_NAME };
  ZISPluginNickname pluginNickname = { .text = ZTOP_PLUGIN_NICKNAME };
  ZISPlugin *plugin = zisCreatePlugin(pluginName,
                                      pluginNickname,
                                      init,
                                      term,
                                      handleCommands,
                                      ZTOP_PLUGIN_VERSION,
                                      ZTOP_PLUGIN_SERVICE_COUNT,
                                      ZIS_PLUGIN_FLAG_LPA);

  if(plugin == NULL){
    return NULL;
  }

  int rc = RC_ZIS_PLUGIN_OK;

  ZISServiceName ztopAuthServiceName = { .text = ZTOP_PLUGIN_AUTH };
  ZISService ztopAuthService =
      zisCreateCurrentPrimaryService(ztopAuthServiceName,
                                     NULL,
                                     NULL,
                                     serveKill,
                                     1);

  rc = zisPluginAddService(plugin, ztopAuthService);
  if (rc != RC_ZIS_PLUGIN_OK) {
    return NULL;
  }


  return plugin;
}



/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/


