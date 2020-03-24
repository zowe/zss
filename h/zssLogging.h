

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

#ifndef MVD_H_ZSSLOGGING_H_
#define MVD_H_ZSSLOGGING_H_

#ifndef ZSS_LOG_ID
#define ZSS_LOG_ID     "ZWE"
#endif

#ifndef LOG_SUBCOMP_ID
#define LOG_SUBCOMP_ID  "S"
#endif

#ifndef LOG_PROD_ID
#define LOG_PROD_ID ZSS_LOG_ID LOG_SUBCOMP_ID
#endif

#ifndef LOG_MSG_SUBCOMP
#define LOG_MSG_SUBCOMP LOG_SUBCOMP_ID
#endif

#ifndef LOG_MSG_PRFX
#define LOG_MSG_PRFX  ZSS_LOG_ID LOG_MSG_SUBCOMP
#endif

#define LOG_COMP_ID_MVD_SERVER    0x008F000300010000
#define LOG_COMP_ID_CTDS          0x008F000300020000
#define LOG_COMP_ID_SECURITY      0x008F000300030000
#define LOG_COMP_ID_UNIXFILE      0x008F000300040000
#define LOG_COMP_ID_DATASERVICE   0x008F000300050000

bool isLogLevelValid(int level);

/* default message IDs */

/* 0000 - 0999 are messages reserved for for crossmemory (see crossmemory.h) */
/* 1000 - 1199 are messages from MVD_SERVER */
/* 1200 - 1399 are messages from UNIXFILE */
/* 1400 - 1599 are messages from SECURITY */
/* 1600 - 1799 are messages from CTDS */

/* MVD Server */

#ifndef ZSS_LOG_PRV_SRV_NAME_MSG_ID
#define ZSS_LOG_PRV_SRV_NAME_MSG_ID          LOG_MSG_PRFX"1000W"
#endif
#define ZSS_LOG_PRV_SRV_NAME_MSG_TEXT        "warning: privileged server name not provided, falling back to default\n"
#define ZSS_LOG_PRV_SRV_NAME_MSG             ZSS_LOG_PRV_SRV_NAME_MSG_ID" "ZSS_LOG_PRV_SRV_NAME_MSG_TEXT

#ifndef ZSS_LOG_INC_LOG_LEVEL_MSG_ID
#define ZSS_LOG_INC_LOG_LEVEL_MSG_ID         LOG_MSG_PRFX"1001E"
#endif
#define ZSS_LOG_INC_LOG_LEVEL_MSG_TEXT       "error: log level %d is incorrect\n"
#define ZSS_LOG_INC_LOG_LEVEL_MSG            ZSS_LOG_INC_LOG_LEVEL_MSG_ID" "ZSS_LOG_INC_LOG_LEVEL_MSG_TEXT

#ifndef ZSS_LOG_FILE_EXPECTED_TOP_MSG_ID
#define ZSS_LOG_FILE_EXPECTED_TOP_MSG_ID     LOG_MSG_PRFX"1002E"
#endif
#define ZSS_LOG_FILE_EXPECTED_TOP_MSG_TEXT   "error in file %s: expected top level object\n"
#define ZSS_LOG_FILE_EXPECTED_TOP_MSG        ZSS_LOG_FILE_EXPECTED_TOP_MSG_ID" "ZSS_LOG_FILE_EXPECTED_TOP_MSG_TEXT

#ifndef ZSS_LOG_PARS_ZSS_SETTING_MSG_ID
#define ZSS_LOG_PARS_ZSS_SETTING_MSG_ID      LOG_MSG_PRFX"1003E"
#endif
#define ZSS_LOG_PARS_ZSS_SETTING_MSG_TEXT    "error while parsing ZSS settings from file %s: %s\n"
#define ZSS_LOG_PARS_ZSS_SETTING_MSG         ZSS_LOG_PARS_ZSS_SETTING_MSG_ID" "ZSS_LOG_PARS_ZSS_SETTING_MSG_TEXT

#ifndef ZSS_LOG_EXPEC_PLUGIN_ID_MSG_ID
#define ZSS_LOG_EXPEC_PLUGIN_ID_MSG_ID       LOG_MSG_PRFX"1004I"
#endif
#define ZSS_LOG_EXPEC_PLUGIN_ID_MSG_TEXT     "expected plugin identifier %s, got %s\n"
#define ZSS_LOG_EXPEC_PLUGIN_ID_MSG          ZSS_LOG_EXPEC_PLUGIN_ID_MSG_ID" "ZSS_LOG_EXPEC_PLUGIN_ID_MSG_TEXT

#ifndef ZSS_LOG_PLUGIN_ID_NFOUND_MSG_ID
#define ZSS_LOG_PLUGIN_ID_NFOUND_MSG_ID      LOG_MSG_PRFX"1005I"
#endif
#define ZSS_LOG_PLUGIN_ID_NFOUND_MSG_TEXT    "plugin identifier was not found in %s\n"
#define ZSS_LOG_PLUGIN_ID_NFOUND_MSG         ZSS_LOG_PLUGIN_ID_NFOUND_MSG_ID" "ZSS_LOG_PLUGIN_ID_NFOUND_MSG_TEXT

#ifndef ZSS_LOG_PARS_FILE_MSG_ID
#define ZSS_LOG_PARS_FILE_MSG_ID             LOG_MSG_PRFX"1006E"
#endif
#define ZSS_LOG_PARS_FILE_MSG_TEXT           "error while parsing file %s: %s\n"
#define ZSS_LOG_PARS_FILE_MSG                ZSS_LOG_PARS_FILE_MSG_ID" "ZSS_LOG_PARS_FILE_MSG_TEXT

#ifndef ZSS_LOG_WEB_CONT_NFOUND_MSG_ID
#define ZSS_LOG_WEB_CONT_NFOUND_MSG_ID       LOG_MSG_PRFX"1007I"
#endif
#define ZSS_LOG_WEB_CONT_NFOUND_MSG_TEXT     "webContent wasn't found in plugin defintion for %s\n"
#define ZSS_LOG_WEB_CONT_NFOUND_MSG          ZSS_LOG_WEB_CONT_NFOUND_MSG_ID" "ZSS_LOG_WEB_CONT_NFOUND_MSG_TEXT

