

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/

#define  cvtsmcaAddr ((char *__ptr32 *__ptr32 *)0)[4][49]  /* smca field address in CVT */
#define  ifaargs_xrequest_register 1                       /* register request          */
#define  defaultVersion 1                                  /* Version default is 1      */
#define  defaultDomain  1                                  /* Domain default is 1       */
#define  defaultScope   1                                  /* Scope default is 1        */

#define  RC_ZSS_PREG_NULL_X1 1                             /* smf option null rc 1      */
#define  RC_ZSS_PREG_NULL_X2 2                             /* smf option null rc 2      */
#define  RC_ZSS_PREG_NULL_X5 5                             /* storage alloc = null rc 5 */
#define  RC_ZSS_PREG_NULL_X6 6                             /* storage alloc = null rc 6 */
#define  RC_ZSS_PREG_NULL_X7 7                             /* storage alloc = null rc 7 */
#define  ZSS_SMCA_Option_null 0                            /* smf option null           */

#include <stdlib.h>
#include <assert.h>
#include "logging.h"
#include "zssLogging.h"
#include "registerProduct.h"
#include "alloc.h"

typedef struct IFAARGS {
  int prefix;
  char id[8];
  short listlen;
  char version;
  char request;
  char prodowner[16];
  char prodname[16];
  char prodvers[8];
  char prodqual[8];
  char prodid[8];
  char domain;
  char scope;
  char rsv0001;
  char flags;
  uint32_t *__ptr32 prtoken_addr;
  uint32_t *__ptr32 begtime_addr;
  char *__ptr32 data_addr;
  char xformat;
  char rsv0002[3];
  char *__ptr32 currentdata_addr;
  char *__ptr32 enddata_addr;
  char *__ptr32 endtime_addr;
} IFAARGS_t;

int productRegistration(const char *major_version,
                        const char *pid,
                        const char *product_owner,
                        const char *product_name){

  /* Creates buffers for registration product info */

  IFAARGS_t *arg;

  const char MODULE_REGISTER_USAGE[sizeof(arg->id) + 1] = "IFAUSAGE";
  const char PROD_QUAL[sizeof(arg->prodqual) + 1] = "NONE    ";

  char str_product_owner[sizeof(arg->prodowner) + 1];
  char str_product_name[sizeof(arg->prodname) + 1];
  char str_pid[sizeof(arg->prodid) + 1];
  char version[sizeof(arg->prodvers) + 1];
  int ifausage_rc = 0;

  char *smcaOpt = cvtsmcaAddr;
  if (ZSS_SMCA_Option_null == smcaOpt) {
    return RC_ZSS_PREG_NULL_X1;
  }
  if (ZSS_SMCA_Option_null == (*smcaOpt & 0x04)) {
    return RC_ZSS_PREG_NULL_X2;
  }

  /* Left justify with space padding */

  snprintf(str_product_owner, sizeof(str_product_owner),
           "%-*s", sizeof(str_product_owner), product_owner);
  snprintf(str_product_name, sizeof(str_product_name),
           "%-*s", sizeof(str_product_name), product_name);
  snprintf(str_pid, sizeof(str_pid),
           "%-*s", sizeof(str_pid), pid);
  snprintf(version, sizeof(version),
           "%-*s", sizeof(version), major_version);

  /* Register Product with IFAUSAGE */

  ALLOC_STRUCT31(
    STRUCT31_NAME(parms31),
    STRUCT31_FIELDS(
      IFAARGS_t arg; // gives you sizeof(IFAARGS_t) bytes on the stack
      uint32_t prtoken; // gives you 4 bytes on the stack
      uint32_t begtime; // gives you 4 bytes on the stack
    )
  );

  if (parms31 == NULL) {
    return RC_ZSS_PREG_NULL_X5;
  }

  memset(parms31, 0, sizeof(IFAARGS_t));
  memcpy(parms31->arg.id, MODULE_REGISTER_USAGE, sizeof(parms31->arg.id));

  parms31->arg.prtoken_addr = &parms31->prtoken;
  parms31->arg.begtime_addr = &parms31->begtime;

  parms31->arg.listlen = sizeof(IFAARGS_t);
  parms31->arg.version = defaultVersion;
  parms31->arg.request = ifaargs_xrequest_register;

  /* Insert properties */

  memcpy(parms31->arg.prodowner, str_product_owner, sizeof(parms31->arg.prodowner));
  memcpy(parms31->arg.prodname, str_product_name, sizeof(parms31->arg.prodname));
  memcpy(parms31->arg.prodvers, version, sizeof(parms31->arg.prodvers));
  memcpy(parms31->arg.prodqual, PROD_QUAL, sizeof(parms31->arg.prodqual));
  memcpy(parms31->arg.prodid, str_pid, sizeof(parms31->arg.prodid));

  parms31->arg.domain = defaultDomain;
  parms31->arg.scope  = defaultScope;

  /* Load 25 (IFAUSAGE) into reg15 and call via SVC */

  asm(" svc 109\n" : "=NR:r15"(ifausage_rc) : "NR:r1"(&parms31->arg), "NR:r15"(25) :);

  FREE_STRUCT31(
    STRUCT31_NAME(parms31)
  );

  return ifausage_rc;

}

void registerProduct(char *productReg, char *productPID, char *productVer,
                     char *productOwner, char *productName){

  if (productReg != NULL) {
    if (strncasecmp(productReg, "ENABLE", 6) == 0) {
      zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_INFO, ZSS_LOG_PROD_REG_ENABLED_MSG);
    }
    else {
      zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_INFO, ZSS_LOG_PROD_REG_DISABLED_MSG);
      return;
    }
  }
  else {
    zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_INFO, ZSS_LOG_PROD_REG_DISABLED_MSG);
    return;
  }

  int rc = productRegistration(productVer,
                               productPID,
                               productOwner,
                               productName);

  if (rc == 0) {
	zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_INFO, ZSS_LOG_PROD_REG_RC_0_MSG);
  }
  else {
    zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_INFO, ZSS_LOG_PROD_REG_RC_MSG, rc);
  }

}


/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/

