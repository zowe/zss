/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/

#ifndef ZIS_ZISDYNAMIC_H_
#define ZIS_ZISDYNAMIC_H_

#pragma map(zisdynGetStubVersion, "ZISDYNSV")
#pragma map(zisdynGetPluginVersion, "ZISDYNPV")

/**
 * Get the version of the stub vector.
 * @return @c The version of the stub vector.
 */
int zisdynGetStubVersion(void);


/**
 * Get the version of the dynamic linkage plugin.
 * @param[out] major The major plugin version.
 * @param[out] minor The minor plugin version.
 * @param[out] revision The revision of the plugin (aka the patch version).
 */
void zisdynGetPluginVersion(int *major, int *minor, int *revision);

#endif /* ZIS_ZISDYNAMIC_H_ */

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/
