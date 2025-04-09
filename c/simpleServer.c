

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
#include <stdarg.h>
#include <sys/stat.h>
#include <iconv.h>
#include <dirent.h>
#include <errno.h>
#include <pthread.h>
#include <signal.h>

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
#include "charsets.h"
#ifdef __ZOWE_OS_ZOS
#include "zis/client.h"
#endif

static void setPrivilegedServerNameV2(HttpServer *server, char *serverNameParm){

  CrossMemoryServerName *privilegedServerName = (CrossMemoryServerName *)SLHAlloc(server->slh, sizeof(CrossMemoryServerName));
  
  *privilegedServerName = cmsMakeServerName(serverNameParm);
  setConfiguredProperty(server, HTTP_SERVER_PRIVILEGED_SERVER_PROPERTY, privilegedServerName);
}

static void printZISStatus(HttpServer *server) {

  CrossMemoryServerName *zisName =
      getConfiguredProperty(server, HTTP_SERVER_PRIVILEGED_SERVER_PROPERTY);

  CrossMemoryServerStatus status = cmsGetStatus(zisName);

  const char *shortDescription = NULL;

  if (status.cmsRC == RC_CMS_OK) {
    shortDescription = "Ok";
  } else {
    shortDescription = "Failure";
  }

  printf("ZIS status - '%s' (name='%.16s', cmsRC='%d', description='%s', clientVersion='%d')\n", 
          shortDescription,
          zisName ? zisName->nameSpacePadded : "name not set",
          status.cmsRC,
          status.descriptionNullTerm,
          CROSS_MEMORY_SERVER_VERSION);

}

/* returns valid */
static int validateAddress(char *address, InetAddr **inetAddress) {
  *inetAddress = getAddressByName(address);
  if (!strcmp(address,"0.0.0.0")) {
    return TRUE;      
  }
  if (!(*inetAddress && (*inetAddress)->data.data4.addrBytes)) {
    printf("address resolution problems\n");
    return FALSE;
  }

  /* TODO: No ipv6 resolution in getAddressByName yet, so nothing here either */
  return TRUE;
}

static char *getKeywordArg(char *key, int argc, char **argv){
  for (int aa=1; aa<argc; aa++){
    if (!strcmp(argv[aa],key) &&
        (aa+1 < argc)){
      return argv[aa+1];
    }
  }
  return NULL;

}

#define ZSS_STATUS_OK     0
#define ZSS_STATUS_ERROR  8

int main(int argc, char **argv){
  int zssStatus = ZSS_STATUS_OK;

  STCBase *base = (STCBase*) safeMalloc31(sizeof(STCBase), "stcbase");
  memset(base, 0x00, sizeof(STCBase));
  stcBaseInit(base); /* inits RLEAnchor, workQueue, socketSet, logContext */

  int returnCode = 0;
  int reasonCode = 0;
  char *tempString;
  int port = atoi(getKeywordArg("--port",argc,argv));
  char *address = getKeywordArg("--host",argc,argv);
  char *zisName = getKeywordArg("--zis",argc,argv);

  ShortLivedHeap *slh = makeShortLivedHeap(0x40000, 0x40);

  HttpServer *server = NULL;
  InetAddr *inetAddress = NULL;

  if (!validateAddress(address, &inetAddress)) {
    printf("Invalid address given for --host\n");
    zssStatus = ZSS_STATUS_ERROR;
    goto out_term_stcbase;
  }
  if (!port) {
    printf("No --port given\n");
    zssStatus = ZSS_STATUS_ERROR;
    goto out_term_stcbase;
  }
  
  int requiredTLSFlag = FALSE;
  char *cookieName = "zowe.validate";
  server = makeHttpServer3(base, inetAddress, port, requiredTLSFlag, cookieName, &returnCode, &reasonCode);
  server->slh = slh;
   
  if (server){
    printf("Bind succeeded, server at 0x%p\n",server);

    if (zisName) {
      setPrivilegedServerNameV2(server, zisName);
      printZISStatus(server);
    }
  } else{
    printf("Server could not start, rc=0x%x, rsn=0x%x\n", returnCode, reasonCode);
    if (returnCode==EADDRINUSE) {
      printf("Server could not start because the port %d was occupied\n", port);
    }
  }

out_term_stcbase:
  stcBaseTerm(base);
  safeFree31((char *)base, sizeof(STCBase));
  base = NULL;

  return zssStatus;
}
