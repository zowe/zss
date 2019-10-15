

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
#include "zos.h"

#include "pc.h"

#ifndef __ZOWE_OS_ZOS
#error z/OS targets are supported only
#endif

#define RC_OK       0
#define RC_FAILURE  8

static int (* __ptr32 getDummyRoutine(void))(void) {

  Addr31 routine = NULL;

  __asm(
      ASM_PREFIX
      "         LARL  1,DRBGN                                                  \n"
      "         ST    1,%[routine]                                             \n"
      "         J     DREND                                                    \n"
      "DRBGN    DS    0H                                                       \n"

      /* actual routine */
      "         LA    15,777                                                   \n"
      "         PR    ,                   return                               \n"

      "DREND    DS    0H                                                       \n"
      : [routine]"=m"(routine)
      :
      : "r1"
  );

  return (int (* __ptr32)(void))routine;
}

int main() {

  printf("info: starting PC test\n");

  supervisorMode(TRUE);

  int status = RC_OK;
  char *failedTestMsg = "";
  int rc, rsn;

  rc = pcSetAllAddressSpaceAuthority(&rsn);
  printf("info: address space authority rc = %d, rsn = %d\n", rc, rsn);
  if (rc != RC_PC_OK) {
    failedTestMsg = "address space authority";
    status = RC_FAILURE;
    goto tests_done;
  }


  PCLinkageIndex linkageIndex;
  rc = pcReserveLinkageIndex(false, true, LX_SIZE_24, &linkageIndex, &rsn);
  printf("info: reserve linkage rc = %d, rsn = %d, result = (%08X, %08X)\n",
         rc, rsn, linkageIndex.pcNumber, linkageIndex.sequenceNumber);
  if (rc != RC_PC_OK) {
    failedTestMsg = "reserve linkage";
    status = RC_FAILURE;
    goto tests_done;
  }

  EntryTableDescriptor *etd = pcMakeEntryTableDescriptor();
  printf("info: entry table descriptor allocated @ 0x%p\n", etd);
  if (etd == NULL) {
    failedTestMsg = "allocate entry table descriptor";
    status = RC_FAILURE;
    goto tests_done;
  }

  rc = pcAddToEntryTableDescriptor(etd, getDummyRoutine(), 0, 0,
                                   true, true, true, true, 8);
  printf("info: add entry table descriptor entry rc = %d\n", rc);
  if (rc != RC_PC_OK) {
    failedTestMsg = "add entry table descriptor entry";
    status = RC_FAILURE;
    goto tests_done;
  }

  PCEntryTableToken resultToken;
  rc = pcCreateEntryTable(etd, &resultToken, &rsn);
  printf("info: entry table create rc = %d, token = 0x%08X\n", rc, resultToken);
  if (rc != RC_PC_OK) {
    failedTestMsg = "add created entry table";
    status = RC_FAILURE;
    goto tests_done;
  }

  rc = pcConnectEntryTable(resultToken, linkageIndex, &rsn);
  printf("info: connect entry table rc = %d\n", rc);
  if (rc != RC_PC_OK) {
    failedTestMsg = "connected created entry table";
    status = RC_FAILURE;
    goto tests_done;
  }

  rc = pcCallRoutine(linkageIndex.pcNumber, linkageIndex.sequenceNumber, NULL);
  printf("info: program call rc = %d\n", rc);
  if (rc != 777) {
    failedTestMsg = "program call";
    status = RC_FAILURE;
  }

  rc = pcDisconnectEntryTable(resultToken, &rsn);
  printf("info: disconnect entry table rc = %d\n", rc);
  if (rc != RC_PC_OK) {
    failedTestMsg = "disconnected entry table";
    status = RC_FAILURE;
    goto tests_done;
  }

  rc = pcDestroyEntryTable(resultToken, false, &rsn);
  printf("info: destroy entry table rc = %d\n", rc);
  if (rc != RC_PC_OK) {
    failedTestMsg = "destroy entry table";
    status = RC_FAILURE;
    goto tests_done;
  }

  pcRemoveEntryTableDescriptor(etd);
  etd = NULL;
  printf("info: entry table descriptor removed\n");

  rc = pcFreeLinkageIndex(linkageIndex, false, &rsn);
  printf("info: free linkage index rc = %d\n", rc);
  if (rc != RC_PC_OK) {
    failedTestMsg = "free linkage index";
    status = RC_FAILURE;
    goto tests_done;
  }

  tests_done:

  supervisorMode(FALSE);

  if (status == RC_OK) {
  printf("info: SUCCESS, all tests have passed\n");
  } else {
    printf("error: FAILURE, not all tests have passed, failed test = %s\n",
           failedTestMsg);
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

