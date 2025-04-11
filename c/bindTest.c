

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
#include "zos.h"
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

#define BIND_STATUS_OK     0
#define BIND_STATUS_ERROR  8

int main(int argc, char **argv){
  int status = BIND_STATUS_OK;

  STCBase *base = (STCBase*) safeMalloc31(sizeof(STCBase), "stcbase");
  memset(base, 0x00, sizeof(STCBase));
  stcBaseInit(base); /* inits RLEAnchor, workQueue, socketSet, logContext */

  int returnCode = 0;
  int reasonCode = 0;
  int port = atoi(getKeywordArg("--port",argc,argv));
  char *address = getKeywordArg("--host",argc,argv);

  HttpServer *server = NULL;
  InetAddr *inetAddress = NULL;

  if (!validateAddress(address, &inetAddress)) {
    printf("Error: Invalid address given for --host\n");
    status = BIND_STATUS_ERROR;
    goto out_term_stcbase;
  }
  if (!port) {
    printf("Error: No --port given\n");
    status = BIND_STATUS_ERROR;
    goto out_term_stcbase;
  }
  
  int requiredTLSFlag = FALSE;
  char *cookieName = "zowe.validate";
  server = makeHttpServer3(base, inetAddress, port, requiredTLSFlag, cookieName, &returnCode, &reasonCode);
   
  if (server){
    printf("Bind succeeded (pointer=0x%p, rc=0x%x, rsn=0x%x)\n",server, returnCode, reasonCode);
  } else{
    status = BIND_STATUS_ERROR;
    char *jobname = getenv("_BPX_JOBNAME");
    printf("Error: Bind failed (rc=0x%x, rsn=0x%x)\n", returnCode, reasonCode);
    if (returnCode==EADDRINUSE) {
      printf("Error: Port %d was already occupied\n", port);
    } else if (jobname) {
      char *username = getenv("USER");
      if (username) {
        printf("Ensure jobname %s for the Zowe STC id (possibly the current user: %s) has permission to make TCPIP binds to %s:%d\n", jobname, username, address, port);
      } else {
        printf("Ensure jobname %s for the Zowe STC id has permission to make TCPIP binds to %s:%d\n", jobname, address, port);
      }
    } else {
      printf("Ensure the Zowe STC job and STC id has permission to make TCPIP binds to %s:%d\n", address, port);
    }
  }

out_term_stcbase:
  stcBaseTerm(base);
  safeFree31((char *)base, sizeof(STCBase));
  base = NULL;

  return status;
}
