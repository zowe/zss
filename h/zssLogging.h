

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

#ifndef ZSS_LOG_SUBCOMP_ID
#define ZSS_LOG_SUBCOMP_ID  "S"
#endif

#ifndef ZSS_LOG_PROD_ID
#define ZSS_LOG_PROD_ID ZSS_LOG_ID ZSS_LOG_SUBCOMP_ID
#endif

#ifndef ZSS_LOG_MSG_SUBCOMP
#define ZSS_LOG_MSG_SUBCOMP ZSS_LOG_SUBCOMP_ID
#endif

#ifndef ZSS_LOG_MSG_PRFX
#define ZSS_LOG_MSG_PRFX  ZSS_LOG_ID ZSS_LOG_MSG_SUBCOMP
#endif

#define LOG_COMP_ID_MVD_SERVER    0x008F000300010000
#define LOG_COMP_ID_CTDS          0x008F000300020000
#define LOG_COMP_ID_SECURITY      0x008F000300030000
#define LOG_COMP_ID_UNIXFILE      0x008F000300040000
#define LOG_COMP_ID_DATASERVICE   0x008F000300050000
#define LOG_COMP_ID_APIML_STORAGE 0x008F000300060000
#define LOG_COMP_ID_JWK           0x008F000300070000

bool isLogLevelValid(int level);

/* default message IDs */

/* 0000 - 0999 are messages reserved for for crossmemory (see crossmemory.h) */
/* 1000 - 1199 are messages from MVD_SERVER */
/* 1200 - 1399 are messages from UNIXFILE */
/* 1400 - 1599 are messages from SECURITY */
/* 1600 - 1799 are messages from CTDS */

/* MVD Server */

#ifndef ZSS_LOG_PRV_SRV_NAME_MSG_ID
#define ZSS_LOG_PRV_SRV_NAME_MSG_ID          ZSS_LOG_MSG_PRFX"1000W"
#endif
#define ZSS_LOG_PRV_SRV_NAME_MSG_TEXT        "Privileged server name not provided, falling back to default.\n"
#define ZSS_LOG_PRV_SRV_NAME_MSG             ZSS_LOG_PRV_SRV_NAME_MSG_ID" "ZSS_LOG_PRV_SRV_NAME_MSG_TEXT

#ifndef ZSS_LOG_INC_LOG_LEVEL_MSG_ID
#define ZSS_LOG_INC_LOG_LEVEL_MSG_ID         ZSS_LOG_MSG_PRFX"1001E"
#endif
#define ZSS_LOG_INC_LOG_LEVEL_MSG_TEXT       "Log level '%d' is incorrect.\n"
#define ZSS_LOG_INC_LOG_LEVEL_MSG            ZSS_LOG_INC_LOG_LEVEL_MSG_ID" "ZSS_LOG_INC_LOG_LEVEL_MSG_TEXT

#ifndef ZSS_LOG_FILE_EXPECTED_TOP_MSG_ID
#define ZSS_LOG_FILE_EXPECTED_TOP_MSG_ID     ZSS_LOG_MSG_PRFX"1002E"
#endif
#define ZSS_LOG_FILE_EXPECTED_TOP_MSG_TEXT   "Error in timeouts file: Could not parse config file as a JSON object.\n"
#define ZSS_LOG_FILE_EXPECTED_TOP_MSG        ZSS_LOG_FILE_EXPECTED_TOP_MSG_ID" "ZSS_LOG_FILE_EXPECTED_TOP_MSG_TEXT

#ifndef ZSS_LOG_PARS_ZSS_SETTING_MSG_ID
#define ZSS_LOG_PARS_ZSS_SETTING_MSG_ID      ZSS_LOG_MSG_PRFX"1003E"
#endif
#define ZSS_LOG_PARS_ZSS_SETTING_MSG_TEXT    "Error while parsing ZSS settings from config file '%s': '%s'\n"
#define ZSS_LOG_PARS_ZSS_SETTING_MSG         ZSS_LOG_PARS_ZSS_SETTING_MSG_ID" "ZSS_LOG_PARS_ZSS_SETTING_MSG_TEXT

#ifndef ZSS_LOG_EXPEC_PLUGIN_ID_MSG_ID
#define ZSS_LOG_EXPEC_PLUGIN_ID_MSG_ID       ZSS_LOG_MSG_PRFX"1004I"
#endif
#define ZSS_LOG_EXPEC_PLUGIN_ID_MSG_TEXT     "Expected plugin ID '%s', instead received '%s'\n"
#define ZSS_LOG_EXPEC_PLUGIN_ID_MSG          ZSS_LOG_EXPEC_PLUGIN_ID_MSG_ID" "ZSS_LOG_EXPEC_PLUGIN_ID_MSG_TEXT

#ifndef ZSS_LOG_PLUGIN_ID_NFOUND_MSG_ID
#define ZSS_LOG_PLUGIN_ID_NFOUND_MSG_ID      ZSS_LOG_MSG_PRFX"1005I"
#endif
#define ZSS_LOG_PLUGIN_ID_NFOUND_MSG_TEXT    "Plugin ID was not found in '%s'\n"
#define ZSS_LOG_PLUGIN_ID_NFOUND_MSG         ZSS_LOG_PLUGIN_ID_NFOUND_MSG_ID" "ZSS_LOG_PLUGIN_ID_NFOUND_MSG_TEXT

#ifndef ZSS_LOG_PARS_FILE_MSG_ID
#define ZSS_LOG_PARS_FILE_MSG_ID             ZSS_LOG_MSG_PRFX"1006E"
#endif
#define ZSS_LOG_PARS_FILE_MSG_TEXT           "Error while parsing file '%s': '%s'\n"
#define ZSS_LOG_PARS_FILE_MSG                ZSS_LOG_PARS_FILE_MSG_ID" "ZSS_LOG_PARS_FILE_MSG_TEXT

#ifndef ZSS_LOG_WEB_CONT_NFOUND_MSG_ID
#define ZSS_LOG_WEB_CONT_NFOUND_MSG_ID       ZSS_LOG_MSG_PRFX"1007I"
#endif
#define ZSS_LOG_WEB_CONT_NFOUND_MSG_TEXT     "webContent was not found in plugin definition for '%s'\n"
#define ZSS_LOG_WEB_CONT_NFOUND_MSG          ZSS_LOG_WEB_CONT_NFOUND_MSG_ID" "ZSS_LOG_WEB_CONT_NFOUND_MSG_TEXT