#ifndef ZSS_LOG_LIBR_VER_NFOUND_MSG_ID
#define ZSS_LOG_LIBR_VER_NFOUND_MSG_ID       LOG_MSG_PRFX"1008I"
#endif
#define ZSS_LOG_LIBR_VER_NFOUND_MSG_TEXT     "libraryVersion wasn't found in plugin defintion for %s\n"
#define ZSS_LOG_LIBR_VER_NFOUND_MSG          ZSS_LOG_LIBR_VER_NFOUND_MSG_ID" "ZSS_LOG_LIBR_VER_NFOUND_MSG_TEXT

#ifndef ZSS_LOG_PLUGIN_ID_NULL_MSG_ID
#define ZSS_LOG_PLUGIN_ID_NULL_MSG_ID        LOG_MSG_PRFX"1009W"
#endif
#define ZSS_LOG_PLUGIN_ID_NULL_MSG_TEXT      "plugin id=%s is NULL and cannot be loaded.\n"
#define ZSS_LOG_PLUGIN_ID_NULL_MSG           ZSS_LOG_PLUGIN_ID_NULL_MSG_ID" "ZSS_LOG_PLUGIN_ID_NULL_MSG_TEXT

#ifndef ZSS_LOG_PLUGIN_IDLOC_NFOUND_MSG_ID
#define ZSS_LOG_PLUGIN_IDLOC_NFOUND_MSG_ID   LOG_MSG_PRFX"1010I"
#endif
#define ZSS_LOG_PLUGIN_IDLOC_NFOUND_MSG_TEXT "plugin identifier or/and pluginLocation was not found in %s\n"
#define ZSS_LOG_PLUGIN_IDLOC_NFOUND_MSG      ZSS_LOG_PLUGIN_IDLOC_NFOUND_MSG_ID" "ZSS_LOG_PLUGIN_IDLOC_NFOUND_MSG_TEXT

#ifndef ZSS_LOG_PARS_GENERIC_MSG_ID
#define ZSS_LOG_PARS_GENERIC_MSG_ID          LOG_MSG_PRFX"1011E"
#endif
#define ZSS_LOG_PARS_GENERIC_MSG_TEXT        "error while parsing %s: %s\n"
#define ZSS_LOG_PARS_GENERIC_MSG             ZSS_LOG_PARS_GENERIC_MSG_ID" "ZSS_LOG_PARS_GENERIC_MSG_TEXT

#ifndef ZSS_LOG_OPEN_DIR_FAIL_MSG_ID
#define ZSS_LOG_OPEN_DIR_FAIL_MSG_ID         LOG_MSG_PRFX"1012W"
#endif
#define ZSS_LOG_OPEN_DIR_FAIL_MSG_TEXT       "couldn't open directory '%s' returnCode=%d reasonCode=0x%x\n"
#define ZSS_LOG_OPEN_DIR_FAIL_MSG            ZSS_LOG_OPEN_DIR_FAIL_MSG_ID" "ZSS_LOG_OPEN_DIR_FAIL_MSG_TEXT

#ifndef ZSS_LOG_ZSS_START_VER_MSG_ID
#define ZSS_LOG_ZSS_START_VER_MSG_ID         LOG_MSG_PRFX"1013I"
#endif
#define ZSS_LOG_ZSS_START_VER_MSG_TEXT       "zssServer startup, version %s\n"
#define ZSS_LOG_ZSS_START_VER_MSG            ZSS_LOG_ZSS_START_VER_MSG_ID" "ZSS_LOG_ZSS_START_VER_MSG_TEXT

#ifndef ZSS_LOG_ZIS_STATUS_MSG_ID
#define ZSS_LOG_ZIS_STATUS_MSG_ID            LOG_MSG_PRFX"1014I"
#endif
#define ZSS_LOG_ZIS_STATUS_MSG_TEXT          "ZIS status - %s (name='%.16s', cmsRC=%d, description='%s', clientVersion=%d)\n"
#define ZSS_LOG_ZIS_STATUS_MSG               ZSS_LOG_ZIS_STATUS_MSG_ID" "ZSS_LOG_ZIS_STATUS_MSG_TEXT

#ifndef ZSS_LOG_HTTPS_NO_IMPLEM_MSG_ID
#define ZSS_LOG_HTTPS_NO_IMPLEM_MSG_ID       LOG_MSG_PRFX"1015W"
#endif
#define ZSS_LOG_HTTPS_NO_IMPLEM_MSG_TEXT     "*** WARNING: Server doesn't implement HTTPS! ***\n*** In production only use localhost or 127.0.0.1! Server using: %s ***\n"
#define ZSS_LOG_HTTPS_NO_IMPLEM_MSG          ZSS_LOG_HTTPS_NO_IMPLEM_MSG_ID" "ZSS_LOG_HTTPS_NO_IMPLEM_MSG_TEXT

#ifndef ZSS_LOG_CANT_VAL_PERMISS_MSG_ID
#define ZSS_LOG_CANT_VAL_PERMISS_MSG_ID      LOG_MSG_PRFX"1016E"
#endif
#define ZSS_LOG_CANT_VAL_PERMISS_MSG_TEXT    "Cannot validate file permission, path is not defined.\n"
#define ZSS_LOG_CANT_VAL_PERMISS_MSG         ZSS_LOG_CANT_VAL_PERMISS_MSG_ID" "ZSS_LOG_CANT_VAL_PERMISS_MSG_TEXT

#ifndef ZSS_LOG_CANT_STAT_CONFIG_MSG_ID
#define ZSS_LOG_CANT_STAT_CONFIG_MSG_ID      LOG_MSG_PRFX"1017E"
#endif
#define ZSS_LOG_CANT_STAT_CONFIG_MSG_TEXT    "Couldnt stat config path=%s. return=%d, reason=%d\n"
#define ZSS_LOG_CANT_STAT_CONFIG_MSG         ZSS_LOG_CANT_STAT_CONFIG_MSG_ID" "ZSS_LOG_CANT_STAT_CONFIG_MSG_TEXT

#ifndef ZSS_LOG_CONFIG_OPEN_PERMISS_MSG_ID
#define ZSS_LOG_CONFIG_OPEN_PERMISS_MSG_ID   LOG_MSG_PRFX"1018E"
#endif
#define ZSS_LOG_CONFIG_OPEN_PERMISS_MSG_TEXT "Config path=%s has group & other permissions that are too open! Refusing to start.\n"
#define ZSS_LOG_CONFIG_OPEN_PERMISS_MSG      ZSS_LOG_CONFIG_OPEN_PERMISS_MSG_ID" "ZSS_LOG_CONFIG_OPEN_PERMISS_MSG_TEXT

