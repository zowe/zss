

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
#define ZIS_LOG_DEBUG_DEV_MSG                   ZIS_LOG_DEBUG_MSG_ID" "ZIS_LOG_DEBUG_MSG_TEXT

#define ZIS_LOG_DUMP_MSG_ID                     ZIS_MSG_PRFX"0099I" /* TODO will it be picked up? */

/* keep this in sync with the default messages IDs from crossmemory.h and zssLogging.h */
#define ZIS_LOG_STARTUP_MSG_ID                  ZIS_MSG_PRFX"0001I"
 #ifdef CROSS_MEMORY_SERVER_DEBUG
  #define ZIS_LOG_STARTUP_MSG_TEXT              "ZSS Cross-Memory Server starting in debug mode, version is %d.%d.%d+%d\n"
 #else
  #define ZIS_LOG_STARTUP_MSG_TEXT              "ZSS Cross-Memory Server starting, version is %d.%d.%d+%d\n"
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
#define ZIS_LOG_SERVICE_ADDED_MSG_TEXT          "Service '%-16.16s':'%-16.16s' v%d.%d has been added"
#define ZIS_LOG_SERVICE_ADDED_MSG               ZIS_LOG_SERVICE_ADDED_MSG_ID" "ZIS_LOG_SERVICE_ADDED_MSG_TEXT

#define ZIS_LOG_PLUGIN_FAILURE_MSG_ID           ZIS_MSG_PRFX"0017W"
#define ZIS_LOG_PLUGIN_FAILURE_MSG_TEXT         "Plug-in '%-16.16s' failure -"
#define ZIS_LOG_PLUGIN_FAILURE_MSG_PREFIX       ZIS_LOG_PLUGIN_FAILURE_MSG_ID" "ZIS_LOG_PLUGIN_FAILURE_MSG_TEXT

#define ZIS_LOG_PLUGIN_VER_MISMATCH_MSG_ID      ZIS_MSG_PRFX"0018W"
#define ZIS_LOG_PLUGIN_VER_MISMATCH_MSG_TEXT    "Plug-in '%-16.16s' version %u doesn't match anchor version %u, LPA module discarded"
#define ZIS_LOG_PLUGIN_VER_MISMATCH_MSG         ZIS_LOG_PLUGIN_VER_MISMATCH_MSG_ID" "ZIS_LOG_PLUGIN_VER_MISMATCH_MSG_TEXT

#define ZIS_LOG_BAD_CONFIG_PARM_MSG_ID          ZIS_MSG_PRFX"0019W"
#define ZIS_LOG_BAD_CONFIG_PARM_MSG_TEXT        "Parameter \'%s\' has an invalid value"
#define ZIS_LOG_BAD_CONFIG_PARM_MSG             ZIS_LOG_BAD_CONFIG_PARM_MSG_ID" "ZIS_LOG_BAD_CONFIG_PARM_MSG_TEXT

#define ZIS_LOG_CXMS_PMEM_NAME_FAILED_MSG_ID    ZIS_MSG_PRFX"0020E"
#define ZIS_LOG_CXMS_PMEM_NAME_FAILED_MSG_TEXT  "ZSS Cross-Memory server PARMLIB member name not determined, RC = %d"
#define ZIS_LOG_CXMS_PMEM_NAME_FAILED_MSG       ZIS_LOG_CXMS_PMEM_NAME_FAILED_MSG_ID" "ZIS_LOG_CXMS_PMEM_NAME_FAILED_MSG_TEXT

#define ZIS_LOG_CXMS_MOD_NAME_FAILED_MSG_ID     ZIS_MSG_PRFX"0021E"
#define ZIS_LOG_CXMS_MOD_NAME_FAILED_MSG_TEXT   "ZSS Cross-Memory server module member name not determined, RC = %d"
#define ZIS_LOG_CXMS_MOD_NAME_FAILED_MSG        ZIS_LOG_CXMS_MOD_NAME_FAILED_MSG_ID" "ZIS_LOG_CXMS_MOD_NAME_FAILED_MSG_TEXT

