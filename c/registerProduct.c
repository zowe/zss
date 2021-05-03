

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
#include <metal/stdio.h>
#include <metal/stdlib.h>
#include <metal/string.h>
#include <metal/stdarg.h>
#include "metalio.h"

#else
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <iconv.h>
#include <dirent.h>
#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <assert.h>
#include <stdbool.h>

#endif

#include "zowetypes.h"
#include "alloc.h"
#include "utils.h"
#ifdef __ZOWE_OS_ZOS
#include "zos.h"
#endif
#include "bpxnet.h"
#include "collections.h"
#include "unixfile.h"
#include "socketmgmt.h"
#include "le.h"
#include "logging.h"
#include "scheduling.h"
#include "json.h"

#include "xml.h"
#include "httpserver.h"
#include "httpfileservice.h"
#include "dataservice.h"
#include "envService.h"
#ifdef __ZOWE_OS_ZOS
#include "zosDiscovery.h"
#endif
#include "charsets.h"
#include "plugins.h"
#ifdef __ZOWE_OS_ZOS
#include "datasetjson.h"
#include "authService.h"
#include "securityService.h"
#include "zis/client.h"
#endif

#include "zssLogging.h"
#include "serviceUtils.h"
#include "unixFileService.h"
#include "omvsService.h"
#include "datasetService.h"
#include "serverStatusService.h"
#include "rasService.h"
#include "certificateService.h"
#include "registerProduct.h"

#include "jwt.h"

#define PRODUCT "ZLUX"
#ifndef PRODUCT_MAJOR_VERSION
#define PRODUCT_MAJOR_VERSION 0
#endif
#ifndef PRODUCT_MINOR_VERSION
#define PRODUCT_MINOR_VERSION 0
#endif
#ifndef PRODUCT_REVISION
#define PRODUCT_REVISION 0
#endif
#ifndef PRODUCT_VERSION_DATE_STAMP
#define PRODUCT_VERSION_DATE_STAMP 0
#endif

unsigned long long __registerProduct(const char *major_version,
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

  /* Check if SMF is Active first  */
  char *xx = ((char *__ptr32 *__ptr32 *)0)[4][49];
  if (0 == xx) {
    return 1;
  }
  if (0 == (*xx & 0x04)) {
    return 2;
  }

  /* zowelog info */

  zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_INFO,"Major_version : %s\n", major_version);
  zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_INFO,"Product_PID.. : %s\n", pid);
  zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_INFO,"Product_owner : %s\n", product_owner);
  zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_INFO,"Product_name. : %s\n", product_name);
  zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_INFO,"Feature_name. : %s\n", feature_name);

  /*
  return 0;
  */

  /* Left justify with space padding */

  snprintf(str_product_owner, sizeof(str_product_owner), "%-16s",
           product_owner);
  snprintf(str_feature_name, sizeof(str_feature_name), "%-16s", feature_name);
  snprintf(str_product_name, sizeof(str_product_name), "%-16s", product_name);
  snprintf(str_pid, sizeof(str_pid), "%-8s", pid);
  snprintf(version, sizeof(version), "%-8s", major_version);
  /* Register Product with IFAUSAGE */

/*IFAARGS_t *arg = (IFAARGS_t *)__malloc31(sizeof(IFAARGS_t)); */
  arg = (IFAARGS_t *)__malloc31(sizeof(IFAARGS_t));

  assert(arg);
  memset(arg, 0, sizeof(IFAARGS_t));
  memcpy(arg->id, MODULE_REGISTER_USAGE, 8);
  arg->listlen = sizeof(IFAARGS_t);
  arg->version = 1;
  arg->request = 1; /* 1=REGISTER */

  /* Insert properties */

  memcpy(arg->prodowner, str_product_owner, 16);
  memcpy(arg->prodname, str_product_name, 16);
  memcpy(arg->prodvers, version, 8);
  memcpy(arg->prodqual, "NONE    ", 8);
  memcpy(arg->prodid, str_pid, 8);
  arg->domain = 1;
  arg->scope = 1;


  arg->prtoken_addr = (char *__ptr32)__malloc31(sizeof(char *__ptr32));
  arg->begtime_addr = (char *__ptr32)__malloc31(sizeof(char *__ptr32));
  /* Load 25 (IFAUSAGE) into reg15 and call via SVC */
  asm(" svc 109\n" : "=NR:r15"(ifausage_rc) : "NR:r1"(arg), "NR:r15"(25) :);
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

  unsigned long long rc = __registerProduct(productVer ,
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

