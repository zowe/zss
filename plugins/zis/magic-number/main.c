

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

#include "services/getservice.h"

static int handleCommands(struct ZISContext_tag *context,
                          ZISPlugin *plugin,
                          ZISPluginAnchor *anchor,
                          const CMSModifyCommand *command,
                          CMSModifyCommandStatus *status) {

  if (command->commandVerb == NULL) {
    return RC_ZIS_PLUGIN_OK;
  }

  if (!strcmp(command->commandVerb, "SHOW")) {

    cmsPrintf(&context->cmServer->name,
              "I can't show secret magic data, please use the test program\n");

    *status = CMS_MODIFY_COMMAND_STATUS_CONSUMED;

  }

  return RC_ZIS_PLUGIN_OK;
}

ZISPlugin *getPluginDescriptor() {

  ZISPluginName pluginName = {.text = "MAGICNUMBER     "};
  ZISPluginNickname pluginNickname = {.text = "M   "};
  ZISPlugin *plugin = zisCreatePlugin(pluginName, pluginNickname,
                                      NULL, NULL, handleCommands,
                                      3, 1, 0);
  if (plugin == NULL) {
    return NULL;
  }

  ZISServiceName serviceName1 = {.text = "GET             "};
  ZISService service1 = zisCreateSpaceSwitchService(serviceName1,
                                                    initMagicNumberService,
                                                    termMagicNumberService,
                                                    serveMagicNumber);

  zisPluginAddService(plugin, service1);

  return plugin;
}


/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/