#ifndef ZSS_LOG_LIBR_VER_NFOUND_MSG_ID
#define ZSS_LOG_LIBR_VER_NFOUND_MSG_ID       ZSS_LOG_MSG_PRFX"1008I"
#endif
#define ZSS_LOG_LIBR_VER_NFOUND_MSG_TEXT     "libraryVersion was not found in plugin definition for '%s'\n"
#define ZSS_LOG_LIBR_VER_NFOUND_MSG          ZSS_LOG_LIBR_VER_NFOUND_MSG_ID" "ZSS_LOG_LIBR_VER_NFOUND_MSG_TEXT

#ifndef ZSS_LOG_PLUGIN_ID_NULL_MSG_ID
#define ZSS_LOG_PLUGIN_ID_NULL_MSG_ID        ZSS_LOG_MSG_PRFX"1009W"
#endif
#define ZSS_LOG_PLUGIN_ID_NULL_MSG_TEXT      "Plugin ID '%s' is NULL and cannot be loaded.\n"
#define ZSS_LOG_PLUGIN_ID_NULL_MSG           ZSS_LOG_PLUGIN_ID_NULL_MSG_ID" "ZSS_LOG_PLUGIN_ID_NULL_MSG_TEXT

#ifndef ZSS_LOG_PLUGIN_IDLOC_NFOUND_MSG_ID
#define ZSS_LOG_PLUGIN_IDLOC_NFOUND_MSG_ID   ZSS_LOG_MSG_PRFX"1010I"
#endif
#define ZSS_LOG_PLUGIN_IDLOC_NFOUND_MSG_TEXT "Plugin ID and/or location was not found in '%s'\n"
#define ZSS_LOG_PLUGIN_IDLOC_NFOUND_MSG      ZSS_LOG_PLUGIN_IDLOC_NFOUND_MSG_ID" "ZSS_LOG_PLUGIN_IDLOC_NFOUND_MSG_TEXT

#ifndef ZSS_LOG_PARS_GENERIC_MSG_ID
#define ZSS_LOG_PARS_GENERIC_MSG_ID          ZSS_LOG_MSG_PRFX"1011E"
#endif
#define ZSS_LOG_PARS_GENERIC_MSG_TEXT        "Error while parsing: '%s'\n"
#define ZSS_LOG_PARS_GENERIC_MSG             ZSS_LOG_PARS_GENERIC_MSG_ID" "ZSS_LOG_PARS_GENERIC_MSG_TEXT

#ifndef ZSS_LOG_OPEN_DIR_FAIL_MSG_ID
#define ZSS_LOG_OPEN_DIR_FAIL_MSG_ID         ZSS_LOG_MSG_PRFX"1012W"
#endif
#define ZSS_LOG_OPEN_DIR_FAIL_MSG_TEXT       "Could not open directory '%s': Ret='%d', res='0x%x'\n"
#define ZSS_LOG_OPEN_DIR_FAIL_MSG            ZSS_LOG_OPEN_DIR_FAIL_MSG_ID" "ZSS_LOG_OPEN_DIR_FAIL_MSG_TEXT

#ifndef ZSS_LOG_ZSS_START_VER_MSG_ID
#define ZSS_LOG_ZSS_START_VER_MSG_ID         ZSS_LOG_MSG_PRFX"1013I"
#endif
#define ZSS_LOG_ZSS_START_VER_MSG_TEXT       "ZSS Server has started. Version '%s'\n"
#define ZSS_LOG_ZSS_START_VER_MSG            ZSS_LOG_ZSS_START_VER_MSG_ID" "ZSS_LOG_ZSS_START_VER_MSG_TEXT

#ifndef ZSS_LOG_ZIS_STATUS_MSG_ID
#define ZSS_LOG_ZIS_STATUS_MSG_ID            ZSS_LOG_MSG_PRFX"1014I"
#endif
#define ZSS_LOG_ZIS_STATUS_MSG_TEXT          "ZIS status - '%s' (name='%.16s', cmsRC='%d', description='%s', clientVersion='%d')\n"
#define ZSS_LOG_ZIS_STATUS_MSG               ZSS_LOG_ZIS_STATUS_MSG_ID" "ZSS_LOG_ZIS_STATUS_MSG_TEXT

#ifndef ZSS_LOG_HTTPS_NO_IMPLEM_MSG_ID
#define ZSS_LOG_HTTPS_NO_IMPLEM_MSG_ID       ZSS_LOG_MSG_PRFX"1015W"
#endif
#define ZSS_LOG_HTTPS_NO_IMPLEM_MSG_TEXT     "*** Server has not implemented HTTPS! ***\n*** In production, please use 27.0.0.1 or localhost. Server is using '%s' ***\n"
#define ZSS_LOG_HTTPS_NO_IMPLEM_MSG          ZSS_LOG_HTTPS_NO_IMPLEM_MSG_ID" "ZSS_LOG_HTTPS_NO_IMPLEM_MSG_TEXT

#ifndef ZSS_LOG_CANT_VAL_PERMISS_MSG_ID
#define ZSS_LOG_CANT_VAL_PERMISS_MSG_ID      ZSS_LOG_MSG_PRFX"1016E"
#endif
#define ZSS_LOG_CANT_VAL_PERMISS_MSG_TEXT    "Cannot validate file permission, path is not defined.\n"
#define ZSS_LOG_CANT_VAL_PERMISS_MSG         ZSS_LOG_CANT_VAL_PERMISS_MSG_ID" "ZSS_LOG_CANT_VAL_PERMISS_MSG_TEXT

#ifndef ZSS_LOG_CANT_STAT_CONFIG_MSG_ID
#define ZSS_LOG_CANT_STAT_CONFIG_MSG_ID      ZSS_LOG_MSG_PRFX"1017E"
#endif
#define ZSS_LOG_CANT_STAT_CONFIG_MSG_TEXT    "Could not get file info on config path='%s': Ret='%d', res='%d'\n"
#define ZSS_LOG_CANT_STAT_CONFIG_MSG         ZSS_LOG_CANT_STAT_CONFIG_MSG_ID" "ZSS_LOG_CANT_STAT_CONFIG_MSG_TEXT