#ifndef ZSS_LOG_ENSURE_PERMISS_MSG_ID
#define ZSS_LOG_ENSURE_PERMISS_MSG_ID        LOG_MSG_PRFX"1019E"
#endif
#define ZSS_LOG_ENSURE_PERMISS_MSG_TEXT      "Ensure group has no write (or execute, if file) permission. Ensure other has no permissions. Then, restart zssServer to retry.\n"
#define ZSS_LOG_ENSURE_PERMISS_MSG           ZSS_LOG_ENSURE_PERMISS_MSG_ID" "ZSS_LOG_ENSURE_PERMISS_MSG_TEXT

#ifndef ZSS_LOG_SKIP_PERMISS_MSG_ID
#define ZSS_LOG_SKIP_PERMISS_MSG_ID          LOG_MSG_PRFX"1020E"
#endif
#define ZSS_LOG_SKIP_PERMISS_MSG_TEXT        "Skipping validation of file permissions: disabled during compilation, file %s.\n"
#define ZSS_LOG_SKIP_PERMISS_MSG             ZSS_LOG_SKIP_PERMISS_MSG_ID" "ZSS_LOG_SKIP_PERMISS_MSG_TEXT

#ifndef ZSS_LOG_PATH_UNDEF_MSG_ID
#define ZSS_LOG_PATH_UNDEF_MSG_ID            LOG_MSG_PRFX"1021E"
#endif
#define ZSS_LOG_PATH_UNDEF_MSG_TEXT          "Cannot validate file permission, path is not defined.\n"
#define ZSS_LOG_PATH_UNDEF_MSG               ZSS_LOG_PATH_UNDEF_MSG_ID" "ZSS_LOG_PATH_UNDEF_MSG_TEXT

#ifndef ZSS_LOG_PATH_DIR_MSG_ID
#define ZSS_LOG_PATH_DIR_MSG_ID              LOG_MSG_PRFX"1022E"
#endif
#define ZSS_LOG_PATH_DIR_MSG_TEXT            "Cannot validate file permission, path was for a directory not a file.\n"
#define ZSS_LOG_PATH_DIR_MSG                 ZSS_LOG_PATH_DIR_MSG_ID" "ZSS_LOG_PATH_DIR_MSG_TEXT

#ifndef ZSS_LOG_NO_JWT_AGENT_MSG_ID
#define ZSS_LOG_NO_JWT_AGENT_MSG_ID          LOG_MSG_PRFX"1023I"
#endif
#define ZSS_LOG_NO_JWT_AGENT_MSG_TEXT        "Will not accept JWTs: agent configuration missing\n"
#define ZSS_LOG_NO_JWT_AGENT_MSG             ZSS_LOG_NO_JWT_AGENT_MSG_ID" "ZSS_LOG_NO_JWT_AGENT_MSG_TEXT

#ifndef ZSS_LOG_NO_JWT_CONFIG_MSG_ID
#define ZSS_LOG_NO_JWT_CONFIG_MSG_ID         LOG_MSG_PRFX"1024I"
#endif
#define ZSS_LOG_NO_JWT_CONFIG_MSG_TEXT       "Will not accept JWTs: JWT keystore configuration missing\n"
#define ZSS_LOG_NO_JWT_CONFIG_MSG            ZSS_LOG_NO_JWT_CONFIG_MSG_ID" "ZSS_LOG_NO_JWT_CONFIG_MSG_TEXT

#ifndef ZSS_LOG_NO_JWT_DISABLED_MSG_ID
#define ZSS_LOG_NO_JWT_DISABLED_MSG_ID       LOG_MSG_PRFX"1025I"
#endif
#define ZSS_LOG_NO_JWT_DISABLED_MSG_TEXT     "Will not accept JWTs: disabled in the configuration\n"
#define ZSS_LOG_NO_JWT_DISABLED_MSG          ZSS_LOG_NO_JWT_DISABLED_MSG_ID" "ZSS_LOG_NO_JWT_DISABLED_MSG_TEXT

#ifndef ZSS_LOG_JWT_CONFIG_MISSING_MSG_ID
#define ZSS_LOG_JWT_CONFIG_MISSING_MSG_ID    LOG_MSG_PRFX"1026E"
#endif
#define ZSS_LOG_JWT_CONFIG_MISSING_MSG_TEXT  "JWT keystore configuration missing\n"
#define ZSS_LOG_JWT_CONFIG_MISSING_MSG       ZSS_LOG_JWT_CONFIG_MISSING_MSG_ID" "ZSS_LOG_JWT_CONFIG_MISSING_MSG_TEXT

#ifndef ZSS_LOG_JWT_KEYSTORE_UNKN_MSG_ID
#define ZSS_LOG_JWT_KEYSTORE_UNKN_MSG_ID     LOG_MSG_PRFX"1027E"
#endif
#define ZSS_LOG_JWT_KEYSTORE_UNKN_MSG_TEXT   "Invalid JWT configuration: unknown keystore type %s\n"
#define ZSS_LOG_JWT_KEYSTORE_UNKN_MSG        ZSS_LOG_JWT_KEYSTORE_UNKN_MSG_ID" "ZSS_LOG_JWT_KEYSTORE_UNKN_MSG_TEXT

#ifndef ZSS_LOG_JWT_KEYSTORE_NAME_MSG_ID
#define ZSS_LOG_JWT_KEYSTORE_NAME_MSG_ID     LOG_MSG_PRFX"1028E"
#endif
#define ZSS_LOG_JWT_KEYSTORE_NAME_MSG_TEXT   "Invalid JWT configuration: keystore name missing\n"
#define ZSS_LOG_JWT_KEYSTORE_NAME_MSG        ZSS_LOG_JWT_KEYSTORE_NAME_MSG_ID" "ZSS_LOG_JWT_KEYSTORE_NAME_MSG_TEXT

#ifndef ZSS_LOG_JWT_TOKEN_FALLBK_MSG_ID
#define ZSS_LOG_JWT_TOKEN_FALLBK_MSG_ID      LOG_MSG_PRFX"1029I"
#endif
#define ZSS_LOG_JWT_TOKEN_FALLBK_MSG_TEXT    "Will use JWT using PKCS#11 token '%s', key id '%s', %s fallback to legacy tokens\n"
#define ZSS_LOG_JWT_TOKEN_FALLBK_MSG         ZSS_LOG_JWT_TOKEN_FALLBK_MSG_ID" "ZSS_LOG_JWT_TOKEN_FALLBK_MSG_TEXT

