/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/

#ifndef PLUGINS_ZIS_SAMPLE_PLUGIN_H
#define PLUGINS_ZIS_SAMPLE_PLUGIN_H

// these are the codes returned by the "get CR" service
// the success return code must be equal to @c RC_ZIS_SRVC_OK and any other
// return codes must have values larger than @c ZIS_MAX_GEN_SRVC_RC
#define RC_GETCR_OK                 RC_ZIS_SRVC_OK
#define RC_GETCR_BAD_EYECATCHER     (ZIS_MAX_GEN_SRVC_RC + 1)
#define RC_GETCR_BAD_VERSION        (ZIS_MAX_GEN_SRVC_RC + 2)

#define SAMPLE_PLUGIN_NAME        "SAMPLE          "
#define SAMPLE_PLUGIN_NICKNAME    "SMPL"

#define GETCR_SERVICE_NAME        "GET_CTRL_REGS   "

typedef __packed struct ControlRegisters_tag {
  uint64_t reg[16];
} ControlRegisters;

/**
 * This struct describes the parameter list of the "get CR" service.
 */
typedef __packed struct GetCRServiceParm_tag {
#define GETCR_SERVICE_PARM_EYECATCHER "GETCRPRM"
#define GETCR_SERVICE_PARM_VERSION 1
  char eyecatcher[8];
  uint8_t version;
  ControlRegisters result;
} GetCRServiceParm;

#endif //PLUGINS_ZIS_SAMPLE_PLUGIN_H

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  SPDX-License-Identifier: EPL-2.0
  Copyright Contributors to the Zowe Project.
*/