#define ZIS_LOG_MODULE_STATUS_MSG_ID            ZIS_MSG_PRFX"0022I"
#define ZIS_LOG_MODULE_STATUS_MSG_TEXT          "Module %-8.8s status: %s"
#define ZIS_LOG_MODULE_STATUS_MSG               ZIS_LOG_MODULE_STATUS_MSG_ID" "ZIS_LOG_MODULE_STATUS_MSG_TEXT

#define ZIS_LOG_MODREG_NO_MARK_MSG_ID           ZIS_MSG_PRFX"0023I"
#define ZIS_LOG_MODREG_NO_MARK_MSG_TEXT         "Module %-8.8s not eligible for registry, proceeding with LPA ADD"
#define ZIS_LOG_MODREG_NO_MARK_MSG              ZIS_LOG_MODREG_NO_MARK_MSG_ID" "ZIS_LOG_MODREG_NO_MARK_MSG_TEXT

#define ZIS_LOG_MODREG_FAILURE_MSG_ID           ZIS_MSG_PRFX"0024E"
#define ZIS_LOG_MODREG_FAILURE_MSG_TEXT         "Module %-8.8s not registered, RC = %d, RSN = 0x%016llX"
#define ZIS_LOG_MODREG_FAILURE_MSG              ZIS_LOG_MODREG_FAILURE_MSG_ID" "ZIS_LOG_MODREG_FAILURE_MSG_TEXT

/* ZIS AUX messages */

#define ZISAUX_LOG_STARTUP_MSG_ID               ZIS_MSG_PRFX"0050I"
#define ZISAUX_LOG_STARTUP_MSG_TEXT             "ZIS AUX Server starting, version is %d.%d.%d+%d"
#define ZISAUX_LOG_STARTUP_MSG                  ZISAUX_LOG_STARTUP_MSG_ID" "ZISAUX_LOG_STARTUP_MSG_TEXT

#define ZISAUX_LOG_TERM_OK_MSG_ID               ZIS_MSG_PRFX"0051I"
#define ZISAUX_LOG_TERM_OK_MSG_TEXT             "ZIS AUX Server terminated"
#define ZISAUX_LOG_TERM_OK_MSG                  ZISAUX_LOG_TERM_OK_MSG_ID" "ZISAUX_LOG_TERM_OK_MSG_TEXT

#define ZISAUX_LOG_INPUT_PARM_MSG_ID            ZIS_MSG_PRFX"0052I"
#define ZISAUX_LOG_INPUT_PARM_MSG_TEXT          "Input parameters at 0x%p:"
#define ZISAUX_LOG_INPUT_PARM_MSG               ZISAUX_LOG_INPUT_PARM_MSG_ID" "ZISAUX_LOG_INPUT_PARM_MSG_TEXT

#define ZISAUX_LOG_NOT_APF_MSG_ID               ZIS_MSG_PRFX"0053E"
#define ZISAUX_LOG_NOT_APF_MSG_TEXT             "Not APF-authorized (%d)"
#define ZISAUX_LOG_NOT_APF_MSG                  ZISAUX_LOG_NOT_APF_MSG_ID" "ZISAUX_LOG_NOT_APF_MSG_TEXT

#define ZISAUX_LOG_BAD_KEY_MSG_ID               ZIS_MSG_PRFX"0054E"
#define ZISAUX_LOG_BAD_KEY_MSG_TEXT             "ZIS AUX Server started in wrong key %d"
#define ZISAUX_LOG_BAD_KEY_MSG                  ZISAUX_LOG_BAD_KEY_MSG_ID" "ZISAUX_LOG_BAD_KEY_MSG_TEXT

#define ZISAUX_LOG_RES_ALLOC_FAILED_MSG_ID      ZIS_MSG_PRFX"0055E"
#define ZISAUX_LOG_RES_ALLOC_FAILED_MSG_TEXT    "ZIS AUX Server resource not allocated (%s)"
#define ZISAUX_LOG_RES_ALLOC_FAILED_MSG         ZISAUX_LOG_RES_ALLOC_FAILED_MSG_ID" "ZISAUX_LOG_RES_ALLOC_FAILED_MSG_TEXT