#ifndef ZSS_LOG_NO_LOAD_JWT_MSG_ID
#define ZSS_LOG_NO_LOAD_JWT_MSG_ID           LOG_MSG_PRFX"1030W"
#endif
#define ZSS_LOG_NO_LOAD_JWT_MSG_TEXT         "Server startup problem: could not load the JWT key %s from token %s: rc %d, p11rc %d, p11Rsn %d\n"
#define ZSS_LOG_NO_LOAD_JWT_MSG              ZSS_LOG_NO_LOAD_JWT_MSG_ID" "ZSS_LOG_NO_LOAD_JWT_MSG_TEXT

#ifndef ZSS_LOG_PATH_TO_SERVER_MSG_ID
#define ZSS_LOG_PATH_TO_SERVER_MSG_ID        LOG_MSG_PRFX"1031W"
#endif
#define ZSS_LOG_PATH_TO_SERVER_MSG_TEXT      "Usage: zssServer <path to server.json file>\n"
#define ZSS_LOG_PATH_TO_SERVER_MSG           ZSS_LOG_PATH_TO_SERVER_MSG_ID" "ZSS_LOG_PATH_TO_SERVER_MSG_TEXT

#ifndef ZSS_LOG_SIG_IGNORE_MSG_ID
#define ZSS_LOG_SIG_IGNORE_MSG_ID            LOG_MSG_PRFX"1032W"
#endif
#define ZSS_LOG_SIG_IGNORE_MSG_TEXT          "warning: sigignore(SIGPIPE)=%d, errno=%d\n"
#define ZSS_LOG_SIG_IGNORE_MSG               ZSS_LOG_SIG_IGNORE_MSG_ID" "ZSS_LOG_SIG_IGNORE_MSG_TEXT

#ifndef ZSS_LOG_SERVER_CONFIG_MSG_ID
#define ZSS_LOG_SERVER_CONFIG_MSG_ID         LOG_MSG_PRFX"1033I"
#endif
#define ZSS_LOG_SERVER_CONFIG_MSG_TEXT       "Server config file=%s\n"
#define ZSS_LOG_SERVER_CONFIG_MSG            ZSS_LOG_SERVER_CONFIG_MSG_ID" "ZSS_LOG_SERVER_CONFIG_MSG_TEXT

#ifndef ZSS_LOG_SERVER_STARTUP_MSG_ID
#define ZSS_LOG_SERVER_STARTUP_MSG_ID        LOG_MSG_PRFX"1034W"
#endif
#define ZSS_LOG_SERVER_STARTUP_MSG_TEXT      "server startup problem, address %s not valid\n"
#define ZSS_LOG_SERVER_STARTUP_MSG           ZSS_LOG_SERVER_STARTUP_MSG_ID" "ZSS_LOG_SERVER_STARTUP_MSG_TEXT

#ifndef ZSS_LOG_ZSS_SETTINGS_MSG_ID
#define ZSS_LOG_ZSS_SETTINGS_MSG_ID          LOG_MSG_PRFX"1035I"
#endif
#define ZSS_LOG_ZSS_SETTINGS_MSG_TEXT        "ZSS server settings: address=%s, port=%d\n"
#define ZSS_LOG_ZSS_SETTINGS_MSG             ZSS_LOG_ZSS_SETTINGS_MSG_ID" "ZSS_LOG_ZSS_SETTINGS_MSG_TEXT

#ifndef ZSS_LOG_ZSS_STARTUP_MSG_ID
#define ZSS_LOG_ZSS_STARTUP_MSG_ID           LOG_MSG_PRFX"1036W"
#endif
#define ZSS_LOG_ZSS_STARTUP_MSG_TEXT         "Server startup problem ret=%d reason=0x%x\n"
#define ZSS_LOG_ZSS_STARTUP_MSG              ZSS_LOG_ZSS_STARTUP_MSG_ID" "ZSS_LOG_ZSS_STARTUP_MSG_TEXT

#ifndef ZSS_LOG_PORT_OCCUP_MSG_ID
#define ZSS_LOG_PORT_OCCUP_MSG_ID            LOG_MSG_PRFX"1037W"
#endif
#define ZSS_LOG_PORT_OCCUP_MSG_TEXT          "This is usually due to the server port (%d) already being occupied. Is ZSS running twice?\n"
#define ZSS_LOG_PORT_OCCUP_MSG               ZSS_LOG_PORT_OCCUP_MSG_ID" "ZSS_LOG_PORT_OCCUP_MSG_TEXT

/* MVD Server (Datasets) */

#ifndef ZSS_LOG_DS_NAME_HLQ_MSG_ID
#define ZSS_LOG_DS_NAME_HLQ_MSG_ID           LOG_MSG_PRFX"1038I"
#endif
#define ZSS_LOG_DS_NAME_HLQ_MSG_TEXT         "l1=%s\n"
#define ZSS_LOG_DS_NAME_HLQ_MSG              ZSS_LOG_DS_NAME_HLQ_MSG_ID" "ZSS_LOG_DS_NAME_HLQ_MSG_TEXT

#ifndef ZSS_LOG_DS_SERVING_NAME_MSG_ID
#define ZSS_LOG_DS_SERVING_NAME_MSG_ID       LOG_MSG_PRFX"1039I"
#endif
#define ZSS_LOG_DS_SERVING_NAME_MSG_TEXT     "Serving: %s\n"
#define ZSS_LOG_DS_SERVING_NAME_MSG          ZSS_LOG_DS_SERVING_NAME_MSG_ID" "ZSS_LOG_DS_SERVING_NAME_MSG_TEXT

#ifndef ZSS_LOG_DS_UPDATING_IF_MSG_ID
#define ZSS_LOG_DS_UPDATING_IF_MSG_ID        LOG_MSG_PRFX"1040I"
#endif
#define ZSS_LOG_DS_UPDATING_IF_MSG_TEXT      "Updating if exists: %s\n"
#define ZSS_LOG_DS_UPDATING_IF_MSG           ZSS_LOG_DS_UPDATING_IF_MSG_ID" "ZSS_LOG_DS_UPDATING_IF_MSG_TEXT

#ifndef ZSS_LOG_DS_DELETING_IF_MSG_ID
#define ZSS_LOG_DS_DELETING_IF_MSG_ID        LOG_MSG_PRFX"1041I"
#endif
#define ZSS_LOG_DS_DELETING_IF_MSG_TEXT      "Deleting if exists: %s\n"
#define ZSS_LOG_DS_DELETING_IF_MSG           ZSS_LOG_DS_DELETING_IF_MSG_ID" "ZSS_LOG_DS_DELETING_IF_MSG_TEXT

