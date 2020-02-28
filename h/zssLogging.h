

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

#ifndef MVD_H_ZSSLOGGING_H_
#define MVD_H_ZSSLOGGING_H_

#define LOG_COMP_ID_ZSS 0x008F000300010000
#define LOG_COMP_ID_CTDS       0x008F000300020000
#define LOG_COMP_ID_SECURITY   0x008F000300030000
#define LOG_COMP_ID_UNIXFILE   0x008F000300040000

#define LOG_COMP_ID_TEXT_ZSS        "zss"
#define LOG_COMP_ID_TEXT_CTDS       "ctds"
#define LOG_COMP_ID_TEXT_SECURITY   "security"
#define LOG_COMP_ID_TEXT_UNIXFILE   "unixfile"

#define LOG_PREFIX_ZSS "_zss"
#define LOG_PREFIX_ZCC "_zcc"
#define LOG_PREFIX_ZIS "_zis"

#define LOG_LEVEL_SEVERE "SEVERE"
#define LOG_LEVEL_WARN "WARN"
#define LOG_LEVEL_INFO "INFO"
#define LOG_LEVEL_DEBUG "DEBUG"
#define LOG_LEVEL_TRACE "TRACE"

#define LOG_LEVEL_ID_SEVERE      0
#define LOG_LEVEL_ID_WARN        1  
#define LOG_LEVEL_ID_INFO        2 
#define LOG_LEVEL_ID_DEBUG       3
#define LOG_LEVEL_ID_DEBUG2      4
#define LOG_LEVEL_ID_TRACE       5

#define PREFIXED_LINE_MAX_COUNT         1000
#define PREFIXED_LINE_MAX_MSG_LENGTH    4096
#define LOG_MSG_PREFIX_SIZE             1000
#define LOCATION_PREFIX_PADDING         7
#define LOCATION_SUFFIX_PADDING         5
#define USER_SIZE                       7     //Will this always be 7?
#define THREAD_SIZE                     10
#define LOG_LEVEL_MAX_SIZE              16
#define PREFIX_SUFFIX_SIZE              128

bool isLogLevelValid(int level);
void zssFormatter(LoggingContext *context, LoggingComponent *component, char* path, int line, int level, uint64 compID, void *data, char *formatString, va_list argList);

#endif /* MVD_H_ZSSLOGGING_H_ */


/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

