
/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/

#ifndef ZIS_SERVER_API_H_
#define ZIS_SERVER_API_H_

#ifndef METTLE
#error Metal C header only
#endif

#pragma map(zisGetServerVersion, "ZISGVRSN")
#pragma map(zisIsLPADevModeOn, "ZISLPADV")

/**
 * Get the version of ZIS.
 * @param[out] major The major ZIS version.
 * @param[out] minor The minor ZIS version.
 * @param[out] revision The revision of ZIS (aka the patch version).
 */
void zisGetServerVersion(int *major, int *minor, int *revision);

/**
 * Check if the LPA dev mode has been enabled.
 * @param[in] context The server context.
 * @return @c true if on, otherwise @c false.
 */
_Bool zisIsLPADevModeOn(const struct ZISContext_tag *context);

#endif /* ZIS_SERVER_API_H_ */

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/
