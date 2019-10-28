

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

#include "zowetypes.h"
#include "alloc.h"
#include "metalio.h"
#include "zos.h"

#include "as.h"

static void post(int * __ptr32 ecb, int code) {
  __asm(
      ASM_PREFIX
      "         POST (%0),(%1),LINKAGE=SYSTEM                                  \n"
      :
      : "r"(ecb), "r"(code)
      : "r0", "r1", "r14", "r15"
  );
}

static void wait(int * __ptr32 ecb) {
  __asm(
      ASM_PREFIX
      "       WAIT  1,ECB=(%0)                                                 \n"
      :
      : "r"(ecb)
      : "r0", "r1", "r14", "r15"
  );
}

static void printSuccessMessage(void *userParm) {
  int *termECB = userParm;
  post(termECB, 0);
  wtoPrintf("ASCRE TERMINATION STATUS = SUCCESS\n");
  printf("ASCRE TERMINATION STATUS = SUCCESS\n");
}

int main() {

  printf("Info: starting termination exit test\n");

  ASParmString startParmString = {
      .text = "IEESYSAS.TST00001,PROG=IEFBR14",
      .length = strlen(startParmString.text)
  };


  ASParm parm = {0};
  ASOutputData result = {0};
  int termECB = 0;
  int rc = 0, rsn = 0;

  int wasProblem = supervisorMode(TRUE);

  rc = addressSpaceCreateWithTerm(&startParmString, &parm, AS_ATTR_PERM, &result,
                                  printSuccessMessage, &termECB, &rsn);

  if (wasProblem) {
    supervisorMode(FALSE);
  }

  if (rc > 0) {
    printf("Error: address space not started, ASCRE RC = %d, RSN = %d\n",
           rc, rsn);
    return 8;
  } else {
    printf("Info: address space started, STOKEN = \'%16.16llX\'\n",
           result.stoken);
  }

  wait(&termECB);

  printf("Info: the test has successfully completed\n");

  return 0;
}


/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/