#ifndef ZSS_LOG_CONFIG_OPEN_PERMISS_MSG_ID
#define ZSS_LOG_CONFIG_OPEN_PERMISS_MSG_ID   ZSS_LOG_MSG_PRFX"1018E"
#endif
#define ZSS_LOG_CONFIG_OPEN_PERMISS_MSG_TEXT "Refusing to start. Group & other permissions are too open on config path='%s'\n"
#define ZSS_LOG_CONFIG_OPEN_PERMISS_MSG      ZSS_LOG_CONFIG_OPEN_PERMISS_MSG_ID" "ZSS_LOG_CONFIG_OPEN_PERMISS_MSG_TEXT

#ifndef ZSS_LOG_ENSURE_PERMISS_MSG_ID
#define ZSS_LOG_ENSURE_PERMISS_MSG_ID        ZSS_LOG_MSG_PRFX"1019E"
#endif
#define ZSS_LOG_ENSURE_PERMISS_MSG_TEXT      "Ensure group has no write (or execute, if file) permission. Ensure other has no permissions. Then, restart zssServer to retry.\n"
#define ZSS_LOG_ENSURE_PERMISS_MSG           ZSS_LOG_ENSURE_PERMISS_MSG_ID" "ZSS_LOG_ENSURE_PERMISS_MSG_TEXT

#ifndef ZSS_LOG_SKIP_PERMISS_MSG_ID
#define ZSS_LOG_SKIP_PERMISS_MSG_ID          ZSS_LOG_MSG_PRFX"1020E"
#endif
#define ZSS_LOG_SKIP_PERMISS_MSG_TEXT        "Skipping validation of file permissions: Disabled during compilation, using file '%s'.\n"
#define ZSS_LOG_SKIP_PERMISS_MSG             ZSS_LOG_SKIP_PERMISS_MSG_ID" "ZSS_LOG_SKIP_PERMISS_MSG_TEXT

#ifndef ZSS_LOG_PATH_UNDEF_MSG_ID
#define ZSS_LOG_PATH_UNDEF_MSG_ID            ZSS_LOG_MSG_PRFX"1021E"
#endif
#define ZSS_LOG_PATH_UNDEF_MSG_TEXT          "Cannot validate file permissions: Path is not defined.\n"
#define ZSS_LOG_PATH_UNDEF_MSG               ZSS_LOG_PATH_UNDEF_MSG_ID" "ZSS_LOG_PATH_UNDEF_MSG_TEXT

#ifndef ZSS_LOG_PATH_DIR_MSG_ID
#define ZSS_LOG_PATH_DIR_MSG_ID              ZSS_LOG_MSG_PRFX"1022E"
#endif
#define ZSS_LOG_PATH_DIR_MSG_TEXT            "Cannot validate file permissions: Path is for a directory and not a file.\n"
#define ZSS_LOG_PATH_DIR_MSG                 ZSS_LOG_PATH_DIR_MSG_ID" "ZSS_LOG_PATH_DIR_MSG_TEXT

#ifndef ZSS_LOG_NO_JWT_AGENT_MSG_ID
#define ZSS_LOG_NO_JWT_AGENT_MSG_ID          ZSS_LOG_MSG_PRFX"1023I"
#endif
#define ZSS_LOG_NO_JWT_AGENT_MSG_TEXT        "Will not accept JWTs: Agent configuration is missing.\n"
#define ZSS_LOG_NO_JWT_AGENT_MSG             ZSS_LOG_NO_JWT_AGENT_MSG_ID" "ZSS_LOG_NO_JWT_AGENT_MSG_TEXT

#ifndef ZSS_LOG_NO_JWT_CONFIG_MSG_ID
#define ZSS_LOG_NO_JWT_CONFIG_MSG_ID         ZSS_LOG_MSG_PRFX"1024I"
#endif
#define ZSS_LOG_NO_JWT_CONFIG_MSG_TEXT       "Will not accept JWTs: JWT keystore configuration is missing.\n"
#define ZSS_LOG_NO_JWT_CONFIG_MSG            ZSS_LOG_NO_JWT_CONFIG_MSG_ID" "ZSS_LOG_NO_JWT_CONFIG_MSG_TEXT

#ifndef ZSS_LOG_NO_JWT_DISABLED_MSG_ID
#define ZSS_LOG_NO_JWT_DISABLED_MSG_ID       ZSS_LOG_MSG_PRFX"1025I"
#endif
#define ZSS_LOG_NO_JWT_DISABLED_MSG_TEXT     "Will not accept JWTs: Disabled in the configuration.\n"
#define ZSS_LOG_NO_JWT_DISABLED_MSG          ZSS_LOG_NO_JWT_DISABLED_MSG_ID" "ZSS_LOG_NO_JWT_DISABLED_MSG_TEXT

#ifndef ZSS_LOG_JWT_CONFIG_MISSING_MSG_ID
#define ZSS_LOG_JWT_CONFIG_MISSING_MSG_ID    ZSS_LOG_MSG_PRFX"1026E"
#endif
#define ZSS_LOG_JWT_CONFIG_MISSING_MSG_TEXT  "JWT keystore configuration is missing.\n"
#define ZSS_LOG_JWT_CONFIG_MISSING_MSG       ZSS_LOG_JWT_CONFIG_MISSING_MSG_ID" "ZSS_LOG_JWT_CONFIG_MISSING_MSG_TEXT

#ifndef ZSS_LOG_JWT_KEYSTORE_UNKN_MSG_ID
#define ZSS_LOG_JWT_KEYSTORE_UNKN_MSG_ID     ZSS_LOG_MSG_PRFX"1027E"
#endif
#define ZSS_LOG_JWT_KEYSTORE_UNKN_MSG_TEXT   "Invalid JWT configuration: Unknown keystore type '%s'\n"
#define ZSS_LOG_JWT_KEYSTORE_UNKN_MSG        ZSS_LOG_JWT_KEYSTORE_UNKN_MSG_ID" "ZSS_LOG_JWT_KEYSTORE_UNKN_MSG_TEXT

