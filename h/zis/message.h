

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

#ifndef ZIS_MSG_H_
#define ZIS_MSG_H_

#include "zis/server.h"
#include "zis/version.h"

#ifndef ZIS_MSG_PRFX
#define ZIS_MSG_PRFX CMS_MSG_PRFX
#endif

/* Use these when defining new debug messages in the ZIS code. */
#define ZIS_LOG_DEBUG_MSG_ID                    ZIS_MSG_PRFX"0098I"
#define ZIS_LOG_DEBUG_MSG_TEXT                  ""
#define ZIS_LOG_DEBUG_DEV_MSG                   ZIS_LOG_DEBUG_MSG_ID" "ZIS_LOG_DEBUG_DEV_MSG

#define ZIS_LOG_DUMP_MSG_ID                     ZIS_MSG_PRFX"0099I" /* TODO will it be picked up? */

/* keep this in synch with the default messages IDs from crossmemory.h */
#define ZIS_LOG_STARTUP_MSG_ID                  ZIS_MSG_PRFX"0001I"
 #ifdef CROSS_MEMORY_SERVER_DEBUG
  #define ZIS_LOG_STARTUP_MSG_TEXT              "ZSS Cross-Memory Server starting in debug mode, version is "ZIS_VERSION"\n"
 #else
  #define ZIS_LOG_STARTUP_MSG_TEXT              "ZSS Cross-Memory Server starting, version is "ZIS_VERSION"\n"
 #endif
#define ZIS_LOG_STARTUP_MSG                     ZIS_LOG_STARTUP_MSG_ID" "ZIS_LOG_STARTUP_MSG_TEXT

#define ZIS_LOG_INPUT_PARM_MSG_ID               ZIS_MSG_PRFX"0002I"
#define ZIS_LOG_INPUT_PARM_MSG_TEXT             "Input parameters at 0x%p:"
#define ZIS_LOG_INPUT_PARM_MSG                  ZIS_LOG_INPUT_PARM_MSG_ID" "ZIS_LOG_INPUT_PARM_MSG_TEXT

#define ZIS_LOG_CXMS_NO_NAME_MSG_ID             ZIS_MSG_PRFX"0003I"
#define ZIS_LOG_CXMS_NO_NAME_MSG_TEXT           "Server name not provided, default value \'%16.16s\' will be used"
#define ZIS_LOG_CXMS_NO_NAME_MSG                ZIS_LOG_CXMS_NO_NAME_MSG_ID" "ZIS_LOG_CXMS_NO_NAME_MSG_TEXT

#define ZIS_LOG_CXMS_NAME_MSG_ID                ZIS_MSG_PRFX"0004I"
#define ZIS_LOG_CXMS_NAME_MSG_TEXT              "Server name is \'%16.16s\'"
#define ZIS_LOG_CXMS_NAME_MSG                   ZIS_LOG_CXMS_NAME_MSG_ID" "ZIS_LOG_CXMS_NAME_MSG_TEXT

#define ZIS_LOG_CXMS_NOT_CREATED_MSG_ID         ZIS_MSG_PRFX"0005E"
#define ZIS_LOG_CXMS_NOT_CREATED_MSG_TEXT       "ZSS Cross-Memory server not created, RSN = %d"
#define ZIS_LOG_CXMS_NOT_CREATED_MSG            ZIS_LOG_CXMS_NOT_CREATED_MSG_ID" "ZIS_LOG_CXMS_NOT_CREATED_MSG_TEXT

#define ZIS_LOG_CXMS_RES_ALLOC_FAILED_MSG_ID    ZIS_MSG_PRFX"0006E"
#define ZIS_LOG_CXMS_RES_ALLOC_FAILED_MSG_TEXT  "ZSS Cross-Memory server resource not allocated (%s)"
#define ZIS_LOG_CXMS_RES_ALLOC_FAILED_MSG        ZIS_LOG_CXMS_RES_ALLOC_FAILED_MSG_ID" "ZIS_LOG_CXMS_RES_ALLOC_FAILED_MSG_TEXT

#define ZIS_LOG_CXMS_BAD_PMEM_SUFFIX_MSG_ID     ZIS_MSG_PRFX"0007E"
#define ZIS_LOG_CXMS_BAD_PMEM_SUFFIX_MSG_TEXT   "ZSS Cross-Memory server PARMLIB member suffix is incorrect - \'%s\'"
#define ZIS_LOG_CXMS_BAD_PMEM_SUFFIX_MSG         ZIS_LOG_CXMS_BAD_PMEM_SUFFIX_MSG_ID" "ZIS_LOG_CXMS_BAD_PMEM_SUFFIX_MSG_TEXT

#define ZIS_LOG_CXMS_CFG_NOT_READ_MSG_ID        ZIS_MSG_PRFX"0008E"
#define ZIS_LOG_CXMS_CFG_NOT_READ_MSG_TEXT      "ZSS Cross-Memory server configuration not read, member = \'%8.8s\', RC = %d (%d, %d)"
#define ZIS_LOG_CXMS_CFG_NOT_READ_MSG            ZIS_LOG_CXMS_CFG_NOT_READ_MSG_ID" "ZIS_LOG_CXMS_CFG_NOT_READ_MSG_TEXT

