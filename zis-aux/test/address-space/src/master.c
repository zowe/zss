

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/

#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "zowetypes.h"
#include "alloc.h"
#include "utils.h"

#include "as.h"

static bool areParmsValid(int argc, char **argv) {

  bool isValid = false;

  if (argc > 1) {
    if (!strcmp(argv[1], "start") && argc == 4) {
      isValid = true;
    } else if (!strcmp(argv[1], "stop") && argc == 3) {
      isValid = true;
    }
  }

  return isValid;
}

static int handleStart(char *startString, char *parmString) {

  printf("Info: start request for \'%s\' with parm \'%s\'\n",
         startString, parmString);

  ASParmString startParmString = {0};
  size_t startStringLength = strlen(startString);
  if (startStringLength > sizeof(startParmString.text)) {
    printf("Error: start string too long\n");
    return -1;
  }

  memcpy(startParmString.text, startString, startStringLength);
  startParmString.length = startStringLength;

  ASParm parm = {0};
  size_t parmStringLength = strlen(parmString);
  if (parmStringLength > sizeof(parm.data)) {
    printf("Error: parm string too long\n");
    return -1;
  }

  memcpy(parm.data, parmString, parmStringLength);
  parm.length = parmStringLength;

  ASOutputData result = {0};

  int rc = 0, rsn = 0;
  rc = addressSpaceCreate(&startParmString, &parm, AS_ATTR_PERM, &result, &rsn);

  if (rc > 0) {
    printf("Error: address space not started, ASCRE RC = %d, RSN = %d\n",
           rc, rsn);
    return -1;
  } else {
    printf("Info: address space started, STOKEN = \'%16.16llX\'\n",
           result.stoken);
  }

  return 0;
}

static int handleStop(char *stokenString) {

  printf("Info: stop request for STOKEN = \'%s\'\n", stokenString);

  uint64_t stoken = strtoull(stokenString, NULL, 16);
  if (stoken == 0 || stoken == ULLONG_MAX) {
    printf("Error: bad STOKEN value\n");
    return -1;
  }

  printf("Info: decoded token value = 0x%16.16llX\n", stoken);

  int rc = 0, rsn = 0;
  rc = addressSpaceTerminate(stoken, &rsn);

  if (rc > 0) {
    printf("Error: address space not terminated, ASDES RC = %d, RSN = %d\n",
           rc, rsn);
    return -1;
  } else {
    printf("Info: address space terminated\n");
  }

  return 0;
}

int main(int argc, char **argv) {

  if (!areParmsValid(argc, argv)) {
    printf("Error: incorrect parameters, use the following options:\n");
    printf("  > master start <start-string> <parameter-string>\n");
    printf("  > master stop <stoken>\n");
    exit(EXIT_FAILURE);
  }

  supervisorMode(TRUE);

  int handleRC;
  if (!strcmp(argv[1], "start")) {
    handleRC = handleStart(argv[2], argv[3]);
  } else {
    handleRC = handleStop(argv[2]);
  }

  if (handleRC == 0) {
    exit(EXIT_SUCCESS);
  } else {
    exit(EXIT_FAILURE);
  }

}


/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/