#ifndef ZSS_LOG_JWT_KEYSTORE_NAME_MSG_ID
#define ZSS_LOG_JWT_KEYSTORE_NAME_MSG_ID     ZSS_LOG_MSG_PRFX"1028E"
#endif
#define ZSS_LOG_JWT_KEYSTORE_NAME_MSG_TEXT   "Invalid JWT configuration: Keystore name is missing.\n"
#define ZSS_LOG_JWT_KEYSTORE_NAME_MSG        ZSS_LOG_JWT_KEYSTORE_NAME_MSG_ID" "ZSS_LOG_JWT_KEYSTORE_NAME_MSG_TEXT

#ifndef ZSS_LOG_NO_LOAD_JWT_MSG_ID
#define ZSS_LOG_NO_LOAD_JWT_MSG_ID           ZSS_LOG_MSG_PRFX"1030W"
#endif
#define ZSS_LOG_NO_LOAD_JWT_MSG_TEXT         "Server startup problem: Could not load the JWT key '%s' from token '%s': rc '%d', p11rc '%d', p11Rsn '%d'\n"
#define ZSS_LOG_NO_LOAD_JWT_MSG              ZSS_LOG_NO_LOAD_JWT_MSG_ID" "ZSS_LOG_NO_LOAD_JWT_MSG_TEXT

#ifndef ZSS_LOG_PATH_TO_SERVER_MSG_ID
#define ZSS_LOG_PATH_TO_SERVER_MSG_ID        ZSS_LOG_MSG_PRFX"1031W"
#endif
#define ZSS_LOG_PATH_TO_SERVER_MSG_TEXT      "Correct usage: zssServer <path to server.json file>\n"
#define ZSS_LOG_PATH_TO_SERVER_MSG           ZSS_LOG_PATH_TO_SERVER_MSG_ID" "ZSS_LOG_PATH_TO_SERVER_MSG_TEXT

#ifndef ZSS_LOG_SIG_IGNORE_MSG_ID
#define ZSS_LOG_SIG_IGNORE_MSG_ID            ZSS_LOG_MSG_PRFX"1032W"
#endif
#define ZSS_LOG_SIG_IGNORE_MSG_TEXT          "Problem encountered: Sigignore(SIGPIPE)='%d', errno='%d'\n"
#define ZSS_LOG_SIG_IGNORE_MSG               ZSS_LOG_SIG_IGNORE_MSG_ID" "ZSS_LOG_SIG_IGNORE_MSG_TEXT

#ifndef ZSS_LOG_SERVER_CONFIG_MSG_ID
#define ZSS_LOG_SERVER_CONFIG_MSG_ID         ZSS_LOG_MSG_PRFX"1033I"
#endif
#define ZSS_LOG_SERVER_CONFIG_MSG_TEXT       "Using server config file '%s'\n"
#define ZSS_LOG_SERVER_CONFIG_MSG            ZSS_LOG_SERVER_CONFIG_MSG_ID" "ZSS_LOG_SERVER_CONFIG_MSG_TEXT

#ifndef ZSS_LOG_SERVER_STARTUP_MSG_ID
#define ZSS_LOG_SERVER_STARTUP_MSG_ID        ZSS_LOG_MSG_PRFX"1034W"
#endif
#define ZSS_LOG_SERVER_STARTUP_MSG_TEXT      "Server startup problem: Address '%s' not valid\n"
#define ZSS_LOG_SERVER_STARTUP_MSG           ZSS_LOG_SERVER_STARTUP_MSG_ID" "ZSS_LOG_SERVER_STARTUP_MSG_TEXT

#ifndef ZSS_LOG_ZSS_SETTINGS_MSG_ID
#define ZSS_LOG_ZSS_SETTINGS_MSG_ID          ZSS_LOG_MSG_PRFX"1035I"
#endif
#define ZSS_LOG_ZSS_SETTINGS_MSG_TEXT        "ZSS Server settings: Address='%s', port='%d', protocol='%s'\n"
#define ZSS_LOG_ZSS_SETTINGS_MSG             ZSS_LOG_ZSS_SETTINGS_MSG_ID" "ZSS_LOG_ZSS_SETTINGS_MSG_TEXT

#ifndef ZSS_LOG_ZSS_STARTUP_MSG_ID
#define ZSS_LOG_ZSS_STARTUP_MSG_ID           ZSS_LOG_MSG_PRFX"1036W"
#endif
#define ZSS_LOG_ZSS_STARTUP_MSG_TEXT         "Server startup problem: Ret='%d', res='0x%x'\n"
#define ZSS_LOG_ZSS_STARTUP_MSG              ZSS_LOG_ZSS_STARTUP_MSG_ID" "ZSS_LOG_ZSS_STARTUP_MSG_TEXT

#ifndef ZSS_LOG_PORT_OCCUP_MSG_ID
#define ZSS_LOG_PORT_OCCUP_MSG_ID            ZSS_LOG_MSG_PRFX"1037W"
#endif
#define ZSS_LOG_PORT_OCCUP_MSG_TEXT          "This is usually because the server port '%d' is occupied. Is ZSS running twice?\n"
#define ZSS_LOG_PORT_OCCUP_MSG               ZSS_LOG_PORT_OCCUP_MSG_ID" "ZSS_LOG_PORT_OCCUP_MSG_TEXT

#ifndef ZSS_LOG_PARS_ZSS_TIMEOUT_MSG_ID
#define ZSS_LOG_PARS_ZSS_TIMEOUT_MSG_ID      ZSS_LOG_MSG_PRFX"1038I"
#endif
#define ZSS_LOG_PARS_ZSS_TIMEOUT_MSG_TEXT    "Server timeouts file '%s' either not found or invalid JSON. ZSS sessions will use the default length of one hour.\n"
#define ZSS_LOG_PARS_ZSS_TIMEOUT_MSG         ZSS_LOG_PARS_ZSS_TIMEOUT_MSG_ID" "ZSS_LOG_PARS_ZSS_TIMEOUT_MSG_TEXT

/* MVD Server (Datasets) */

#ifndef ZSS_LOG_INSTALL_MSG_ID
#define ZSS_LOG_INSTALL_MSG_ID               ZSS_LOG_MSG_PRFX"1039I"
#endif
#define ZSS_LOG_INSTALL_MSG_TEXT             "Installing '%s' service...\n"
#define ZSS_LOG_INSTALL_MSG                  ZSS_LOG_INSTALL_MSG_ID" "ZSS_LOG_INSTALL_MSG_TEXT