#ifndef ZSS_LOG_INSTALL_DS_CONT_MSG_ID
#define ZSS_LOG_INSTALL_DS_CONT_MSG_ID       LOG_MSG_PRFX"1042I"
#endif
#define ZSS_LOG_INSTALL_DS_CONT_MSG_TEXT     "Installing dataset contents service\n"
#define ZSS_LOG_INSTALL_DS_CONT_MSG          ZSS_LOG_INSTALL_DS_CONT_MSG_ID" "ZSS_LOG_INSTALL_DS_CONT_MSG_TEXT

#ifndef ZSS_LOG_INSTALL_VSAM_CONT_MSG_ID
#define ZSS_LOG_INSTALL_VSAM_CONT_MSG_ID     LOG_MSG_PRFX"1043I"
#endif
#define ZSS_LOG_INSTALL_VSAM_CONT_MSG_TEXT   "Installing VSAM dataset contents service\n"
#define ZSS_LOG_INSTALL_VSAM_CONT_MSG        ZSS_LOG_INSTALL_VSAM_CONT_MSG_ID" "ZSS_LOG_INSTALL_VSAM_CONT_MSG_TEXT

#ifndef ZSS_LOG_INSTALL_DS_MTADTA_MSG_ID
#define ZSS_LOG_INSTALL_DS_MTADTA_MSG_ID     LOG_MSG_PRFX"1044I"
#endif
#define ZSS_LOG_INSTALL_DS_MTADTA_MSG_TEXT   "Installing dataset metadata service\n"
#define ZSS_LOG_INSTALL_DS_MTADTA_MSG        ZSS_LOG_INSTALL_DS_MTADTA_MSG_ID" "ZSS_LOG_INSTALL_DS_MTADTA_MSG_TEXT

/* MVD Server (Discovery) */

#ifndef ZSS_LOG_DISC_THIRD_NGIVEN_MSG_ID
#define ZSS_LOG_DISC_THIRD_NGIVEN_MSG_ID     LOG_MSG_PRFX"1045W"
#endif
#define ZSS_LOG_DISC_THIRD_NGIVEN_MSG_TEXT   "zosDiscovery third level name not given\n"
#define ZSS_LOG_DISC_THIRD_NGIVEN_MSG        ZSS_LOG_DISC_THIRD_NGIVEN_MSG_ID" "ZSS_LOG_DISC_THIRD_NGIVEN_MSG_TEXT

#ifndef ZSS_LOG_DISC_THIRD_NKNOWN_MSG_ID
#define ZSS_LOG_DISC_THIRD_NKNOWN_MSG_ID     LOG_MSG_PRFX"1046W"
#endif
#define ZSS_LOG_DISC_THIRD_NKNOWN_MSG_TEXT   "zosDiscovery third level name not known %s\n"
#define ZSS_LOG_DISC_THIRD_NKNOWN_MSG        ZSS_LOG_DISC_THIRD_NKNOWN_MSG_ID" "ZSS_LOG_DISC_THIRD_NKNOWN_MSG_TEXT

#ifndef ZSS_LOG_DISC_SECND_NGIVEN_MSG_ID
#define ZSS_LOG_DISC_SECND_NGIVEN_MSG_ID     LOG_MSG_PRFX"1047W"
#endif
#define ZSS_LOG_DISC_SECND_NGIVEN_MSG_TEXT   "zosDiscovery second level name not given\n"
#define ZSS_LOG_DISC_SECND_NGIVEN_MSG        ZSS_LOG_DISC_SECND_NGIVEN_MSG_ID" "ZSS_LOG_DISC_SECND_NGIVEN_MSG_TEXT

#ifndef ZSS_LOG_DISC_SECND_NKNOWN_MSG_ID
#define ZSS_LOG_DISC_SECND_NKNOWN_MSG_ID     LOG_MSG_PRFX"1048W"
#endif
#define ZSS_LOG_DISC_SECND_NKNOWN_MSG_TEXT   "zosDiscovery second level name not known %s\n"
#define ZSS_LOG_DISC_SECND_NKNOWN_MSG        ZSS_LOG_DISC_SECND_NKNOWN_MSG_ID" "ZSS_LOG_DISC_SECND_NKNOWN_MSG_TEXT

#ifndef ZSS_LOG_DISC_FIRST_NKNOWN_MSG_ID
#define ZSS_LOG_DISC_FIRST_NKNOWN_MSG_ID     LOG_MSG_PRFX"1049W"
#endif
#define ZSS_LOG_DISC_FIRST_NKNOWN_MSG_TEXT   "zosDiscovery first level name not known %s\n"
#define ZSS_LOG_DISC_FIRST_NKNOWN_MSG        ZSS_LOG_DISC_FIRST_NKNOWN_MSG_ID" "ZSS_LOG_DISC_FIRST_NKNOWN_MSG_TEXT

/* MVD Server (ServerStatusService) */

#ifndef ZSS_LOG_FAIL_RMF_FETCH_MSG_ID
#define ZSS_LOG_FAIL_RMF_FETCH_MSG_ID        LOG_MSG_PRFX"1050W"
#endif
#define ZSS_LOG_FAIL_RMF_FETCH_MSG_TEXT      "Failed to fetch RMF data, RC = %d\n"
#define ZSS_LOG_FAIL_RMF_FETCH_MSG           ZSS_LOG_FAIL_RMF_FETCH_MSG_ID" "ZSS_LOG_FAIL_RMF_FETCH_MSG_TEXT

/* Unixfile */

#ifndef ZSS_LOG_UNABLE_CLOSE_MSG_ID
#define ZSS_LOG_UNABLE_CLOSE_MSG_ID          LOG_MSG_PRFX"1200W"
#endif
#define ZSS_LOG_UNABLE_CLOSE_MSG_TEXT        "Could not close file. Ret: %d, Res: %d\n"
#define ZSS_LOG_UNABLE_CLOSE_MSG             ZSS_LOG_UNABLE_CLOSE_MSG_ID" "ZSS_LOG_UNABLE_CLOSE_MSG_TEXT

#ifndef ZSS_LOG_UNABLE_CREATE_MSG_ID
#define ZSS_LOG_UNABLE_CREATE_MSG_ID         LOG_MSG_PRFX"1201W"
#endif
#define ZSS_LOG_UNABLE_CREATE_MSG_TEXT       "Could not create file. Ret: %d, Res: %d\n"
#define ZSS_LOG_UNABLE_CREATE_MSG            ZSS_LOG_UNABLE_CREATE_MSG_ID" "ZSS_LOG_UNABLE_CREATE_MSG_TEXT

