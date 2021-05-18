

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/

#define  cvtsmcaAddr ((char *__ptr32 *__ptr32 *)0)[4][49]  /* smca field address in CVT */
#define  ifaargs_xrequest_register 1                       /* register request          */
#define  RC_ZSS_PREG_NULL_X1 1                             /* smf option null rc 1      */
#define  RC_ZSS_PREG_NULL_X2 2                             /* smf option null rc 2      */
#define  ZSS_SMCA_Option_null 0                            /* smf option null           */
#include <stdlib.h>
#include <assert.h>
#include "logging.h"
#include "zssLogging.h"
#include "registerProduct.h"

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
  char *__ptr32 prtoken_addr;
  char *__ptr32 begtime_addr;
  char *__ptr32 data_addr;
  char xformat;
  char rsv0002[3];
  char *__ptr32 currentdata_addr;
  char *__ptr32 enddata_addr;
  char *__ptr32 endtime_addr;
} IFAARGS_t;

unsigned long long productRegistration(const char *major_version,
                              const char *pid,
                              const char *product_owner,
                              const char *product_name,
                              const char *feature_name){

  /* Creates buffers for registration product info */

  char str_product_owner[17];
  char str_feature_name[17];
  char str_product_name[17];
  char str_pid[9];
  char version[9];
  unsigned long long ifausage_rc = 0;
  IFAARGS_t *arg;

  char *smcaOpt = cvtsmcaAddr;
  if (ZSS_SMCA_Option_null == smcaOpt) {
    return RC_ZSS_PREG_NULL_X1;
  }
  if (ZSS_SMCA_Option_null == (*smcaOpt & 0x04)) {
    return RC_ZSS_PREG_NULL_X2;
  }

  /* Left justify with space padding */

  snprintf(str_product_owner, sizeof(str_product_owner), "%-16s",
           product_owner);
  snprintf(str_feature_name, sizeof(str_feature_name), "%-16s", feature_name);
  snprintf(str_product_name, sizeof(str_product_name), "%-16s", product_name);
  snprintf(str_pid, sizeof(str_pid), "%-8s", pid);
  snprintf(version, sizeof(version), "%-8s", major_version);

  /* Register Product with IFAUSAGE */

  arg = (IFAARGS_t *)__malloc31(sizeof(IFAARGS_t));

  assert(arg);
  memset(arg, 0, sizeof(IFAARGS_t));
  memcpy(arg->id, MODULE_REGISTER_USAGE, 8);
  arg->listlen = sizeof(IFAARGS_t);
  arg->version = 1;
  arg->request = ifaargs_xrequest_register;

  /* Insert properties */

  memcpy(arg->prodowner, str_product_owner, 16);
  memcpy(arg->prodname, str_product_name, 16);
  memcpy(arg->prodvers, version, 8);
  memcpy(arg->prodqual, "NONE    ", 8);
  memcpy(arg->prodid, str_pid, 8);
  arg->domain = 1;
  arg->scope = 1;


  arg->prtoken_addr = (char *__ptr32)__malloc31(sizeof(char *__ptr32));
  assert(arg->prtoken_addr);
  arg->begtime_addr = (char *__ptr32)__malloc31(sizeof(char *__ptr32));
  assert(arg->begtime_addr);
  /* Load 25 (IFAUSAGE) into reg15 and call via SVC */
  asm(" svc 109\n" : "=NR:r15"(ifausage_rc) : "NR:r1"(arg), "NR:r15"(25) :);
  free(arg->prtoken_addr);
  free(arg->begtime_addr);
  free(arg);

  return ifausage_rc;

}

void registerProduct(char *productReg, char *productPID, char *productVer,
                     char *productOwner, char *productName, char *productFeature){

  if (productReg != NULL) {
    if (strncasecmp(productReg, "ENABLE", 6) == 0) {
      zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_INFO, "Product Registration is enabled.\n");
    }
    else {
      zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_INFO, "Product Registration is disabled.\n");
      return;
    }
  }
  else {
    zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_INFO, "Product Registration is disabled.\n");
    return;
  }

  unsigned long long rc = productRegistration(productVer,
                                     productPID,
                                     productOwner,
                                     productName,
                                     productFeature);

  zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_INFO, "Product Registration RC = %llu\n", rc);

}


/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/