/* MVD Server (Discovery) */

/* empty */

/* MVD Server (ServerStatusService) */

#ifndef ZSS_LOG_FAIL_RMF_FETCH_MSG_ID
#define ZSS_LOG_FAIL_RMF_FETCH_MSG_ID        ZSS_LOG_MSG_PRFX"1050W"
#endif
#define ZSS_LOG_FAIL_RMF_FETCH_MSG_TEXT      "Failed to fetch RMF data: RC='%d'\n"
#define ZSS_LOG_FAIL_RMF_FETCH_MSG           ZSS_LOG_FAIL_RMF_FETCH_MSG_ID" "ZSS_LOG_FAIL_RMF_FETCH_MSG_TEXT

/* MVD Server (TLS) */
#ifndef ZSS_LOG_TLS_INIT_MSG_ID
#define ZSS_LOG_TLS_INIT_MSG_ID              ZSS_LOG_MSG_PRFX"1060W"
#endif
#define ZSS_LOG_TLS_INIT_MSG_TEXT            "Failed to init TLS environment, rc=%d(%s)\n"
#define ZSS_LOG_TLS_INIT_MSG                 ZSS_LOG_TLS_INIT_MSG_ID" "ZSS_LOG_TLS_INIT_MSG_TEXT

#ifndef ZSS_LOG_TLS_SETTINGS_MSG_ID
#define ZSS_LOG_TLS_SETTINGS_MSG_ID          ZSS_LOG_MSG_PRFX"1061I"
#endif
#define ZSS_LOG_TLS_SETTINGS_MSG_TEXT        "TLS settings: keyring '%s', label '%s', password '%s', stash '%s'\n"
#define ZSS_LOG_TLS_SETTINGS_MSG             ZSS_LOG_TLS_SETTINGS_MSG_ID" "ZSS_LOG_TLS_SETTINGS_MSG_TEXT

#ifndef ZSS_LOG_ENV_SETTINGS_MSG_ID
#define ZSS_LOG_ENV_SETTINGS_MSG_ID          ZSS_LOG_MSG_PRFX"1062E"
#endif
#define ZSS_LOG_ENV_SETTINGS_MSG_TEXT        "Failed to process environment variables\n"
#define ZSS_LOG_ENV_SETTINGS_MSG             ZSS_LOG_ENV_SETTINGS_MSG_ID" "ZSS_LOG_ENV_SETTINGS_MSG_TEXT

#ifndef ZSS_LOG_CACHE_SETTINGS_MSG_ID
#define ZSS_LOG_CACHE_SETTINGS_MSG_ID        ZSS_LOG_MSG_PRFX"1063I"
#endif
#define ZSS_LOG_CACHE_SETTINGS_MSG_TEXT      "Caching Service settings: gateway host '%s', port %d\n",
#define ZSS_LOG_CACHE_SETTINGS_MSG           ZSS_LOG_CACHE_SETTINGS_MSG_ID" "ZSS_LOG_CACHE_SETTINGS_MSG_TEXT

#ifndef ZSS_LOG_CACHE_NOT_CFG_MSG_ID
#define ZSS_LOG_CACHE_NOT_CFG_MSG_ID         ZSS_LOG_MSG_PRFX"1064I"
#endif
#define ZSS_LOG_CACHE_NOT_CFG_MSG_TEXT       "Caching Service not configured\n"
#define ZSS_LOG_CACHE_NOT_CFG_MSG            ZSS_LOG_CACHE_NOT_CFG_MSG_ID" "ZSS_LOG_CACHE_NOT_CFG_MSG_TEXT

#ifndef ZSS_LOG_HTTPS_INVALID_MSG_ID
#define ZSS_LOG_HTTPS_INVALID_MSG_ID          ZSS_LOG_MSG_PRFX"1065E"
#endif
#define ZSS_LOG_HTTPS_INVALID_MSG_TEXT        "Failed to configure https server, check agent https settings\n"
#define ZSS_LOG_HTTPS_INVALID_MSG             ZSS_LOG_HTTPS_INVALID_MSG_ID" "ZSS_LOG_HTTPS_INVALID_MSG_TEXT

/* registerProduct */

#ifndef ZSS_LOG_PROD_REG_ENABLED_MSG_ID
#define ZSS_LOG_PROD_REG_ENABLED_MSG_ID       ZSS_LOG_MSG_PRFX"1100I"
#endif
#define ZSS_LOG_PROD_REG_ENABLED_MSG_TEXT     "Product Registration is enabled.\n"
#define ZSS_LOG_PROD_REG_ENABLED_MSG          ZSS_LOG_PROD_REG_ENABLED_MSG_ID" "ZSS_LOG_PROD_REG_ENABLED_MSG_TEXT

#ifndef ZSS_LOG_PROD_REG_DISABLED_MSG_ID
#define ZSS_LOG_PROD_REG_DISABLED_MSG_ID      ZSS_LOG_MSG_PRFX"1101I"
#endif
#define ZSS_LOG_PROD_REG_DISABLED_MSG_TEXT    "Product Registration is disabled.\n"
#define ZSS_LOG_PROD_REG_DISABLED_MSG         ZSS_LOG_PROD_REG_DISABLED_MSG_ID" "ZSS_LOG_PROD_REG_DISABLED_MSG_TEXT

#ifndef ZSS_LOG_PROD_REG_RC_0_MSG_ID
#define ZSS_LOG_PROD_REG_RC_0_MSG_ID          ZSS_LOG_MSG_PRFX"1102I"
#endif
#define ZSS_LOG_PROD_REG_RC_0_MSG_TEXT        "Product Registration RC = 0.\n"
#define ZSS_LOG_PROD_REG_RC_0_MSG             ZSS_LOG_PROD_REG_RC_0_MSG_ID" "ZSS_LOG_PROD_REG_RC_0_MSG_TEXT

#ifndef ZSS_LOG_PROD_REG_RC_MSG_ID
#define ZSS_LOG_PROD_REG_RC_MSG_ID            ZSS_LOG_MSG_PRFX"1103W"
#endif
#define ZSS_LOG_PROD_REG_RC_MSG_TEXT          "Product Registration RC = %d\n"
#define ZSS_LOG_PROD_REG_RC_MSG               ZSS_LOG_PROD_REG_RC_MSG_ID" "ZSS_LOG_PROD_REG_RC_MSG_TEXT



