

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/

#include <metal/metal.h>
#include <metal/ctype.h>
#include <metal/stddef.h>
#include <metal/stdlib.h>
#include <metal/string.h>

#include "zowetypes.h"
#include "alloc.h"
#include "cmutils.h"
#include "crossmemory.h"
#include "zis/plugin.h"
#include "zis/service.h"

static bool isPluginNameValid(const ZISPluginName *name) {

  for (int i = 0; i < sizeof(name->text); i++) {
    if (!isprint(name->text[i])) {
      return false;
    }
  }

  return true;
}

static bool isServiceNameValid(const ZISServiceName *name) {

  for (int i = 0; i < sizeof(name->text); i++) {
    if (!isprint(name->text[i])) {
      return false;
    }
  }

  return true;
}

static bool isNicknameValid(const ZISPluginNickname *nickname) {

  bool spaceFound = false;

  for (int i = 0; i < sizeof(nickname->text); i++) {

    char c = nickname->text[i];

    if (!isprint(c)) {
      return false;
    }

    if (isgraph(c)) {
      if (spaceFound) {
        return false;
      }
    }

    if (isspace(c)) {
      if (i == 0) {
        return false;
      }
      spaceFound = true;
    }

  }

  return true;
}


ZISPlugin *zisCreatePlugin(ZISPluginName name,
                           ZISPluginNickname nickname,
                           ZISPuginInitFunction *initFunction,
                           ZISPuginInitFunction *termFunction,
                           ZISPuginModifyCommandFunction *commandFunction,
                           unsigned int version,
                           unsigned int serviceCount,
                           int flags) {

  if (!isPluginNameValid(&name)) {
    return false;
  }

  if (!isNicknameValid(&nickname)) {
    return false;
  }

  unsigned int requiredSize = sizeof(ZISPlugin) +
      sizeof(ZISService) * serviceCount;

  ZISPlugin *plugin = (ZISPlugin *)safeMalloc(requiredSize, "ZISPlugin");
  if (plugin == NULL) {
    return NULL;
  }

  memset(plugin, 0, requiredSize);
  memcpy(plugin->eyecatcher, ZIS_PLUGIN_EYECATCHER, sizeof(plugin->eyecatcher));
  plugin->version = ZIS_PLUGIN_VERSION;
  plugin->size = requiredSize;
  plugin->flags = flags;
  plugin->maxServiceCount = serviceCount;
  plugin->name = name;
  plugin->nickname = nickname;

  plugin->init = initFunction;
  plugin->term = termFunction;
  plugin->handleCommand = commandFunction;

  plugin->pluginVersion = version;

  return plugin;
}



void zisDestroyPlugin(ZISPlugin *plugin) {
  safeFree((char *)plugin, plugin->size);
  plugin = NULL;
}

int zisPluginAddService(ZISPlugin *plugin, ZISService service) {

  if (plugin->maxServiceCount == plugin->serviceCount) {
    return RC_ZIS_PLUGIN_SEVICE_TABLE_FULL;
  }

  bool isPCCPService = !(service.flags & ZIS_SERVICE_ANCHOR_FLAG_SPACE_SWITCH);
  bool isLPAPlugin = plugin->flags & ZIS_PLUGIN_ANCHOR_FLAG_LPA;

  if (isPCCPService && !isLPAPlugin) {
    return RC_ZIS_PLUGIN_INCOMPATIBLE_SEVICE;
  }

  if (!isServiceNameValid(&service.name)) {
    return RC_ZIS_PLUGIN_BAD_SERVICE_NAME;
  }

  plugin->services[plugin->serviceCount++] = service;

  return RC_ZIS_PLUGIN_OK;
}

ZISPluginAnchor *zisCreatePluginAnchor(const ZISPlugin *plugin) {

  unsigned int size = sizeof(ZISPluginAnchor);
  int subpool = CROSS_MEMORY_SERVER_SUBPOOL;
  int key = CROSS_MEMORY_SERVER_KEY;

  ZISPluginAnchor *anchor = cmAlloc(size, subpool, key);
  if (anchor == NULL) {
    return NULL;
  }

  memset(anchor, 0, size);
  memcpy(anchor->eyecatcher, ZIS_PLUGIN_ANCHOR_EYECATCHER,
         sizeof(anchor->eyecatcher));
  anchor->version = ZIS_PLUGIN_ANCHOR_VERSION;
  anchor->key = key;
  anchor->subpool = subpool;
  anchor->size = size;

  anchor->name = plugin->name;
  anchor->pluginVersion = plugin->pluginVersion;

  return anchor;
}

void zisRemovePluginAnchor(ZISPluginAnchor **anchor) {

  unsigned int size = sizeof(ZISPluginAnchor);
  int subpool = CROSS_MEMORY_SERVER_SUBPOOL;
  int key = CROSS_MEMORY_SERVER_KEY;

  cmFree2((void **)anchor, size, subpool, key);

}


/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/