#ifndef ZSS_LOG_UNABLE_METADATA_MSG_ID
#define ZSS_LOG_UNABLE_METADATA_MSG_ID       LOG_MSG_PRFX"1202W"
#endif
#define ZSS_LOG_UNABLE_METADATA_MSG_TEXT     "Could not get metadata for file. Ret: %d, Res: %d\n"
#define ZSS_LOG_UNABLE_METADATA_MSG          ZSS_LOG_UNABLE_METADATA_MSG_ID" "ZSS_LOG_UNABLE_METADATA_MSG_TEXT

#ifndef ZSS_LOG_UNABLE_OPEN_MSG_ID
#define ZSS_LOG_UNABLE_OPEN_MSG_ID           LOG_MSG_PRFX"1203W"
#endif
#define ZSS_LOG_UNABLE_OPEN_MSG_TEXT         "Could not open file. Ret: %d, Res: %d\n"
#define ZSS_LOG_UNABLE_OPEN_MSG              ZSS_LOG_UNABLE_OPEN_MSG_ID" "ZSS_LOG_UNABLE_OPEN_MSG_TEXT

#ifndef ZSS_LOG_TTYPE_NOT_SET_MSG_ID
#define ZSS_LOG_TTYPE_NOT_SET_MSG_ID         LOG_MSG_PRFX"1203W"
#endif
#define ZSS_LOG_TTYPE_NOT_SET_MSG_TEXT       "Transfer type hasn't been set."
#define ZSS_LOG_TTYPE_NOT_SET_MSG            ZSS_LOG_TTYPE_NOT_SET_MSG_ID" "ZSS_LOG_TTYPE_NOT_SET_MSG_TEXT

/* Security */

#ifndef ZSS_LOG_NON_STRD_GET_MSG_ID
#define ZSS_LOG_NON_STRD_GET_MSG_ID          LOG_MSG_PRFX"1400W"
#endif
#define ZSS_LOG_NON_STRD_GET_MSG_TEXT        "non standard class provided for profiles GET, leaving...\n"
#define ZSS_LOG_NON_STRD_GET_MSG             ZSS_LOG_NON_STRD_GET_MSG_ID" "ZSS_LOG_NON_STRD_GET_MSG_TEXT

#ifndef ZSS_LOG_PROF_REQ_GET_MSG_ID
#define ZSS_LOG_PROF_REQ_GET_MSG_ID          LOG_MSG_PRFX"1401W"
#endif
#define ZSS_LOG_PROF_REQ_GET_MSG_TEXT        "profile not provided for profiles GET, leaving...\n"
#define ZSS_LOG_PROF_REQ_GET_MSG             ZSS_LOG_PROF_REQ_GET_MSG_ID" "ZSS_LOG_PROF_REQ_GET_MSG_TEXT

#ifndef ZSS_LOG_NON_STRD_POST_MSG_ID
#define ZSS_LOG_NON_STRD_POST_MSG_ID         LOG_MSG_PRFX"1402W"
#endif
#define ZSS_LOG_NON_STRD_POST_MSG_TEXT       "non standard class provided for profiles POST, leaving...\n"
#define ZSS_LOG_NON_STRD_POST_MSG            ZSS_LOG_NON_STRD_POST_MSG_ID" "ZSS_LOG_NON_STRD_POST_MSG_TEXT

#ifndef ZSS_LOG_PROF_REQ_POST_MSG_ID
#define ZSS_LOG_PROF_REQ_POST_MSG_ID         LOG_MSG_PRFX"1403W"
#endif
#define ZSS_LOG_PROF_REQ_POST_MSG_TEXT       "profile name required for profile POST\n"
#define ZSS_LOG_PROF_REQ_POST_MSG            ZSS_LOG_PROF_REQ_POST_MSG_ID" "ZSS_LOG_PROF_REQ_POST_MSG_TEXT

#ifndef ZSS_LOG_NON_STRD_DEL_MSG_ID
#define ZSS_LOG_NON_STRD_DEL_MSG_ID          LOG_MSG_PRFX"1404W"
#endif
#define ZSS_LOG_NON_STRD_DEL_MSG_TEXT        "non standard class provided for profiles DELETE, leaving...\n"
#define ZSS_LOG_NON_STRD_DEL_MSG             ZSS_LOG_NON_STRD_DEL_MSG_ID" "ZSS_LOG_NON_STRD_DEL_MSG_TEXT

#ifndef ZSS_LOG_PROF_REQ_DEL_MSG_ID
#define ZSS_LOG_PROF_REQ_DEL_MSG_ID          LOG_MSG_PRFX"1405W"
#endif
#define ZSS_LOG_PROF_REQ_DEL_MSG_TEXT        "profile name required for profile DELETE\n"
#define ZSS_LOG_PROF_REQ_DEL_MSG             ZSS_LOG_PROF_REQ_DEL_MSG_ID" "ZSS_LOG_PROF_REQ_DEL_MSG_TEXT

#ifndef ZSS_LOG_NON_STRD_USER_PP_MSG_ID
#define ZSS_LOG_NON_STRD_USER_PP_MSG_ID      LOG_MSG_PRFX"1406W"
#endif
#define ZSS_LOG_NON_STRD_USER_PP_MSG_TEXT    "non standard class provided for user POST/PUT, leaving...\n"
#define ZSS_LOG_NON_STRD_USER_PP_MSG         ZSS_LOG_NON_STRD_USER_PP_MSG_ID" "ZSS_LOG_NON_STRD_USER_PP_MSG_TEXT

#ifndef ZSS_LOG_PROF_REQ_USER_PP_MSG_ID
#define ZSS_LOG_PROF_REQ_USER_PP_MSG_ID      LOG_MSG_PRFX"1407W"
#endif
#define ZSS_LOG_PROF_REQ_USER_PP_MSG_TEXT    "profile name required for user POST/PUT\n"
#define ZSS_LOG_PROF_REQ_USER_PP_MSG         ZSS_LOG_PROF_REQ_USER_PP_MSG_ID" "ZSS_LOG_PROF_REQ_USER_PP_MSG_TEXT

#ifndef ZSS_LOG_USER_REQ_USER_PP_MSG_ID
#define ZSS_LOG_USER_REQ_USER_PP_MSG_ID      LOG_MSG_PRFX"1408W"
#endif
#define ZSS_LOG_USER_REQ_USER_PP_MSG_TEXT    "user ID required for user POST/PUT\n"
#define ZSS_LOG_USER_REQ_USER_PP_MSG         ZSS_LOG_USER_REQ_USER_PP_MSG_ID" "ZSS_LOG_USER_REQ_USER_PP_MSG_TEXT

