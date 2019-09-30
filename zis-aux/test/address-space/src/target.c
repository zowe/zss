

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

#include "as.h"
#include "metalio.h"
#include "zos.h"

#ifndef METTLE
#error Metal C is supported only
#endif

#define SLEEP_INTERVAL_IN_SEC 2
#define SLEEP_INTERVAL_COUNT  100

static void sleepInSeconds(uint16_t seconds) {

  uint32_t waitValue = seconds * 100;

  __asm(
      "         STIMER WAIT,BINTVL=%0                                          \n"
      :
      : "m"(waitValue)
      : "r0", "r1", "r14", "r15"
  );


}

int main() {

  printf("Info: hello from address space target test program\n");

  supervisorMode(TRUE);

  int extractRC = 0, extractRSN = 0;
  ASParm *parm = NULL;

  extractRC = addressSpaceExtractParm(&parm, &extractRSN);
  if (extractRC == 0) {
    printf("Info: address space extract RC = %d, RSN = %d, parm = 0x%p:\n",
           extractRC, extractRSN, parm);
    if (parm) {
      dumpbuffer(parm->data, parm->length);
    }
  } else {
    printf("Error: address space extract RC = %d, RSN = %d, parm = 0x%p:\n",
           extractRC, extractRSN, parm);
  }

  for (size_t i = 0; i < SLEEP_INTERVAL_COUNT; i++) {
    printf("Info: sleeping %d second for %d time\n", SLEEP_INTERVAL_IN_SEC, i);
    sleepInSeconds(SLEEP_INTERVAL_IN_SEC);
  }

  printf("Info: done, terminating...\n");

  return 0;
}


/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/