#define ZISAUX_LOG_RESMGR_FAILED_MSG_ID         ZIS_MSG_PRFX"0056E"
#define ZISAUX_LOG_RESMGR_FAILED_MSG_TEXT       "RESMGR failed, RC = %d, service RC = %d"
#define ZISAUX_LOG_RESMGR_FAILED_MSG            ZISAUX_LOG_RESMGR_FAILED_MSG_ID" "ZISAUX_LOG_RESMGR_FAILED_MSG_TEXT

#define ZISAUX_LOG_PCSETUP_FAILED_MSG_ID        ZIS_MSG_PRFX"0057E"
#define ZISAUX_LOG_PCSETUP_FAILED_MSG_TEXT      "PC not established, RC = %d, RSN = %d"
#define ZISAUX_LOG_PCSETUP_FAILED_MSG           ZISAUX_LOG_PCSETUP_FAILED_MSG_ID" "ZISAUX_LOG_PCSETUP_FAILED_MSG_TEXT

#define ZISAUX_LOG_COMM_FAILED_MSG_ID           ZIS_MSG_PRFX"0058E"
#define ZISAUX_LOG_COMM_FAILED_MSG_TEXT         "Communication area failure -"
#define ZISAUX_LOG_COMM_FAILED_MSG              ZISAUX_LOG_COMM_FAILED_MSG_ID" "ZISAUX_LOG_COMM_FAILED_MSG_TEXT

#define ZISAUX_LOG_ASXTR_FAILED_MSG_ID          ZIS_MSG_PRFX"0059E"
#define ZISAUX_LOG_ASXTR_FAILED_MSG_TEXT        "Address space extract RC = %d, RSN = %d"
#define ZISAUX_LOG_ASXTR_FAILED_MSG             ZISAUX_LOG_ASXTR_FAILED_MSG_ID" "ZISAUX_LOG_ASXTR_FAILED_MSG_TEXT

#define ZISAUX_LOG_CONFIG_FAILURE_MSG_ID        ZIS_MSG_PRFX"0060E"
#define ZISAUX_LOG_CONFIG_FAILURE_MSG_TEXT      "Fatal config error - %s, RC = %d"
#define ZISAUX_LOG_CONFIG_FAILURE_MSG           ZISAUX_LOG_CONFIG_FAILURE_MSG_ID" "ZISAUX_LOG_CONFIG_FAILURE_MSG_TEXT

#define ZISAUX_LOG_CFG_NOT_READ_MSG_ID          ZIS_MSG_PRFX"0061E"
#define ZISAUX_LOG_CFG_NOT_READ_MSG_TEXT        "ZIS AUX Server configuration not read, member = \'%8.8s\', RC = %d (%d, %d)"
#define ZISAUX_LOG_CFG_NOT_READ_MSG             ZISAUX_LOG_CFG_NOT_READ_MSG_ID" "ZISAUX_LOG_CFG_NOT_READ_MSG_TEXT

#define ZISAUX_LOG_CFG_MISSING_MSG_ID           ZIS_MSG_PRFX"0062E"
#define ZISAUX_LOG_CFG_MISSING_MSG_TEXT         "ZIS AUX Server configuration not found, member = \'%8.8s\', RC = %d"
#define ZISAUX_LOG_CFG_MISSING_MSG              ZISAUX_LOG_CFG_MISSING_MSG_ID" "ZISAUX_LOG_CFG_MISSING_MSG_TEXT

#define ZISAUX_LOG_USERMOD_FAIULRE_MSG_ID       ZIS_MSG_PRFX"0063E"
#define ZISAUX_LOG_USERMOD_FAIULRE_MSG_TEXT     "User module failure -"
#define ZISAUX_LOG_USERMOD_FAIULRE_MSG          ZISAUX_LOG_USERMOD_FAIULRE_MSG_ID" "ZISAUX_LOG_USERMOD_FAIULRE_MSG_TEXT