#ifndef ZSS_LOG_BODY_NPROV_USER_PP_MSG_ID
#define ZSS_LOG_BODY_NPROV_USER_PP_MSG_ID    LOG_MSG_PRFX"1409W"
#endif
#define ZSS_LOG_BODY_NPROV_USER_PP_MSG_TEXT  "body not provided for user POST/PUT, leaving...\n"
#define ZSS_LOG_BODY_NPROV_USER_PP_MSG       ZSS_LOG_BODY_NPROV_USER_PP_MSG_ID" "ZSS_LOG_BODY_NPROV_USER_PP_MSG_TEXT

#ifndef ZSS_LOG_BAD_TYPE_USER_PP_MSG_ID
#define ZSS_LOG_BAD_TYPE_USER_PP_MSG_ID      LOG_MSG_PRFX"1410W"
#endif
#define ZSS_LOG_BAD_TYPE_USER_PP_MSG_TEXT    "bad access type provided for user POST/PUT, leaving...\n"
#define ZSS_LOG_BAD_TYPE_USER_PP_MSG         ZSS_LOG_BAD_TYPE_USER_PP_MSG_ID" "ZSS_LOG_BAD_TYPE_USER_PP_MSG_TEXT

#ifndef ZSS_LOG_UNK_TYPE_USER_PP_MSG_ID
#define ZSS_LOG_UNK_TYPE_USER_PP_MSG_ID      LOG_MSG_PRFX"1411W"
#endif
#define ZSS_LOG_UNK_TYPE_USER_PP_MSG_TEXT    "unknown access type (%d( provided for user POST/PUT, leaving...\n"
#define ZSS_LOG_UNK_TYPE_USER_PP_MSG         ZSS_LOG_UNK_TYPE_USER_PP_MSG_ID" "ZSS_LOG_UNK_TYPE_USER_PP_MSG_TEXT

#ifndef ZSS_LOG_NON_STRD_ACCESS_GET_MSG_ID
#define ZSS_LOG_NON_STRD_ACCESS_GET_MSG_ID   LOG_MSG_PRFX"1412W"
#endif
#define ZSS_LOG_NON_STRD_ACCESS_GET_MSG_TEXT "non standard class provided for access list GET, leaving...\n"
#define ZSS_LOG_NON_STRD_ACCESS_GET_MSG      ZSS_LOG_NON_STRD_ACCESS_GET_MSG_ID" "ZSS_LOG_NON_STRD_ACCESS_GET_MSG_TEXT

#ifndef ZSS_LOG_ACCESS_LIST_BULK_MSG_ID
#define ZSS_LOG_ACCESS_LIST_BULK_MSG_ID      LOG_MSG_PRFX"1413W"
#endif
#define ZSS_LOG_ACCESS_LIST_BULK_MSG_TEXT    "access list can only be retrieved in bulk, leaving...\n"
#define ZSS_LOG_ACCESS_LIST_BULK_MSG         ZSS_LOG_ACCESS_LIST_BULK_MSG_ID" "ZSS_LOG_ACCESS_LIST_BULK_MSG_TEXT

#ifndef ZSS_LOG_ACCESS_LIST_BUFR_MSG_ID
#define ZSS_LOG_ACCESS_LIST_BUFR_MSG_ID      LOG_MSG_PRFX"1414W"
#endif
#define ZSS_LOG_ACCESS_LIST_BUFR_MSG_TEXT    "access list buffer with size %u not allocated, leaving...\n"
#define ZSS_LOG_ACCESS_LIST_BUFR_MSG         ZSS_LOG_ACCESS_LIST_BUFR_MSG_ID" "ZSS_LOG_ACCESS_LIST_BUFR_MSG_TEXT

#ifndef ZSS_LOG_ACCESS_LIST_OORG_MSG_ID
#define ZSS_LOG_ACCESS_LIST_OORG_MSG_ID      LOG_MSG_PRFX"1415W"
#endif
#define ZSS_LOG_ACCESS_LIST_OORG_MSG_TEXT    "access list size out of range (%u), leaving...\n"
#define ZSS_LOG_ACCESS_LIST_OORG_MSG         ZSS_LOG_ACCESS_LIST_OORG_MSG_ID" "ZSS_LOG_ACCESS_LIST_OORG_MSG_TEXT

#ifndef ZSS_LOG_NON_STRD_ACCESS_DEL_MSG_ID
#define ZSS_LOG_NON_STRD_ACCESS_DEL_MSG_ID   LOG_MSG_PRFX"1416W"
#endif
#define ZSS_LOG_NON_STRD_ACCESS_DEL_MSG_TEXT "non standard class provided for access list DELETE, leaving...\n"
#define ZSS_LOG_NON_STRD_ACCESS_DEL_MSG      ZSS_LOG_NON_STRD_ACCESS_DEL_MSG_ID" "ZSS_LOG_NON_STRD_ACCESS_DEL_MSG_TEXT

#ifndef ZSS_LOG_PROF_REQ_ACCESS_DEL_MSG_ID
#define ZSS_LOG_PROF_REQ_ACCESS_DEL_MSG_ID   LOG_MSG_PRFX"1417W"
#endif
#define ZSS_LOG_PROF_REQ_ACCESS_DEL_MSG_TEXT "profile name required for access list  DELETE\n"
#define ZSS_LOG_PROF_REQ_ACCESS_DEL_MSG      ZSS_LOG_PROF_REQ_ACCESS_DEL_MSG_ID" "ZSS_LOG_PROF_REQ_ACCESS_DEL_MSG_TEXT

#ifndef ZSS_LOG_ACCESS_REQ_ACCESS_DEL_MSG_ID
#define ZSS_LOG_ACCESS_REQ_ACCESS_DEL_MSG_ID LOG_MSG_PRFX"1418W"
#endif
#define ZSS_LOG_ACCESS_REQ_ACCESS_DEL_MSG_TEXT "access list entry name required for access list DELETE\n"
#define ZSS_LOG_ACCESS_REQ_ACCESS_DEL_MSG    ZSS_LOG_ACCESS_REQ_ACCESS_DEL_MSG_ID" "ZSS_LOG_ACCESS_REQ_ACCESS_DEL_MSG_TEXT

