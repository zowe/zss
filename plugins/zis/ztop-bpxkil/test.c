

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "zowetypes.h"
#include "alloc.h"
#include "qsam.h"
#include "utils.h"
#include "zos.h"

#include "zis/client.h"
#include "zis/service.h"

#include "services/echoservice.h"

#ifndef __ZOWE_OS_ZOS
#error z/OS targets are supported only
#endif

#define RC_OK     0
#define RC_ERROR  8

int main(int argc, char **argv) {

  return RC_OK;
}


/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/


