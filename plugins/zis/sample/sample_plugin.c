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
#include "cmutils.h"
#include "zis/plugin.h"

#include "sample_plugin.h"

// forward declaration of the "serve" function
static int serveControlRegisters(const CrossMemoryServerGlobalArea *ga,
                                 ZISServiceAnchor *anchor,
                                 ZISServiceData *data,
                                 void *serviceParmList);

ZISPlugin *getPluginDescriptor(void) {

  // this call will create a plugin data structure with the provided plugin 
  // name and nickname
  ZISPlugin *plugin = zisCreatePlugin(
      (ZISPluginName) {SAMPLE_PLUGIN_NAME},
      (ZISPluginNickname) {SAMPLE_PLUGIN_NICKNAME},
      NULL, // plugin "init" function
      NULL, // plugin "term" function
      NULL, // plugin "modify command" handler
      1, // plugin version
      1, // number of services
      ZIS_PLUGIN_FLAG_NONE // plugin flags
  );
  if (plugin == NULL) {
    return NULL;
  }

  // add a single service to our plugin
  ZISService service = zisCreateService(
      (ZISServiceName) {GETCR_SERVICE_NAME},
      ZIS_SERVICE_FLAG_SPACE_SWITCH, // service flags
      NULL, // service "init" function
      NULL, // service "term" function
      serveControlRegisters, // service "serve" function
      1 // service version
  );
  int addRC = zisPluginAddService(plugin, service);
  if (addRC != RC_ZIS_PLUGIN_OK) {
    zisDestroyPlugin(plugin);
    return NULL;
  }

  return plugin;
}

//  forward declaration of the helper functions
static int verifyGetCRServiceParmList(const GetCRServiceParm *parmList);
void getControlRegisters(ControlRegisters *result);

/**
 * This is the "serve" function of the "get CR" service. It will be invoked by
 * the cross-memory server once the respective service is called.
 * @param[in] ga the cross-memory server's global area.
 * @param[in,out] anchor the ZIS anchor provided by the cross-memory server.
 * @param[in,out] data the service data provided by the cross-memory server.
 * @param[in,out] serviceParmList the parameter list from the caller.
 * @return the service's return code.
 */
static int serveControlRegisters(const CrossMemoryServerGlobalArea *ga,
                                 ZISServiceAnchor *anchor,
                                 ZISServiceData *data,
                                 void *serviceParmList) {

  // since the caller's parameter list is in a different address space, we
  // have to copy it to a local variable using a special copy function
  GetCRServiceParm localParmList;
  cmCopyFromSecondaryWithCallerKey(&localParmList, serviceParmList,
                                   sizeof(localParmList));

  // it's good practice to validate the parameter list
  // see all the details in the helper function
  int verifyRC = verifyGetCRServiceParmList(&localParmList);
  if (verifyRC != RC_GETCR_OK) {
    return verifyRC;
  }

  // now, let's perform a privileged operation and extract the control registers
  getControlRegisters(&localParmList.result);

  // finally, we need to return the result to the caller by copying the updated
  // parameter list to the caller's address space
  cmCopyToSecondaryWithCallerKey(serviceParmList, &localParmList,
                                 sizeof(localParmList));

  return RC_GETCR_OK;
}

/**
 * Verify the "get CR" service's parameter list.
 * @param parmList the parameter list to be verified.
 * @return @c RC_GETCR_OK in case of success and the corresponding
 * RC_GETCR_XXXX value in case of failure.
 */
static int verifyGetCRServiceParmList(const GetCRServiceParm *parmList) {
  if (memcmp(parmList->eyecatcher, GETCR_SERVICE_PARM_EYECATCHER,
             sizeof(parmList->eyecatcher))) {
    return RC_GETCR_BAD_EYECATCHER;
  }
  if (parmList->version != GETCR_SERVICE_PARM_VERSION) {
    return RC_GETCR_BAD_VERSION;
  }
  return RC_GETCR_OK;
}

/**
 * Extract the content of the control registers.
 * @param[out] result the 64-bit value from CR.
 */
void getControlRegisters(ControlRegisters *resultRegisters) {
  ControlRegisters regs __attribute__((aligned(8))) = {0};
  __asm(
      "         STCTG  0,15,%[result]                                          \n"
      : [result]"=m"(regs)
      :
      :
  );
  *resultRegisters = regs;
}

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/