

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/

#include <metal/metal.h>
#include <metal/stddef.h>
#include <metal/stdio.h>
#include <metal/string.h>

#ifndef METTLE
#error Non-metal C code is not supported
#endif

#include "zowetypes.h"
#include "crossmemory.h"
#include "radmin.h"
#include "zos.h"

bool secmgmtGetCallerUserID(RadminUserID *caller) {

  ACEE aceeData = {0};
  ACEE *aceeAddress = NULL;
  cmGetCallerTaskACEE(&aceeData, &aceeAddress);
  if (aceeAddress == NULL) {
    return false;
  }

  caller->length = aceeData.aceeuser[0];
  memcpy(caller->value, &aceeData.aceeuser[1], sizeof(caller->value));

  return true;
}

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/
