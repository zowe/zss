/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which
  accompanies this distribution, and is available at
  https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/

/*
 * Program to test the ZIS core services' SAF protection.
 *
 * Invokes a specified core service with a deliberately bad eyecatcher.
 * Expected behavior:
 *   - SAF enabled + caller has access    -> BAD_EYECATCHER
 *   - SAF enabled + caller has no access -> NO_ACCESS
 *   - SAF disabled                       -> BAD_EYECATCHER
 *
 * PARM format: server_name service saf_on has_access
 *   server_name : cross-memory server name (e.g. ZWESIS_STD)
 *   service     : AUTH|SNARFER|NWM|USERPROF|GRESPROF|ACSLIST|
 *                 GENRES_ADMIN|GRPPROF|GRPALIST|GROUP_ADMIN
 *   saf_on      : Y or N (is SAF enabled in the PARMLIB?)
 *   has_access  : Y or N (does the caller have access to the SAF profile?)
 *
 * Return codes:
 *   0  - test passed (actual RC matches expected)
 *   1  - usage info requested
 *   4  - test failed (RC mismatch)
 *   8+ - setup/usage error
 */

#include <stdio.h>
#include <string.h>

#include "crossmemory.h"
#include "zis/services/auth.h"
#include "zis/services/snarfer.h"
#include "zis/services/nwm.h"
#include "zis/services/secmgmt.h"

#define SIZE_OF_FIELD($struct, $field) sizeof((($struct *)0)->$field)

#define RC_TEST_PASS 0
#define RC_USAGE 1
#define RC_TEST_FAIL 4
#define RC_BAD_PARM 8
#define RC_CALL_FAILED 16

typedef struct ServiceAttrs_tag {
  int serviceID;
  int rcNoAccess;
  int rcBadEyecatcher;
} ServiceAttrs;

static int getServiceAttrsByName(const char *name, ServiceAttrs *attrs) {
  if (!strcmp(name, "AUTH")) {
    attrs->serviceID = ZIS_SERVICE_ID_AUTH_SRV;
    attrs->rcNoAccess = RC_ZIS_AUTHSRV_NO_ACCESS;
    attrs->rcBadEyecatcher = RC_ZIS_AUTHSRV_BAD_EYECATCHER;
    return 0;
  }
  if (!strcmp(name, "SNARFER")) {
    attrs->serviceID = ZIS_SERVICE_ID_SNARFER_SRV;
    attrs->rcNoAccess = RC_ZIS_SNRFSRV_NO_ACCESS;
    attrs->rcBadEyecatcher = RC_ZIS_SNRFSRV_BAD_EYECATCHER;
    return 0;
  }
  if (!strcmp(name, "NWM")) {
    attrs->serviceID = ZIS_SERVICE_ID_NWM_SRV;
    attrs->rcNoAccess = RC_ZIS_NWMSRV_NO_ACCESS;
    attrs->rcBadEyecatcher = RC_ZIS_NWMSRV_BAD_EYECATCHER;
    return 0;
  }
  if (!strcmp(name, "USERPROF")) {
    attrs->serviceID = ZIS_SERVICE_ID_USERPROF_SRV;
    attrs->rcNoAccess = RC_ZIS_UPRFSRV_NO_ACCESS;
    attrs->rcBadEyecatcher = RC_ZIS_UPRFSRV_BAD_EYECATCHER;
    return 0;
  }
  if (!strcmp(name, "GRESPROF")) {
    attrs->serviceID = ZIS_SERVICE_ID_GRESPROF_SRV;
    attrs->rcNoAccess = RC_ZIS_GRPRFSRV_NO_ACCESS;
    attrs->rcBadEyecatcher = RC_ZIS_GRPRFSRV_BAD_EYECATCHER;
    return 0;
  }
  if (!strcmp(name, "ACSLIST")) {
    attrs->serviceID = ZIS_SERVICE_ID_ACSLIST_SRV;
    attrs->rcNoAccess = RC_ZIS_ACSLSRV_NO_ACCESS;
    attrs->rcBadEyecatcher = RC_ZIS_ACSLSRV_BAD_EYECATCHER;
    return 0;
  }
  if (!strcmp(name, "GENRES_ADMIN")) {
    attrs->serviceID = ZIS_SERVICE_ID_GENRES_ADMIN_SRV;
    attrs->rcNoAccess = RC_ZIS_GSADMNSRV_NO_ACCESS;
    attrs->rcBadEyecatcher = RC_ZIS_GSADMNSRV_BAD_EYECATCHER;
    return 0;
  }
  if (!strcmp(name, "GRPPROF")) {
    attrs->serviceID = ZIS_SERVICE_ID_GRPPROF_SRV;
    attrs->rcNoAccess = RC_ZIS_GPPRFSRV_NO_ACCESS;
    attrs->rcBadEyecatcher = RC_ZIS_GPPRFSRV_BAD_EYECATCHER;
    return 0;
  }
  if (!strcmp(name, "GRPALIST")) {
    attrs->serviceID = ZIS_SERVICE_ID_GRPALIST_SRV;
    attrs->rcNoAccess = RC_ZIS_GRPALSRV_NO_ACCESS;
    attrs->rcBadEyecatcher = RC_ZIS_GRPALSRV_BAD_EYECATCHER;
    return 0;
  }
  if (!strcmp(name, "GROUP_ADMIN")) {
    attrs->serviceID = ZIS_SERVICE_ID_GROUP_ADMIN_SRV;
    attrs->rcNoAccess = RC_ZIS_GRPASRV_NO_ACCESS;
    attrs->rcBadEyecatcher = RC_ZIS_GRPASRV_BAD_EYECATCHER;
    return 0;
  }
  return -1;
}