/* Unixfile */

#ifndef ZSS_LOG_UNABLE_MSG_ID
#define ZSS_LOG_UNABLE_MSG_ID                ZSS_LOG_MSG_PRFX"1200W"
#endif
#define ZSS_LOG_UNABLE_MSG_TEXT              "Could not %s file: Ret='%d', res='%d'\n"
#define ZSS_LOG_UNABLE_MSG                   ZSS_LOG_UNABLE_MSG_ID" "ZSS_LOG_UNABLE_MSG_TEXT

#ifndef ZSS_LOG_UNABLE_METADATA_MSG_ID
#define ZSS_LOG_UNABLE_METADATA_MSG_ID       ZSS_LOG_MSG_PRFX"1201W"
#endif
#define ZSS_LOG_UNABLE_METADATA_MSG_TEXT     "Could not get metadata for file: Ret='%d', res='%d'\n"
#define ZSS_LOG_UNABLE_METADATA_MSG          ZSS_LOG_UNABLE_METADATA_MSG_ID" "ZSS_LOG_UNABLE_METADATA_MSG_TEXT

#ifndef ZSS_LOG_TTYPE_NOT_SET_MSG_ID
#define ZSS_LOG_TTYPE_NOT_SET_MSG_ID         ZSS_LOG_MSG_PRFX"1202W"
#endif
#define ZSS_LOG_TTYPE_NOT_SET_MSG_TEXT       "Transfer type has not been set."
#define ZSS_LOG_TTYPE_NOT_SET_MSG            ZSS_LOG_TTYPE_NOT_SET_MSG_ID" "ZSS_LOG_TTYPE_NOT_SET_MSG_TEXT

/* Security */

#ifndef ZSS_LOG_NON_STRD_MSG_ID
#define ZSS_LOG_NON_STRD_MSG_ID              ZSS_LOG_MSG_PRFX"1400W"
#endif
#define ZSS_LOG_NON_STRD_MSG_TEXT            "Non standard class provided for '%s' '%s', ending request...\n"
#define ZSS_LOG_NON_STRD_MSG                 ZSS_LOG_NON_STRD_MSG_ID" "ZSS_LOG_NON_STRD_MSG_TEXT

#ifndef ZSS_LOG_PROF_REQ_GET_MSG_ID
#define ZSS_LOG_PROF_REQ_GET_MSG_ID          ZSS_LOG_MSG_PRFX"1401W"
#endif
#define ZSS_LOG_PROF_REQ_GET_MSG_TEXT        "Profile not provided for profiles GET, ending request...\n"
#define ZSS_LOG_PROF_REQ_GET_MSG             ZSS_LOG_PROF_REQ_GET_MSG_ID" "ZSS_LOG_PROF_REQ_GET_MSG_TEXT

#ifndef ZSS_LOG_PROF_REQ_MSG_ID
#define ZSS_LOG_PROF_REQ_MSG_ID              ZSS_LOG_MSG_PRFX"1402W"
#endif
#define ZSS_LOG_PROF_REQ_MSG_TEXT            "Profile name required for '%s' '%s'\n"
#define ZSS_LOG_PROF_REQ_MSG                 ZSS_LOG_PROF_REQ_MSG_ID" "ZSS_LOG_PROF_REQ_MSG_TEXT

#ifndef ZSS_LOG_USER_REQ_USER_PP_MSG_ID
#define ZSS_LOG_USER_REQ_USER_PP_MSG_ID      ZSS_LOG_MSG_PRFX"1403W"
#endif
#define ZSS_LOG_USER_REQ_USER_PP_MSG_TEXT    "User ID required for user POST/PUT\n"
#define ZSS_LOG_USER_REQ_USER_PP_MSG         ZSS_LOG_USER_REQ_USER_PP_MSG_ID" "ZSS_LOG_USER_REQ_USER_PP_MSG_TEXT

#ifndef ZSS_LOG_BODY_NPROV_USER_PP_MSG_ID
#define ZSS_LOG_BODY_NPROV_USER_PP_MSG_ID    ZSS_LOG_MSG_PRFX"1404W"
#endif
#define ZSS_LOG_BODY_NPROV_USER_PP_MSG_TEXT  "Body not provided for user POST/PUT, ending request...\n"
#define ZSS_LOG_BODY_NPROV_USER_PP_MSG       ZSS_LOG_BODY_NPROV_USER_PP_MSG_ID" "ZSS_LOG_BODY_NPROV_USER_PP_MSG_TEXT

#ifndef ZSS_LOG_BAD_TYPE_USER_PP_MSG_ID
#define ZSS_LOG_BAD_TYPE_USER_PP_MSG_ID      ZSS_LOG_MSG_PRFX"1405W"
#endif
#define ZSS_LOG_BAD_TYPE_USER_PP_MSG_TEXT    "Bad access type provided for user POST/PUT, ending request...\n"
#define ZSS_LOG_BAD_TYPE_USER_PP_MSG         ZSS_LOG_BAD_TYPE_USER_PP_MSG_ID" "ZSS_LOG_BAD_TYPE_USER_PP_MSG_TEXT

#ifndef ZSS_LOG_UNK_TYPE_USER_PP_MSG_ID
#define ZSS_LOG_UNK_TYPE_USER_PP_MSG_ID      ZSS_LOG_MSG_PRFX"1406W"
#endif
#define ZSS_LOG_UNK_TYPE_USER_PP_MSG_TEXT    "Unknown access type '%d' provided for user POST/PUT, ending request...\n"
#define ZSS_LOG_UNK_TYPE_USER_PP_MSG         ZSS_LOG_UNK_TYPE_USER_PP_MSG_ID" "ZSS_LOG_UNK_TYPE_USER_PP_MSG_TEXT

#ifndef ZSS_LOG_ACCESS_LIST_BULK_MSG_ID
#define ZSS_LOG_ACCESS_LIST_BULK_MSG_ID      ZSS_LOG_MSG_PRFX"1407W"
#endif
#define ZSS_LOG_ACCESS_LIST_BULK_MSG_TEXT    "Access list can only be retrieved in bulk, ending request...\n"
#define ZSS_LOG_ACCESS_LIST_BULK_MSG         ZSS_LOG_ACCESS_LIST_BULK_MSG_ID" "ZSS_LOG_ACCESS_LIST_BULK_MSG_TEXT

