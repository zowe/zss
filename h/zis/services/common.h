

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/

#ifndef ZIS_SERVICES_COMMON_H_
#define ZIS_SERVICES_COMMON_H_

#define ZIS_SERVICES_DEFAULT_SAF_CLASS "FACILITY"

typedef struct ZISCoreServiceParm_tag {
#define ZIS_CORE_SERVICE_FLAG_NO_SAF_CHECK 0x00000001
  unsigned flags : 8;
  char reserved[7];
} ZISCoreServiceParm;

#define IS_ZIS_CORE_SERVICE_SAF_ON(data) ({ \
     !(((ZISCoreServiceParm *)&data)->flags & ZIS_CORE_SERVICE_FLAG_NO_SAF_CHECK); \
   })

#endif /* ZIS_SERVICES_COMMON_H_ */


/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/