#define ZISAUX_LOG_UNSAFE_CALL_FAIULRE_MSG_ID   ZIS_MSG_PRFX"0064W"
#define ZISAUX_LOG_UNSAFE_CALL_FAIULRE_MSG_TEXT "Unsafe function %s failed, ABEND S%03X-%02X (recovery RC = %d)"
#define ZISAUX_LOG_UNSAFE_CALL_FAIULRE_MSG      ZISAUX_LOG_UNSAFE_CALL_FAIULRE_MSG_ID" "ZISAUX_LOG_UNSAFE_CALL_FAIULRE_MSG_TEXT

#define ZISAUX_LOG_NOT_RELEASED_MSG_ID          ZIS_MSG_PRFX"0065W"
#define ZISAUX_LOG_NOT_RELEASED_MSG_TEXT        "Caller not released, RC = %d"
#define ZISAUX_LOG_NOT_RELEASED_MSG             ZISAUX_LOG_NOT_RELEASED_MSG_ID" "ZISAUX_LOG_NOT_RELEASED_MSG_TEXT

#define ZISAUX_LOG_HOST_ABEND_MSG_ID            ZIS_MSG_PRFX"0066E"
#define ZISAUX_LOG_HOST_ABEND_MSG_TEXT          "AUX host server ABEND S%03X-%02X (recovery RC = %d)"
#define ZISAUX_LOG_HOST_ABEND_MSG               ZISAUX_LOG_HOST_ABEND_MSG_ID" "ZISAUX_LOG_HOST_ABEND_MSG_TEXT

#define ZISAUX_LOG_MAIN_LOOP_FAILURE_MSG_ID     ZIS_MSG_PRFX"0067E"
#define ZISAUX_LOG_MAIN_LOOP_FAILURE_MSG_TEXT   "Main loop unexpectedly terminated"
#define ZISAUX_LOG_MAIN_LOOP_FAILURE_MSG         ZISAUX_LOG_MAIN_LOOP_FAILURE_MSG_ID" "ZISAUX_LOG_MAIN_LOOP_FAILURE_MSG_TEXT

#define ZISAUX_LOG_CMD_TOO_LONG_MSG_ID          ZIS_MSG_PRFX"0068W"
#define ZISAUX_LOG_CMD_TOO_LONG_MSG_TEXT        "Command too long (%u)"
#define ZISAUX_LOG_CMD_TOO_LONG_MSG             ZISAUX_LOG_CMD_TOO_LONG_MSG_ID" "ZISAUX_LOG_CMD_TOO_LONG_MSG_TEXT

#define ZISAUX_LOG_CMD_TKNZ_FAILURE_MSG_ID      ZIS_MSG_PRFX"0069W"
#define ZISAUX_LOG_CMD_TKNZ_FAILURE_MSG_TEXT    "Command not tokenized"
#define ZISAUX_LOG_CMD_TKNZ_FAILURE_MSG         ZISAUX_LOG_CMD_TKNZ_FAILURE_MSG_ID" "ZISAUX_LOG_CMD_TKNZ_FAILURE_MSG_TEXT

#define ZISAUX_LOG_CMD_RECEIVED_MSG_ID          ZIS_MSG_PRFX"0070I"
#define ZISAUX_LOG_CMD_RECEIVED_MSG_TEXT        "Modify %s command received"
#define ZISAUX_LOG_CMD_RECEIVED_MSG             ZISAUX_LOG_CMD_RECEIVED_MSG_ID" "ZISAUX_LOG_CMD_RECEIVED_MSG_TEXT

#define ZISAUX_LOG_TERM_CMD_MSG_ID              ZIS_MSG_PRFX"0071I"
#define ZISAUX_LOG_TERM_CMD_MSG_TEXT            "Termination command received"
#define ZISAUX_LOG_TERM_CMD_MSG                 ZISAUX_LOG_TERM_CMD_MSG_ID" "ZISAUX_LOG_TERM_CMD_MSG_TEXT