#ifndef ZSS_LOG_CLASS_MGMT_QRY_DEL_MSG_ID
#define ZSS_LOG_CLASS_MGMT_QRY_DEL_MSG_ID LOG_MSG_PRFX"1419W"
#endif
#define ZSS_LOG_CLASS_MGMT_QRY_DEL_MSG_TEXT  "class-mgmt query string is invalid, leaving...\n"
#define ZSS_LOG_CLASS_MGMT_QRY_DEL_MSG       ZSS_LOG_CLASS_MGMT_QRY_DEL_MSG_ID" "ZSS_LOG_CLASS_MGMT_QRY_DEL_MSG_TEXT

#ifndef ZSS_LOG_GROUP_REQ_PROF_POST_MSG_ID
#define ZSS_LOG_GROUP_REQ_PROF_POST_MSG_ID LOG_MSG_PRFX"1420W"
#endif
#define ZSS_LOG_GROUP_REQ_PROF_POST_MSG_TEXT "group name required for profile POST\n"
#define ZSS_LOG_GROUP_REQ_PROF_POST_MSG      ZSS_LOG_GROUP_REQ_PROF_POST_MSG_ID" "ZSS_LOG_GROUP_REQ_PROF_POST_MSG_TEXT

#ifndef ZSS_LOG_BODY_REQ_GRP_POST_MSG_ID
#define ZSS_LOG_BODY_REQ_GRP_POST_MSG_ID LOG_MSG_PRFX"1421W"
#endif
#define ZSS_LOG_BODY_REQ_GRP_POST_MSG_TEXT   "body not provided for group POST, leaving...\n"
#define ZSS_LOG_BODY_REQ_GRP_POST_MSG        ZSS_LOG_BODY_REQ_GRP_POST_MSG_ID" "ZSS_LOG_BODY_REQ_GRP_POST_MSG_TEXT

#ifndef ZSS_LOG_SPRR_REQ_GRP_POST_MSG_ID
#define ZSS_LOG_SPRR_REQ_GRP_POST_MSG_ID LOG_MSG_PRFX"1422W"
#endif
#define ZSS_LOG_SPRR_REQ_GRP_POST_MSG_TEXT   "superior not provided for group POST, leaving...\n"
#define ZSS_LOG_SPRR_REQ_GRP_POST_MSG        ZSS_LOG_SPRR_REQ_GRP_POST_MSG_ID" "ZSS_LOG_SPRR_REQ_GRP_POST_MSG_TEXT

#ifndef ZSS_LOG_BSPR_PROV_GRP_POST_MSG_ID
#define ZSS_LOG_BSPR_PROV_GRP_POST_MSG_ID LOG_MSG_PRFX"1423W"
#endif
#define ZSS_LOG_BSPR_PROV_GRP_POST_MSG_TEXT  "bad superior group provided for group POST, leaving...\n"
#define ZSS_LOG_BSPR_PROV_GRP_POST_MSG       ZSS_LOG_BSPR_PROV_GRP_POST_MSG_ID" "ZSS_LOG_BSPR_PROV_GRP_POST_MSG_TEXT

#ifndef ZSS_LOG_GROUP_REQ_USER_PP_MSG_ID
#define ZSS_LOG_GROUP_REQ_USER_PP_MSG_ID LOG_MSG_PRFX"1424W"
#endif
#define ZSS_LOG_GROUP_REQ_USER_PP_MSG_TEXT   "group name required for user POST/PUT\n"
#define ZSS_LOG_GROUP_REQ_USER_PP_MSG        ZSS_LOG_GROUP_REQ_USER_PP_MSG_ID" "ZSS_LOG_GROUP_REQ_USER_PP_MSG_TEXT

#ifndef ZSS_LOG_ACCESS_REQ_USER_PP_MSG_ID
#define ZSS_LOG_ACCESS_REQ_USER_PP_MSG_ID LOG_MSG_PRFX"1425W"
#endif
#define ZSS_LOG_ACCESS_REQ_USER_PP_MSG_TEXT  "access type not provided for user POST/PUT, leaving...\n"
#define ZSS_LOG_ACCESS_REQ_USER_PP_MSG       ZSS_LOG_ACCESS_REQ_USER_PP_MSG_ID" "ZSS_LOG_ACCESS_REQ_USER_PP_MSG_TEXT

#ifndef ZSS_LOG_UNK_TYPE_UCCJ_MSG_ID
#define ZSS_LOG_UNK_TYPE_UCCJ_MSG_ID LOG_MSG_PRFX"1426W"
#endif
#define ZSS_LOG_UNK_TYPE_UCCJ_MSG_TEXT       "unknown access type, use [USE, CREATE, CONNECT, JOIN]"
#define ZSS_LOG_UNK_TYPE_UCCJ_MSG            ZSS_LOG_UNK_TYPE_UCCJ_MSG_ID" "ZSS_LOG_UNK_TYPE_UCCJ_MSG_TEXT

#ifndef ZSS_LOG_LIST_REALLOC_MSG_ID
#define ZSS_LOG_LIST_REALLOC_MSG_ID LOG_MSG_PRFX"1427W"
#endif
#define ZSS_LOG_LIST_REALLOC_MSG_TEXT        "access list will be re-allocated with capacity %u\n"
#define ZSS_LOG_LIST_REALLOC_MSG             ZSS_LOG_LIST_REALLOC_MSG_ID" "ZSS_LOG_LIST_REALLOC_MSG_TEXT

#ifndef ZSS_LOG_GROUP_REQ_LIST_DEL_MSG_ID
#define ZSS_LOG_GROUP_REQ_LIST_DEL_MSG_ID LOG_MSG_PRFX"1428W"
#endif
#define ZSS_LOG_GROUP_REQ_LIST_DEL_MSG_TEXT  "group name required for access list  DELETE\n"
#define ZSS_LOG_GROUP_REQ_LIST_DEL_MSG       ZSS_LOG_GROUP_REQ_LIST_DEL_MSG_ID" "ZSS_LOG_GROUP_REQ_LIST_DEL_MSG_TEXT

#ifndef ZSS_LOG_GROUP_MGMT_QRY_DEL_MSG_ID
#define ZSS_LOG_GROUP_MGMT_QRY_DEL_MSG_ID LOG_MSG_PRFX"1429W"
#endif
#define ZSS_LOG_GROUP_MGMT_QRY_DEL_MSG_TEXT  "group-mgmt query string is invalid, leaving...\n"
#define ZSS_LOG_GROUP_MGMT_QRY_DEL_MSG       ZSS_LOG_GROUP_MGMT_QRY_DEL_MSG_ID" "ZSS_LOG_GROUP_MGMT_QRY_DEL_MSG_TEXT

#endif /* MVD_H_ZSSLOGGING_H_ */


/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

