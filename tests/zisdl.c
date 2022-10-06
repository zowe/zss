/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/


#ifndef METTLE
#error Metal C only
#endif

#include "metal/metal.h"
#include "metal/stddef.h"
#include "metal/string.h"
#include "zvt.h"
#include "crossmemory.h"
#include "metalio.h"
#include "zos.h"

#define RC_OK 0
#define RC_CMS_NOT_FOUND 4
#define RC_NO_CMS_NAME 8
#define RC_BAD_NAME 9
#define RC_NO_LOOKUP 10
#define RC_UNSUPPORTED_CMS 11


int main() {

  struct {
    unsigned short len;
    char string[];
  } *cmsNameParm = NULL;
  __asm(" LLGT 1,0(0,1) " : "=NR:r1"(cmsNameParm));

  if (cmsNameParm == NULL) {
    return RC_NO_CMS_NAME;
  }

  CrossMemoryServerName cmsName;
  memset(&cmsName.nameSpacePadded, ' ', sizeof(cmsName.nameSpacePadded));
  if (cmsNameParm->len > sizeof(cmsName.nameSpacePadded)) {
    return RC_BAD_NAME;
  }
  memcpy(&cmsName.nameSpacePadded, cmsNameParm->string, cmsNameParm->len);

  CMSLookupFunction *lookup = CMS_GET_LOOKUP_FUNCTION();
  if (lookup == NULL) {
    return RC_NO_LOOKUP;
  }

  int rsn = -1;
  const CrossMemoryServerGlobalArea *ga = lookup(&cmsName, &rsn);
  if (ga == NULL) {
    return (rsn << 4) + RC_CMS_NOT_FOUND;
  }

  if (!CMS_DYNLINK_SUPPORTED(ga)) {
   return RC_UNSUPPORTED_CMS;
  }

  CMS_SETUP_DYNLINK_ENV(ga);
  {
    wtoPrintf("info: if you see this, the test has been successful, "
              "here's ECVT %p\n", getECVT());
  }

  return RC_OK;
}

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/