#define ZISAUX_LOG_CMD_ACCEPTED_MSG_ID          ZIS_MSG_PRFX"0072I"
#define ZISAUX_LOG_CMD_ACCEPTED_MSG_TEXT        "Modify %s command accepted"
#define ZISAUX_LOG_CMD_ACCEPTED_MSG             ZISAUX_LOG_CMD_ACCEPTED_MSG_ID" "ZISAUX_LOG_CMD_ACCEPTED_MSG_TEXT

#define ZISAUX_LOG_CMD_NOT_RECOGNIZED_MSG_ID    ZIS_MSG_PRFX"0073I"
#define ZISAUX_LOG_CMD_NOT_RECOGNIZED_MSG_TEXT  "Modify %s command not recognized"
#define ZISAUX_LOG_CMD_NOT_RECOGNIZED_MSG       ZISAUX_LOG_CMD_NOT_RECOGNIZED_MSG_ID" "ZISAUX_LOG_CMD_NOT_RECOGNIZED_MSG_TEXT

#define ZISAUX_LOG_CMD_REJECTED_MSG_ID          ZIS_MSG_PRFX"0074W"
#define ZISAUX_LOG_CMD_REJECTED_MSG_TEXT        "Modify %s command rejected"
#define ZISAUX_LOG_CMD_REJECTED_MSG             ZISAUX_LOG_CMD_REJECTED_MSG_ID" "ZISAUX_LOG_CMD_REJECTED_MSG_TEXT

#define ZISAUX_LOG_INVALID_CMD_ARGS_MSG_ID      ZIS_MSG_PRFX"0075W"
#define ZISAUX_LOG_INVALID_CMD_ARGS_MSG_TEXT    "%s expects %u args, %u provided, command ignored"
#define ZISAUX_LOG_INVALID_CMD_ARGS_MSG         ZISAUX_LOG_INVALID_CMD_ARGS_MSG_ID" "ZISAUX_LOG_INVALID_CMD_ARGS_MSG_TEXT

#define ZISAUX_LOG_BAD_LOG_COMP_MSG_ID          ZIS_MSG_PRFX"0076W"
#define ZISAUX_LOG_BAD_LOG_COMP_MSG_TEXT        "Log component \'%s\' not recognized, command ignored"
#define ZISAUX_LOG_BAD_LOG_COMP_MSG             ZISAUX_LOG_BAD_LOG_COMP_MSG_ID" "ZISAUX_LOG_BAD_LOG_COMP_MSG_TEXT

#define ZISAUX_LOG_BAD_LOG_LEVEL_MSG_ID         ZIS_MSG_PRFX"0077W"
#define ZISAUX_LOG_BAD_LOG_LEVEL_MSG_TEXT       "Log level \'%s\' not recognized, command ignored"
#define ZISAUX_LOG_BAD_LOG_LEVEL_MSG            ZISAUX_LOG_BAD_LOG_LEVEL_MSG_ID" "ZISAUX_LOG_BAD_LOG_LEVEL_MSG_TEXT

#define ZISAUX_LOG_DISP_CMD_RESULT_MSG_ID       ZIS_MSG_PRFX"0078I"
#define ZISAUX_LOG_DISP_CMD_RESULT_MSG_TEXT     ""
#define ZISAUX_LOG_DISP_CMD_RESULT_MSG          ZISAUX_LOG_DISP_CMD_RESULT_MSG_ID" "ZISAUX_LOG_DISP_CMD_RESULT_MSG_TEXT

#define ZISAUX_LOG_USERMOD_CMD_RESP_MSG_ID      ZIS_MSG_PRFX"0079I"
#define ZISAUX_LOG_USERMOD_CMD_RESP_MSG_TEXT    "Response message - \'%.*s\'"
#define ZISAUX_LOG_USERMOD_CMD_RESP_MSG         ZISAUX_LOG_USERMOD_CMD_RESP_MSG_ID" "ZISAUX_LOG_USERMOD_CMD_RESP_MSG_TEXT