#ifndef ZSS_LOG_ACCESS_LIST_BUFR_MSG_ID
#define ZSS_LOG_ACCESS_LIST_BUFR_MSG_ID      ZSS_LOG_MSG_PRFX"1408W"
#endif
#define ZSS_LOG_ACCESS_LIST_BUFR_MSG_TEXT    "Access list buffer with size '%u' not allocated, ending request...\n"
#define ZSS_LOG_ACCESS_LIST_BUFR_MSG         ZSS_LOG_ACCESS_LIST_BUFR_MSG_ID" "ZSS_LOG_ACCESS_LIST_BUFR_MSG_TEXT

#ifndef ZSS_LOG_ACCESS_LIST_OORG_MSG_ID
#define ZSS_LOG_ACCESS_LIST_OORG_MSG_ID      ZSS_LOG_MSG_PRFX"1409W"
#endif
#define ZSS_LOG_ACCESS_LIST_OORG_MSG_TEXT    "Access list size out of range '%u', ending request...\n"
#define ZSS_LOG_ACCESS_LIST_OORG_MSG         ZSS_LOG_ACCESS_LIST_OORG_MSG_ID" "ZSS_LOG_ACCESS_LIST_OORG_MSG_TEXT

#ifndef ZSS_LOG_ACCESS_REQ_ACCESS_DEL_MSG_ID
#define ZSS_LOG_ACCESS_REQ_ACCESS_DEL_MSG_ID ZSS_LOG_MSG_PRFX"1410W"
#endif
#define ZSS_LOG_ACCESS_REQ_ACCESS_DEL_MSG_TEXT "Access list entry name required for access list DELETE\n"
#define ZSS_LOG_ACCESS_REQ_ACCESS_DEL_MSG    ZSS_LOG_ACCESS_REQ_ACCESS_DEL_MSG_ID" "ZSS_LOG_ACCESS_REQ_ACCESS_DEL_MSG_TEXT

#ifndef ZSS_LOG_CLASS_MGMT_QRY_DEL_MSG_ID
#define ZSS_LOG_CLASS_MGMT_QRY_DEL_MSG_ID    ZSS_LOG_MSG_PRFX"1411W"
#endif
#define ZSS_LOG_CLASS_MGMT_QRY_DEL_MSG_TEXT  "Class-mgmt query string is invalid, ending request...\n"
#define ZSS_LOG_CLASS_MGMT_QRY_DEL_MSG       ZSS_LOG_CLASS_MGMT_QRY_DEL_MSG_ID" "ZSS_LOG_CLASS_MGMT_QRY_DEL_MSG_TEXT

#ifndef ZSS_LOG_GROUP_REQ_MSG_ID
#define ZSS_LOG_GROUP_REQ_MSG_ID             ZSS_LOG_MSG_PRFX"1412W"
#endif
#define ZSS_LOG_GROUP_REQ_MSG_TEXT           "Group name required for '%s' '%s'\n"
#define ZSS_LOG_GROUP_REQ_MSG                ZSS_LOG_GROUP_REQ_MSG_ID" "ZSS_LOG_GROUP_REQ_MSG_TEXT

#ifndef ZSS_LOG_BODY_REQ_GRP_POST_MSG_ID
#define ZSS_LOG_BODY_REQ_GRP_POST_MSG_ID     ZSS_LOG_MSG_PRFX"1413W"
#endif
#define ZSS_LOG_BODY_REQ_GRP_POST_MSG_TEXT   "Body not provided for group POST, ending request...\n"
#define ZSS_LOG_BODY_REQ_GRP_POST_MSG        ZSS_LOG_BODY_REQ_GRP_POST_MSG_ID" "ZSS_LOG_BODY_REQ_GRP_POST_MSG_TEXT

#ifndef ZSS_LOG_SPRR_REQ_GRP_POST_MSG_ID
#define ZSS_LOG_SPRR_REQ_GRP_POST_MSG_ID     ZSS_LOG_MSG_PRFX"1414W"
#endif
#define ZSS_LOG_SPRR_REQ_GRP_POST_MSG_TEXT   "Superior not provided for group POST, ending request...\n"
#define ZSS_LOG_SPRR_REQ_GRP_POST_MSG        ZSS_LOG_SPRR_REQ_GRP_POST_MSG_ID" "ZSS_LOG_SPRR_REQ_GRP_POST_MSG_TEXT

#ifndef ZSS_LOG_BSPR_PROV_GRP_POST_MSG_ID
#define ZSS_LOG_BSPR_PROV_GRP_POST_MSG_ID    ZSS_LOG_MSG_PRFX"1415W"
#endif
#define ZSS_LOG_BSPR_PROV_GRP_POST_MSG_TEXT  "Bad superior group provided for group POST, ending request...\n"
#define ZSS_LOG_BSPR_PROV_GRP_POST_MSG       ZSS_LOG_BSPR_PROV_GRP_POST_MSG_ID" "ZSS_LOG_BSPR_PROV_GRP_POST_MSG_TEXT

#ifndef ZSS_LOG_ACCESS_REQ_USER_PP_MSG_ID
#define ZSS_LOG_ACCESS_REQ_USER_PP_MSG_ID    ZSS_LOG_MSG_PRFX"1416W"
#endif
#define ZSS_LOG_ACCESS_REQ_USER_PP_MSG_TEXT  "Access type not provided for user POST/PUT, ending request...\n"
#define ZSS_LOG_ACCESS_REQ_USER_PP_MSG       ZSS_LOG_ACCESS_REQ_USER_PP_MSG_ID" "ZSS_LOG_ACCESS_REQ_USER_PP_MSG_TEXT

#ifndef ZSS_LOG_UNK_TYPE_UCCJ_MSG_ID
#define ZSS_LOG_UNK_TYPE_UCCJ_MSG_ID         ZSS_LOG_MSG_PRFX"1417W"
#endif
#define ZSS_LOG_UNK_TYPE_UCCJ_MSG_TEXT       "Unknown access type, use [USE, CREATE, CONNECT, JOIN]"
#define ZSS_LOG_UNK_TYPE_UCCJ_MSG            ZSS_LOG_UNK_TYPE_UCCJ_MSG_ID" "ZSS_LOG_UNK_TYPE_UCCJ_MSG_TEXT

