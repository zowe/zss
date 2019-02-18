

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
  uint64 initTime;
} EchoPluginData;

static int init(struct ZISContext_tag *context,
                ZISPlugin *plugin,
                ZISPluginAnchor *anchor) {

  EchoPluginData *pluginData = (EchoPluginData *)&anchor->pluginData;

  __asm(" STCK 0(%0) " : : "r"(&pluginData->initTime));

  return RC_ZIS_PLUGIN_OK;
}

static int term(struct ZISContext_tag *context,
                ZISPlugin *plugin,
                ZISPluginAnchor *anchor) {

  EchoPluginData *pluginData = (EchoPluginData *)&anchor->pluginData;

  pluginData->initTime = -1;

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

      /* We can use zowelog but I don't want to link with a lot of unnecessary
       * object files.  */
      cmsPrintf(&context->cmServer->name,
                "Echo plug-in v%d - anchor = 0x%p, init TOD = %16.16llX\n",
                plugin->pluginVersion, anchor, pluginData->initTime);

      *status = CMS_MODIFY_COMMAND_STATUS_CONSUMED;
    }

  }

  return RC_ZIS_PLUGIN_OK;
}

ZISPlugin *getPluginDescriptor() {

  ZISPluginName pluginName = {.text = "ECHO            "};
  ZISPluginNickname pluginNickname = {.text = "ECHO"};
  ZISPlugin *plugin = zisCreatePlugin(pluginName, pluginNickname,
                                      init, term, handleCommands,
                                      9, 2, ZIS_PLUGIN_FLAG_LPA);
  if (plugin == NULL) {
    return NULL;
  }

  ZISServiceName serviceName1 = {.text = "ECHO-MESSAGE    "};
  ZISService service1 = zisCreateSpaceSwitchService(serviceName1, NULL, NULL,
                                                    serveEchoedMessage);

  ZISServiceName serviceName2 = {.text = "ECHO-REV-MESSAGE"};
  ZISService service2 = zisCreateCurrentPrimaryService(serviceName2, NULL, NULL,
                                                       serveReversedEchoedMessage);

  zisPluginAddService(plugin, service1);
  zisPluginAddService(plugin, service2);

  return plugin;
}


/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/

