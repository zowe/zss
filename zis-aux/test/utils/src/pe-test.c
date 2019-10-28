

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/

#ifdef METTLE
#include <metal/metal.h>
#include <metal/stdio.h>
#else
#include <stdio.h>
#endif

#include "utils.h"

#include "pause-element.h"

#ifndef __ZOWE_OS_ZOS
#error z/OS targets are supported only
#endif

#define RC_OK       0
#define RC_FAILURE  8

int main() {

  printf("info: starting pause element test\n");

  PEReleaseCode rcode = {9};
  PET pet = {0};

  int rc;

  rc = peAlloc(&pet, NULL, rcode, PE_AUTH_UNAUTHORIZED, false);
  printf("info: alloc rc = %d\n", rc);
  if (rc != 0) {
    printf("error: PE alloc failed\n");
    return RC_FAILURE;
  }

  rcode.value = 87;
  rc = peRelease(&pet, rcode, false);
  printf("info: release rc = %d\n", rc);
  if (rc != 0) {
    printf("error: PE release failed\n");
    return RC_FAILURE;
  }

  {
    PEInfo info = {0};
    rc = peRetrieveInfo(&pet, &info, false);
    printf("info: info rc = %d, state = %d, STOKEN owner = %-.16llX, curr = %-.16llX, "
        "code = %d, auth level = %d\n", rc, info.state, info.ownerStoken.value,
        info.currentStoken.value, rcode.value, info.authLevel);
    if (rc != 0) {
      printf("error: PE info failed\n");
      return RC_FAILURE;
    }
  }

  {
    PEState state = {0};
    PEReleaseCode currRCode = {0};
    rc = peTest(&pet, &state, &currRCode);
    printf("info: test rc = %d, state = %d, code = %d\n", rc, state, rcode.value);
    if (rc != 0) {
      printf("error: PE test failed\n");
      return RC_FAILURE;
    }
  }

  PET newPET = {0};
  rc = pePause(&pet, &newPET, &rcode, false);
  printf("info: pause rc = %d, code = %d\n", rc, rcode.value);
  if (rc != 0) {
    printf("error: PE pause failed\n");
    return RC_FAILURE;
  }

  rc = peDealloc(&newPET, false);
  printf("info: dealloc rc = %d\n", rc);
  if (rc != 0) {
    printf("error: PE dealloc failed\n");
    return RC_FAILURE;
  }

  printf("info: SUCCESS, all tests have passed\n");

  return RC_OK;
}


/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/