static int parseFlag(const char *str) {
  if (!strcmp(str, "Y")) {
    return 1;
  }
  if (!strcmp(str, "N")) {
    return 0;
  }
  return -1;
}

static void printUsage(void) {
  printf(
      "usage: zis-saf-test <server_name> <service_name> <saf_on> "
      "<has_access>\n");
}

int main(int argc, char *argv[]) {

  if (argc == 2 && !strcmp(argv[1], "-h")) {
    printUsage();
    return RC_USAGE;
  }

  if (argc < 5) {
    printf("error: not enough arguments\n");
    printUsage();
    return RC_BAD_PARM;
  }

  // server name
  int nameLen = strlen(argv[1]);
  if (nameLen > SIZE_OF_FIELD(CrossMemoryServerName, nameSpacePadded)) {
    printf("error: server name too long\n");
    return RC_BAD_PARM;
  }
  CrossMemoryServerName serverName = cmsMakeServerName(argv[1]);

  // service name
  ServiceAttrs service;
  if (getServiceAttrsByName(argv[2], &service) != 0) {
    printf("error: unknown service '%s'\n", argv[2]);
    return RC_BAD_PARM;
  }

  // flags for the SAF check on/off and for whether the caller is supposed to
  // have access
  int safOn = parseFlag(argv[3]);
  if (safOn < 0) {
    printf("error: saf_on must be Y or N\n");
    return RC_BAD_PARM;
  }
  int hasAccess = parseFlag(argv[4]);
  if (hasAccess < 0) {
    printf("error: has_access must be Y or N\n");
    return RC_BAD_PARM;
  }

  int expectedRC;
  if (safOn && !hasAccess) {
    expectedRC = service.rcNoAccess;
  } else {
    expectedRC = service.rcBadEyecatcher;
  }

  // prepare a fake parmlist (all zeros is a bad eyecatcher for any service)
  // this size should be enough; in the worst case the call will ABEND leading
  // to an unexpected RC and a test failure
  char fakePlist[8192] = {0};

  // call the service
  int serviceRC = 0;
  int cmsRC =
      cmsCallService(&serverName, service.serviceID, fakePlist, &serviceRC);
  if (cmsRC != RC_CMS_OK) {
    printf("error: xmem call failed, cmsRC=%d, serviceRC=%d\n", cmsRC,
           serviceRC);
    if (cmsRC == RC_CMS_PERMISSION_DENIED) {
      printf("error: CMS permission denied (no ZWES.IS access)\n");
    }
    return RC_CALL_FAILED;
  }

  // check the result
  if (serviceRC == expectedRC) {
    printf("info: PASS - serviceRC=%d (expected %d)\n", serviceRC, expectedRC);
    return RC_TEST_PASS;
  } else {
    printf("warn: FAIL - serviceRC=%d (expected %d)\n", serviceRC, expectedRC);
    return RC_TEST_FAIL;
  }
}

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which
  accompanies this distribution, and is available at
  https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/