#define ZIS_LOG_CXMS_CFG_MISSING_MSG_ID         ZIS_MSG_PRFX"0009E"
#define ZIS_LOG_CXMS_CFG_MISSING_MSG_TEXT       "ZSS Cross-Memory server configuration not found, member = \'%8.8s\', RC = %d"
#define ZIS_LOG_CXMS_CFG_MISSING_MSG             ZIS_LOG_CXMS_CFG_MISSING_MSG_ID" "ZIS_LOG_CXMS_CFG_MISSING_MSG_TEXT

#define ZIS_LOG_CXMS_CFG_NOT_LOADED_MSG_ID      ZIS_MSG_PRFX"0010E"
#define ZIS_LOG_CXMS_CFG_NOT_LOADED_MSG_TEXT    "ZSS Cross-Memory server configuration not loaded, RC = %d, RSN = %d"
#define ZIS_LOG_CXMS_CFG_NOT_LOADED_MSG         ZIS_LOG_CXMS_CFG_NOT_LOADED_MSG_ID" "ZIS_LOG_CXMS_CFG_NOT_LOADED_MSG_TEXT

#define ZIS_LOG_CXMS_NOT_STARTED_MSG_ID         ZIS_MSG_PRFX"0011E"
#define ZIS_LOG_CXMS_NOT_STARTED_MSG_TEXT       "ZSS Cross-Memory server not started, RC = %d"
#define ZIS_LOG_CXMS_NOT_STARTED_MSG            ZIS_LOG_CXMS_NOT_STARTED_MSG_ID" "ZIS_LOG_CXMS_NOT_STARTED_MSG_TEXT

#define ZIS_LOG_CXMS_TERM_OK_MSG_ID             ZIS_MSG_PRFX"0012I"
#define ZIS_LOG_CXMS_TERM_OK_MSG_TEXT           "ZSS Cross-Memory Server terminated"
#define ZIS_LOG_CXMS_TERM_OK_MSG                ZIS_LOG_CXMS_TERM_OK_MSG_ID" "ZIS_LOG_CXMS_TERM_OK_MSG_TEXT

#define ZIS_LOG_CXMS_TERM_FAILURE_MSG_ID        ZIS_MSG_PRFX"0013E"
#define ZIS_LOG_CXMS_TERM_FAILURE_MSG_TEXT      "ZSS Cross-Memory Server terminated due to an error, status = %d"
#define ZIS_LOG_CXMS_TERM_FAILURE_MSG           ZIS_LOG_CXMS_TERM_FAILURE_MSG_ID" "ZIS_LOG_CXMS_TERM_FAILURE_MSG_TEXT

#define ZIS_LOG_CONFIG_FAILURE_MSG_ID           ZIS_MSG_PRFX"0014E"
#define ZIS_LOG_CONFIG_FAILURE_MSG_TEXT         "Fatal config error - %s, RC = %d"
#define ZIS_LOG_CONFIG_FAILURE_MSG              ZIS_LOG_CONFIG_FAILURE_MSG_ID" "ZIS_LOG_CONFIG_FAILURE_MSG_TEXT

#define ZIS_LOG_LPA_FAILURE_MSG_ID              ZIS_MSG_PRFX"0015E"
#define ZIS_LOG_LPA_FAILURE_MSG_TEXT            "LPA %s failed for module %-8.8s, RC = %d, RSN = %d"
#define ZIS_LOG_LPA_FAILURE_MSG                 ZIS_LOG_LPA_FAILURE_MSG_ID" "ZIS_LOG_LPA_FAILURE_MSG_TEXT

#define ZIS_LOG_SERVICE_ADDED_MSG_ID            ZIS_MSG_PRFX"0016I"
#define ZIS_LOG_SERVICE_ADDED_MSG_TEXT          "Service '%-16.16s':'%-16.16s' v%d has been added"
#define ZIS_LOG_SERVICE_ADDED_MSG               ZIS_LOG_SERVICE_ADDED_MSG_ID" "ZIS_LOG_SERVICE_ADDED_MSG_TEXT

#define ZIS_LOG_PLUGIN_FAILURE_MSG_ID           ZIS_MSG_PRFX"0017W"
#define ZIS_LOG_PLUGIN_FAILURE_MSG_TEXT         "Plug-in '%-16.16s' failure -"
#define ZIS_LOG_PLUGIN_FAILURE_MSG_PREFIX       ZIS_LOG_PLUGIN_FAILURE_MSG_ID" "ZIS_LOG_PLUGIN_FAILURE_MSG_TEXT

#define ZIS_LOG_PLUGIN_VER_MISMATCH_MSG_ID      ZIS_MSG_PRFX"0018W"
#define ZIS_LOG_PLUGIN_VER_MISMATCH_MSG_TEXT    "Plug-in '%-16.16s' version %u doesn't match anchor version %u, LPA module discarded"
#define ZIS_LOG_PLUGIN_VER_MISMATCH_MSG         ZIS_LOG_PLUGIN_VER_MISMATCH_MSG_ID" "ZIS_LOG_PLUGIN_VER_MISMATCH_MSG_TEXT

#endif /* ZIS_MSG_H_ */


/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