#define ZISAUX_LOG_TERM_SIGNAL_MSG_ID           ZIS_MSG_PRFX"0080I"
#define ZISAUX_LOG_TERM_SIGNAL_MSG_TEXT         "Termination signal received (0x%08X)"
#define ZISAUX_LOG_TERM_SIGNAL_MSG              ZISAUX_LOG_TERM_SIGNAL_MSG_ID" "ZISAUX_LOG_TERM_SIGNAL_MSG_TEXT

#define ZISAUX_LOG_DUB_ERROR_MSG_ID             ZIS_MSG_PRFX"0081E"
#define ZISAUX_LOG_DUB_ERROR_MSG_TEXT           "Bad dub status %d (%d,0x%04X), verify that the started task user has an OMVS segment"
#define ZISAUX_LOG_DUB_ERROR_MSG                ZISAUX_LOG_DUB_ERROR_MSG_ID" "ZISAUX_LOG_DUB_ERROR_MSG_TEXT

#define ZISAUX_LOG_LEGACY_API_MSG_ID            ZIS_MSG_PRFX"0082W"
#define ZISAUX_LOG_LEGACY_API_MSG_TEXT          "Legacy API has been detected, some functionality may be limited"
#define ZISAUX_LOG_LEGACY_API_MSG               ZISAUX_LOG_LEGACY_API_MSG_ID" "ZISAUX_LOG_LEGACY_API_MSG_TEXT

/* ZIS dynamic linkage plug-in messages */

#define ZISDYN_LOG_STARTUP_MSG_ID               ZIS_MSG_PRFX"0700I"
#define ZISDYN_LOG_STARTUP_MSG_TEXT             "ZIS Dynamic Base plug-in starting, version %d.%d.%d+%d, stub version %d"
#define ZISDYN_LOG_STARTUP_MSG                  ZISDYN_LOG_STARTUP_MSG_ID" "ZISDYN_LOG_STARTUP_MSG_TEXT

#define ZISDYN_LOG_STARTED_MSG_ID               ZIS_MSG_PRFX"0701I"
#define ZISDYN_LOG_STARTED_MSG_TEXT             "ZIS Dynamic Base plug-in successfully started"
#define ZISDYN_LOG_STARTED_MSG                  ZISDYN_LOG_STARTED_MSG_ID" "ZISDYN_LOG_STARTED_MSG_TEXT

#define ZISDYN_LOG_STARTUP_FAILED_MSG_ID        ZIS_MSG_PRFX"0702E"
#define ZISDYN_LOG_STARTUP_FAILED_MSG_TEXT      "ZIS Dynamic Base plug-in startup failed, status = %d"
#define ZISDYN_LOG_STARTUP_FAILED_MSG           ZISDYN_LOG_STARTUP_FAILED_MSG_ID" "ZISDYN_LOG_STARTUP_FAILED_MSG_TEXT

#define ZISDYN_LOG_INIT_ERROR_MSG_ID            ZIS_MSG_PRFX"0703E"
#define ZISDYN_LOG_INIT_ERROR_MSG_TEXT          "ZIS Dynamic Base plug-in init error -"
#define ZISDYN_LOG_INIT_ERROR_MSG               ZISDYN_LOG_INIT_ERROR_MSG_ID" "ZISDYN_LOG_INIT_ERROR_MSG_TEXT

#define ZISDYN_LOG_TERM_MSG_ID                  ZIS_MSG_PRFX"0704I"
#define ZISDYN_LOG_TERM_MSG_TEXT                "ZIS Dynamic Base plug-in terminating"
#define ZISDYN_LOG_TERM_MSG                     ZISDYN_LOG_TERM_MSG_ID" "ZISDYN_LOG_TERM_MSG_TEXT

