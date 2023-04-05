

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/

#ifndef ZIS_PLUGIN_H_
#define ZIS_PLUGIN_H_

#include "zowetypes.h"
#include "crossmemory.h"

#include "zis/service.h"

struct ZISContext_tag;
typedef struct ZISPlugin_tag ZISPlugin;
typedef struct ZISPluginAnchor_tag ZISPluginAnchor;
typedef struct ZISPluginData_tag ZISPluginData;

typedef int (ZISPuginInitFunction)(struct ZISContext_tag *context,
                                   ZISPlugin *plugin,
                                   ZISPluginAnchor *anchor);
typedef int (ZISPuginTermFunction)(struct ZISContext_tag *context,
                                   ZISPlugin *plugin,
                                   ZISPluginAnchor *anchor);
typedef int (ZISPuginModifyCommandFunction)(struct ZISContext_tag *context,
                                            ZISPlugin *plugin,
                                            ZISPluginAnchor *anchor,
                                            const CMSModifyCommand *command,
                                            CMSModifyCommandStatus *status);

ZOWE_PRAGMA_PACK

typedef struct ZISPluginName_tag {
  char text[16];
} ZISPluginName;


typedef struct ZISPluginNickname_tag {
  char text[4];
} ZISPluginNickname;

struct ZISPluginData_tag {
  char data[64];
};

struct ZISPluginAnchor_tag {

  char eyecatcher[8];
#define ZIS_PLUGIN_ANCHOR_EYECATCHER  "ZISPLGAN"
  int version;
#define ZIS_PLUGIN_ANCHOR_VERSION 1
  int key : 8;
  unsigned int subpool : 8;
  unsigned short size;
  int flags;
#define ZIS_PLUGIN_ANCHOR_FLAG_LPA 0x00000001
  int state;

  ZISPluginName name;
  unsigned int pluginVersion;
  char reserved0[4];
  PAD_LONG(0, struct ZISPluginAnchor_tag *next);

  PAD_LONG(1, struct ZISServiceAnchor_tag *firstService);

  LPMEA moduleInfo;

  ZISPluginData pluginData;

  char reserved1[88];

};

struct ZISPlugin_tag {

  char eyecatcher[8];
#define ZIS_PLUGIN_EYECATCHER "ZISPLUGN"
  int version;
#define ZIS_PLUGIN_VERSION  1
  unsigned int size;
  int flags;
#define ZIS_PLUGIN_FLAG_NONE    0x00000000
#define ZIS_PLUGIN_FLAG_LPA     0x00000001
  unsigned int maxServiceCount;

  /* These are used by the server */
  PAD_LONG(0, struct ZISPlugin_tag *next);
  PAD_LONG(1, ZISPluginAnchor *anchor);

  ZISPluginName name;
  ZISPluginNickname nickname;

  char reserved0[4];

  PAD_LONG(2, ZISPuginInitFunction *init);
  PAD_LONG(3, ZISPuginTermFunction *term);
  PAD_LONG(4, ZISPuginModifyCommandFunction *handleCommand);

  char reserved1[932];

  unsigned int pluginVersion;
  unsigned int serviceCount;
  ZISService services[0];

};

ZOWE_PRAGMA_PACK_RESET

typedef ZISPlugin *(ZISPluginDescriptorFunction)();

#pragma map(zisCreatePlugin, "ZISCPLGN")
#pragma map(zisDestroyPlugin, "ZISDSTPL")
#pragma map(zisPluginAddService, "ZISPLGAS")
#pragma map(zisCreatePluginAnchor, "ZISPLGCA")
#pragma map(zisRemovePluginAnchor, "ZISPLGRM")

/**
 * @brief Create a plugin descriptor.
 * @param[in] name The plugin name (unique within ZIS).
 * @param[in] nickname The plugin nickname unique within ZIS.
 * @param[in] initFunction The init callback function invoked at ZIS startup.
 * @param[in] termFunction The term callback function invoked at ZIS termination.
 * @param[in] commandFunction The command handler function invoked when a user
 * issues a modify command using the plugin's nickname as target.
 * @param[in] version The plugin version (when bumped forces ZIS to load the new
 * version of the plugin module to LPA if it's an LPA plugin).
 * @param[in] serviceCount The number of services in this plugin.
 * @param[in] flags Various flags (use the @c ZIS_PLUGIN_FLAG_xxxx values).
 * @return The descriptor address if success, @c NULL in case of an allocation
 * error.
 */
ZISPlugin *zisCreatePlugin(ZISPluginName name,
                           ZISPluginNickname nickname,
                           ZISPuginInitFunction *initFunction,
                           ZISPuginInitFunction *termFunction,
                           ZISPuginModifyCommandFunction *commandFunction,
                           unsigned int version,
                           unsigned int serviceCount,
                           int flags);

/**
 * @brief Remove the plugin descriptor (releases its memory).
 * @param[in,out] plugin The plugin descriptor to be removed.
 */
void zisDestroyPlugin(ZISPlugin *plugin);

/**
 * @brief Add a service to a plugin.
 * @param[in,out] plugin The plugin descriptor to be used.
 * @param[in] service The service to be added.
 * @return
 * - @c RC_ZIS_PLUGIN_OK in case of success <br>
 * - @c RC_ZIS_PLUGIN_INCOMPATIBLE_SEVICE if a PC-cp service is used with
 * a non-LPA plugin <br>
 * - @c RC_ZIS_PLUGIN_BAD_SERVICE_NAME if the service name is invalid <br>
 */
int zisPluginAddService(ZISPlugin *plugin, ZISService service);

ZISPluginAnchor *zisCreatePluginAnchor();
void zisRemovePluginAnchor(ZISPluginAnchor **anchor);

#define RC_ZIS_PLUGIN_OK                  0
#define RC_ZIS_PLUGIN_COMMAND_NOT_HADNLED 2
#define RC_ZIS_PLUGIN_SEVICE_TABLE_FULL   8
#define RC_ZIS_PLUGIN_INCOMPATIBLE_SEVICE 9
#define RC_ZIS_PLUGIN_BAD_SERVICE_NAME    10

#endif /* ZIS_PLUGIN_H_ */


/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/

