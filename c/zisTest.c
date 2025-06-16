/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "zis/client.h"

static int printZISStatus(char *zisName) {
  CrossMemoryServerName privilegedServerName = cmsMakeServerName(zisName);
  CrossMemoryServerStatus status = cmsGetStatus(&privilegedServerName);

  const char *shortDescription = NULL;

  if (status.cmsRC == RC_CMS_OK) {
    shortDescription = "Ok";
  } else {
    shortDescription = "Error";
  }

  printf("ZIS %s (rc='%d', description='%s', clientVersion='%d')\n", 
          shortDescription,
          status.cmsRC,
          status.descriptionNullTerm,
          CROSS_MEMORY_SERVER_VERSION);
  return status.cmsRC;
}

static char *getKeywordArg(char *key, int argc, char **argv) {
  for (int aa=1; aa<argc; aa++) {
    if (!strcmp(argv[aa],key) &&
        (aa+1 < argc)) {
      return argv[aa+1];
    }
  }
  return NULL;
}

#define VERIFY_STATUS_OK     0
#define VERIFY_STATUS_ERROR  8

int main(int argc, char **argv) {
  int status = VERIFY_STATUS_OK;

  if (argc == 1 || (argc == 2 && (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0))) {
    printf("zis-test - Tests if the user has the permissions to access a specified, running ZIS\n");
    printf("  Format: zisTest --zis zis_nickname\n");
    printf("  Exit values: 0 if ZIS is running and accessible, 8 otherwise\n");
    return VERIFY_STATUS_OK;
  }

  char *zisName = getKeywordArg("--zis",argc,argv);

  if (!zisName) {
    printf("Error: --zis specifying ZIS server name to check is required\n");
    status = VERIFY_STATUS_ERROR;
  } else if (strlen(zisName) > sizeof(CrossMemoryServerName)) {
    printf("Error: ZIS server name must be maximum 16 characters.\n");
    status = VERIFY_STATUS_ERROR;
  } else {
    int rc = printZISStatus(zisName);
    if (rc != 0) {
      status = VERIFY_STATUS_ERROR;
      if (rc == RC_CMS_PERMISSION_DENIED) {
        printf("Error: program lacks permission to use ZIS\n");
        char *username = getenv("USER");
        if (username) {
          printf("Ensure the Zowe STC id (possibly the current user: %s) has READ access to ZWES.IS in the FACILITY class\n", username);
        } else {
          printf("Ensure the Zowe STC id has READ access to ZWES.IS in the FACILITY class\n");
        } 
      } else if (rc == RC_CMS_ZVT_NULL || rc == RC_CMS_ZERO_PC_NUMBER || rc == RC_CMS_GLOBAL_AREA_NULL || rc == RC_CMS_SERVER_NOT_READY) {
        printf("The ZIS STC does not appear to be running. Start the job (Default: ZWESISTC) before starting the rest of Zowe\n");
      }
    }
  }

  return status;
}
