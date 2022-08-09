
/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/


#ifndef METTLE
#error Metal C header only
#endif

#include <metal/metal.h>
#include <metal/stdbool.h>

#pragma map(zisIsLPADevModeOn, "ZISLPADV")

/**
 * Check if the LPA dev mode has been enabled.
 * @param context The server context.
 * @return @c true if on, otherwise @c false.
 */
bool zisIsLPADevModeOn(const struct ZISContext_tag *context);


/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/