#ifndef ZSS_LOG_LIST_REALLOC_MSG_ID
#define ZSS_LOG_LIST_REALLOC_MSG_ID          ZSS_LOG_MSG_PRFX"1418W"
#endif
#define ZSS_LOG_LIST_REALLOC_MSG_TEXT        "Access list will be re-allocated with capacity '%u'\n"
#define ZSS_LOG_LIST_REALLOC_MSG             ZSS_LOG_LIST_REALLOC_MSG_ID" "ZSS_LOG_LIST_REALLOC_MSG_TEXT

#ifndef ZSS_LOG_GROUP_MGMT_QRY_DEL_MSG_ID
#define ZSS_LOG_GROUP_MGMT_QRY_DEL_MSG_ID    ZSS_LOG_MSG_PRFX"1419W"
#endif
#define ZSS_LOG_GROUP_MGMT_QRY_DEL_MSG_TEXT  "Group-mgmt query string is invalid, ending request...\n"
#define ZSS_LOG_GROUP_MGMT_QRY_DEL_MSG       ZSS_LOG_GROUP_MGMT_QRY_DEL_MSG_ID" "ZSS_LOG_GROUP_MGMT_QRY_DEL_MSG_TEXT

/* PassTicket */
#ifndef ZSS_LOG_PASSTICKET_GEN_FAILED_MSG_ID
#define ZSS_LOG_PASSTICKET_GEN_FAILED_MSG_ID    ZSS_LOG_MSG_PRFX"1500E"
#endif
#define ZSS_LOG_PASSTICKET_GEN_FAILED_MSG_TEXT  "Failed to generate PassTicket - userId='%s', applId='%s', %s\n"
#define ZSS_LOG_PASSTICKET_GEN_FAILED_MSG       ZSS_LOG_PASSTICKET_GEN_FAILED_MSG_ID" "ZSS_LOG_PASSTICKET_GEN_FAILED_MSG_TEXT

/* JWK */

#ifndef ZSS_LOG_JWK_URL_MSG_ID
#define ZSS_LOG_JWK_URL_MSG_ID          ZSS_LOG_MSG_PRFX"1600I"
#endif
#define ZSS_LOG_JWK_URL_MSG_TEXT        "JWT will be configured using JWK URL https://%s:%d%s\n"
#define ZSS_LOG_JWK_URL_MSG             ZSS_LOG_JWK_URL_MSG_ID" "ZSS_LOG_JWK_URL_MSG_TEXT

#ifndef ZSS_LOG_JWK_READY_MSG_ID
#define ZSS_LOG_JWK_READY_MSG_ID          ZSS_LOG_MSG_PRFX"1601I"
#endif
#define ZSS_LOG_JWK_READY_MSG_TEXT        "Server is ready to accept JWT %s fallback to legacy tokens\n"
#define ZSS_LOG_JWK_READY_MSG             ZSS_LOG_JWK_READY_MSG_ID" "ZSS_LOG_JWK_READY_MSG_TEXT

#ifndef ZSS_LOG_JWK_UNRECOGNIZED_MSG_ID
#define ZSS_LOG_JWK_UNRECOGNIZED_MSG_ID     ZSS_LOG_MSG_PRFX"1602W"
#endif
#define ZSS_LOG_JWK_UNRECOGNIZED_MSG_TEXT   "JWK is in unrecognized format\n"
#define ZSS_LOG_JWK_UNRECOGNIZED_MSG        ZSS_LOG_JWK_UNRECOGNIZED_MSG_ID" "ZSS_LOG_JWK_UNRECOGNIZED_MSG_TEXT

#ifndef ZSS_LOG_JWK_PUBLIC_KEY_ERROR_MSG_ID
#define ZSS_LOG_JWK_PUBLIC_KEY_ERROR_MSG_ID     ZSS_LOG_MSG_PRFX"1603W"
#endif
#define ZSS_LOG_JWK_PUBLIC_KEY_ERROR_MSG_TEXT   "Failed to construct public key using JWK\n"
#define ZSS_LOG_JWK_PUBLIC_KEY_ERROR_MSG        ZSS_LOG_JWK_PUBLIC_KEY_ERROR_MSG_ID" "ZSS_LOG_JWK_PUBLIC_KEY_ERROR_MSG_TEXT

#ifndef ZSS_LOG_JWK_HTTP_CTX_ERROR_MSG_ID
#define ZSS_LOG_JWK_HTTP_CTX_ERROR_MSG_ID     ZSS_LOG_MSG_PRFX"1604W"
#endif
#define ZSS_LOG_JWK_HTTP_CTX_ERROR_MSG_TEXT   "JWK: failed to init HTTP context, ensure that APIML and TLS settings are correct\n"
#define ZSS_LOG_JWK_HTTP_CTX_ERROR_MSG        ZSS_LOG_JWK_HTTP_CTX_ERROR_MSG_ID" "ZSS_LOG_JWK_HTTP_CTX_ERROR_MSG_TEXT

#ifndef ZSS_LOG_JWK_FAILED_MSG_ID
#define ZSS_LOG_JWK_FAILED_MSG_ID     ZSS_LOG_MSG_PRFX"1605W"
#endif
#define ZSS_LOG_JWK_FAILED_MSG_TEXT   "Server will not accept JWT\n"
#define ZSS_LOG_JWK_FAILED_MSG        ZSS_LOG_JWK_FAILED_MSG_ID" "ZSS_LOG_JWK_FAILED_MSG_TEXT

#ifndef ZSS_LOG_JWK_RETRY_MSG_ID
#define ZSS_LOG_JWK_RETRY_MSG_ID     ZSS_LOG_MSG_PRFX"1606W"
#endif
#define ZSS_LOG_JWK_RETRY_MSG_TEXT   "Failed to get JWK - %s, retry in %d seconds\n"
#define ZSS_LOG_JWK_RETRY_MSG        ZSS_LOG_JWK_RETRY_MSG_ID" "ZSS_LOG_JWK_RETRY_MSG_TEXT

#endif /* MVD_H_ZSSLOGGING_H_ */


/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

