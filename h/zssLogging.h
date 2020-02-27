

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