#define ZISDYN_LOG_TERMED_MSG_ID                ZIS_MSG_PRFX"0705I"
#define ZISDYN_LOG_TERMED_MSG_TEXT              "ZIS Dynamic Base plug-in successfully terminated"
#define ZISDYN_LOG_TERMED_MSG                   ZISDYN_LOG_TERMED_MSG_ID" "ZISDYN_LOG_TERMED_MSG_TEXT

#define ZISDYN_LOG_TERM_FAILED_MSG_ID           ZIS_MSG_PRFX"0706E"
#define ZISDYN_LOG_TERM_FAILED_MSG_TEXT         "ZIS Dynamic Base plug-in terminated with error"
#define ZISDYN_LOG_TERM_FAILED_MSG              ZISDYN_LOG_TERM_FAILED_MSG_ID" "ZISDYN_LOG_TERM_FAILED_MSG_TEXT

#define ZISDYN_LOG_CMD_RESP_MSG_ID              ZIS_MSG_PRFX"0707I"
#define ZISDYN_LOG_CMD_RESP_TEXT                ""
#define ZISDYN_LOG_CMD_RESP_MSG                 ZISDYN_LOG_CMD_RESP_MSG_ID""ZISDYN_LOG_CMD_RESP_TEXT

#define ZISDYN_LOG_STUB_CREATED_MSG_ID          ZIS_MSG_PRFX"0708I"
#define ZISDYN_LOG_STUB_CREATED_MSG_TEXT        "Stub vector has been created at %p"
#define ZISDYN_LOG_STUB_CREATED_MSG             ZISDYN_LOG_STUB_CREATED_MSG_ID" "ZISDYN_LOG_STUB_CREATED_MSG_TEXT

#define ZISDYN_LOG_STUB_REUSED_MSG_ID           ZIS_MSG_PRFX"0710I"
#define ZISDYN_LOG_STUB_REUSED_MSG_TEXT         "Stub vector at %p has been reused"
#define ZISDYN_LOG_STUB_REUSED_MSG              ZISDYN_LOG_STUB_REUSED_MSG_ID" "ZISDYN_LOG_STUB_REUSED_MSG_TEXT

#define ZISDYN_LOG_STUB_DELETED_MSG_ID          ZIS_MSG_PRFX"0711I"
#define ZISDYN_LOG_STUB_DELETED_MSG_TEXT        "Stub vector at %p has been deleted"
#define ZISDYN_LOG_STUB_DELETED_MSG             ZISDYN_LOG_STUB_DELETED_MSG_ID" "ZISDYN_LOG_STUB_DELETED_MSG_TEXT

#define ZISDYN_LOG_STUB_DISCARDED_MSG_ID        ZIS_MSG_PRFX"0712W"
#define ZISDYN_LOG_STUB_DISCARDED_MSG_TEXT      "Stub vector at %p is discarded due to %s:"
#define ZISDYN_LOG_STUB_DISCARDED_MSG           ZISDYN_LOG_STUB_DISCARDED_MSG_ID" "ZISDYN_LOG_STUB_DISCARDED_MSG_TEXT

#define ZISDYN_LOG_DEV_MODE_MSG_ID              ZIS_MSG_PRFX"0713W"
#define ZISDYN_LOG_DEV_MODE_MSG_TEXT            "ZIS Dynamic base plug-in development mode is enabled"
#define ZISDYN_LOG_DEV_MODE_MSG                 ZISDYN_LOG_DEV_MODE_MSG_ID" "ZISDYN_LOG_DEV_MODE_MSG_TEXT

#define ZISDYN_LOG_BAD_ZIS_VERSION_MSG_ID       ZIS_MSG_PRFX"0714E"
#define ZISDYN_LOG_BAD_ZIS_VERSION_MSG_TEXT     "Bad cross-memory server version: expected [%d.%d.%d, %d.0.0), found %d.%d.%d"
#define ZISDYN_LOG_BAD_ZIS_VERSION_MSG          ZISDYN_LOG_BAD_ZIS_VERSION_MSG_ID" "ZISDYN_LOG_BAD_ZIS_VERSION_MSG_TEXT

#endif /* ZIS_MSG_H_ */


/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

