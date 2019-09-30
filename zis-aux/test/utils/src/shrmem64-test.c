

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/

#ifdef METTLE
#include <metal/metal.h>
#include <metal/stddef.h>
#include <metal/stdint.h>
#include <metal/stdio.h>
#include <metal/string.h">
#include "metalio.h"
#else
#include "stddef.h"
#include "stdint.h"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#endif

#include "shrmem64.h"
#include "zos.h"

#define RC_OK       0
#define RC_ERROR    8

#define MYASID() (getASCB()->ascbasid)

#define TEST_MSG "hello from shared storage!"

#define SHR64_PRINTF($text, ...) \
  printf("%04X " $text, MYASID(), ##__VA_ARGS__)

static int runMasterASTest(int argc, char **argv) {

  int status = RC_OK;
  supervisorMode(TRUE);

  int rc = 0;
  int rsn = 0;
  void *mem = NULL;
  void *mem2 = NULL;
  MemObjToken token = shrmem64GetAddressSpaceToken();

  SHR64_PRINTF("info: master address space started, token = %016llX\n", token);

  rc = shrmem64Alloc(token, 8000000000, &mem, &rsn);

  if (rc == RC_SHRMEM64_OK) {
    SHR64_PRINTF("info: storage allocated at 0x%p\n", mem);
  } else {
    SHR64_PRINTF("error: storage not allocated, rc = %d, rsn = 0x%08X\n",
                 rc, rsn);
    return RC_ERROR;
  }

  rc = shrmem64Alloc(token, 100000, &mem2, &rsn);

  if (rc == RC_SHRMEM64_OK) {
    SHR64_PRINTF("info: storage (2) allocated at 0x%p\n", mem2);
  } else {
    SHR64_PRINTF("error: storage (2) not allocated, rc = %d, rsn = 0x%08X\n",
                 rc, rsn);
    return RC_ERROR;
  }

  rc = shrmem64GetAccess(token, mem, &rsn);

  if (rc == RC_SHRMEM64_OK) {
    SHR64_PRINTF("info: got access for 0x%p\n", mem);
  } else {
    SHR64_PRINTF("error: access not obtained, rc = %d, rsn = 0x%08X\n",
                  rc, rsn);
    status = (status != RC_OK) ? status : RC_ERROR;
  }

  strcat(mem, TEST_MSG);

  char targetCommand[512];
  sprintf(targetCommand, "./shrmem64-target-test 0x%p", mem);
  int targetRC = system(targetCommand);
  if (targetRC == RC_OK) {
    SHR64_PRINTF("info: target address space successfully executed\n");
  } else {
    SHR64_PRINTF("error: target address space failed, rc = %d\n",
                 targetRC);
    status = (status != RC_OK) ? status : RC_ERROR;
  }

  rc = shrmem64RemoveAccess(token, mem, &rsn);

  if (rc == RC_SHRMEM64_OK) {
    SHR64_PRINTF("info: access removed for 0x%p\n", mem);
  } else {
    SHR64_PRINTF("error: access not removed, rc = %d, rsn = 0x%08X\n",
                  rc, rsn);
    status = (status != RC_OK) ? status : RC_ERROR;
  }

  rc = shrmem64Release(token, mem, &rsn);

  if (rc == RC_SHRMEM64_OK) {
    SHR64_PRINTF("info: storage free'd at 0x%p\n", mem);
  } else {
    SHR64_PRINTF("error: storage not free'd, rc = %d, rsn = 0x%08X\n",
                 rc, rsn);
    status = (status != RC_OK) ? status : RC_ERROR;
  }

  rc = shrmem64ReleaseAll(token, &rsn);

  if (rc == RC_SHRMEM64_OK) {
    SHR64_PRINTF("info: the rest of the storage free'd\n", mem);
  } else {
    SHR64_PRINTF("error: the rest of the storage token not free'd, "
                 "rc = %d, rsn = 0x%08X\n", rc, rsn);
    status = (status != RC_OK) ? status : RC_ERROR;
  }

  if (status == RC_OK) {
    SHR64_PRINTF("info: SUCCESS, the tests have passed\n");
  } else {
    SHR64_PRINTF("info: FAILURE, not all tests have passed\n");
  }

  return status;
}

static int runTargetASTest(int argc, char **argv) {

  int status = RC_OK;
  supervisorMode(TRUE);


  if (argc != 2) {
    SHR64_PRINTF("error: bad arguments in the target address space test\n");
    return RC_ERROR;
  }

  char *mobjString = argv[1];
  int rc = 0;
  int rsn = 0;
  void *mem = (void *)strtoll(mobjString, NULL, 16);
  MemObjToken token = shrmem64GetAddressSpaceToken();

  SHR64_PRINTF("info: subtask has been started, mem obj string = \'%s\', "
                "token = %016llX\n", argv[1], token);

  rc = shrmem64GetAccess(token, mem, &rsn);

  if (rc == RC_SHRMEM64_OK) {
    SHR64_PRINTF("info: got access for 0x%p\n", mem);
  } else {
    SHR64_PRINTF("error: access not obtained, rc = %d, rsn = 0x%08X\n",
                 rc, rsn);
    status = (status != RC_OK) ? status : RC_ERROR;
  }

  SHR64_PRINTF("info: message = \'%s\'\n", mem);

  if (strcmp(mem, TEST_MSG)) {
    SHR64_PRINTF("error: unexpected message in the shared storage\n");
    status = (status != RC_OK) ? status : RC_ERROR;
  }

  rc = shrmem64RemoveAccess(token, mem, &rsn);

  if (rc == RC_SHRMEM64_OK) {
    SHR64_PRINTF("info: access removed for 0x%p\n", mem);
  } else {
    SHR64_PRINTF("error: access not removed, rc = %d, rsn = 0x%08X\n",
                 rc, rsn);
    status = (status != RC_OK) ? status : RC_ERROR;
  }

  return status;
}

int main(int argc, char **argv) {

#ifndef SHRMEM64_TEST_TARGET
  return runMasterASTest(argc, argv);
#else
  return runTargetASTest(argc, argv);
#endif

}


/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/
