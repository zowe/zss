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
#include <errno.h>

#include "utils.h"
#include "bpxnet.h"
#include "le.h"

/* returns valid */
static int validateAddress(char *address, InetAddr **inetAddress) {
  *inetAddress = getAddressByName(address);
  if (!strcmp(address,"0.0.0.0")) {
    return TRUE;      
  }
  if (!(*inetAddress && (*inetAddress)->data.data4.addrBytes)) {
    printf("Error: Could not resolve hostname from DNS. Is this a valid hostname for this system?\n");
    return FALSE;
  } else {
    int part4 = 0xff & (*inetAddress)->data.data4.addrBytes;
    int part3 = (0xff00 & (*inetAddress)->data.data4.addrBytes) >> 8;
    int part2 = (0xff0000 & (*inetAddress)->data.data4.addrBytes) >> 16;
    int part1 = (0xff000000 & (*inetAddress)->data.data4.addrBytes) >> 24;
    printf("Resolved IP address as %d.%d.%d.%d\n", part1, part2, part3, part4);
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

  int returnCode = 0;
  int reasonCode = 0;
  int port = atoi(getKeywordArg("--port",argc,argv));
  char *address = getKeywordArg("--host",argc,argv);

  InetAddr *inetAddress = NULL;

  if (!validateAddress(address, &inetAddress)) {
    printf("Error: Invalid address given for --host\n");
    return BIND_STATUS_ERROR;
  }
  if (!port) {
    printf("Error: No --port given\n");
    return BIND_STATUS_ERROR;
  }
  
  int tlsFlags = 0;
  Socket *serverSocket = tcpServer2(inetAddress, port, tlsFlags, &returnCode, &reasonCode);
   
  if (serverSocket){
    printf("Bind succeeded (pointer=0x%p, rc=0x%x, rsn=0x%x)\n", serverSocket, returnCode, reasonCode);
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
  return status;
}
