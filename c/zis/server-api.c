
/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/

#ifndef METTLE
#error Metal C source only
#endif

#include <metal/metal.h>
#include <metal/stddef.h>
#include <metal/stdbool.h>

#include "zis/server.h"
#include "zis/server-api.h"
#include "zis/version.h"

/* Check this for backward compatibility with the compile-time dev mode. */
#ifndef ZIS_LPA_DEV_MODE
#define ZIS_LPA_DEV_MODE 0
#endif

bool zisIsLPADevModeOn(const ZISContext *context) {
  int devFlags = CMS_SERVER_FLAG_DEV_MODE_LPA | CMS_SERVER_FLAG_DEV_MODE;
  return (context->cmsFlags & devFlags) || ZIS_LPA_DEV_MODE;
}

void zisGetServerVersion(int *major, int *minor, int *revision) {
  *major = ZIS_MAJOR_VERSION;
  *minor = ZIS_MINOR_VERSION;
  *revision = ZIS_REVISION;
}

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/
