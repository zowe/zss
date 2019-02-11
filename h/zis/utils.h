

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/

#ifndef ZIS_UTILS_H_
#define ZIS_UTILS_H_

#define CMS_DEBUG($globalArea, $fmt, ...) do {\
  if ($globalArea->pcLogLevel >= ZOWE_LOG_DEBUG) {\
    cmsPrintf(&$globalArea->serverName, CMS_LOG_DEBUG_MSG_ID" " $fmt, \
        ##__VA_ARGS__);\
  }\
} while (0)


#define CMS_DEBUG2($globalArea, $level, $fmt, ...) do {\
  if ($level >= ZOWE_LOG_DEBUG) {\
    cmsPrintf(&$globalArea->serverName, CMS_LOG_DEBUG_MSG_ID" " $fmt, \
        ##__VA_ARGS__);\
  }\
} while (0)

#endif /* ZIS_UTILS_H_ */


/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/

