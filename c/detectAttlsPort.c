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

#include "ezbztlsc.h" /* for TTLS ioctl support */


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

static char *getKeywordArg(char *key, int argc, char **argv) {
  for (int aa=1; aa<argc; aa++) {
    if (!strcmp(argv[aa],key) &&
        (aa+1 < argc)) {
      // if --self-check then return the argument itself
      if(!strcmp(argv[aa], "--self-check")) {
        return argv[aa];
      }
      return argv[aa+1];
    }
  }
  return NULL;
}

#define DETECT_ATTLS_PORT_STATUS_OK     0
#define DETECT_ATTLS_PORT_STATUS_ERROR  8

int main(int argc, char **argv) {
  int status = DETECT_ATTLS_PORT_STATUS_OK;

  if (argc == 1 || (argc == 2 && (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0))) {
    printf("detect-attls-port - detects if AT-TLS is enabled in a live port.\n");
    printf("  Format: [_BPX_JOBNAME=jobname] detect-attls-port --server hostname_or_ipv4 --port tcp_port --self-check [Use If you want to open a listening port,"
           " and check if AT-TLS is enabled\n.");
    printf("  Exit values: 0 AT-TLS has been enabled, 8 otherwise\n");
    return DETECT_ATTLS_PORT_STATUS_OK;
  }  

  int returnCode = 0;
  int reasonCode = 0;
  int port = atoi(getKeywordArg("--port",argc,argv));
  char *address = getKeywordArg("--host",argc,argv);
  int selfCheck = FALSE;
  if (getKeywordArg("--self-check",argc,argv)) {
    selfCheck = TRUE;
  }
  
  InetAddr *inetAddress = NULL;

  if (!validateAddress(address, &inetAddress)) {
    printf("Error: Invalid address given for --host\n");
    return DETECT_ATTLS_PORT_STATUS_ERROR;
  }
  if (!port) {
    printf("Error: No --port given\n");
    return DETECT_ATTLS_PORT_STATUS_ERROR;
  }

  // self check if port has AT-TLS enabled as both server and client
  if(selfCheck) {

  } else { // Check destination port
    int tlsFlags = 0;
    Socket *serverSocket = tcpServer2(inetAddress, port, tlsFlags, &returnCode, &reasonCode);
    
    if (serverSocket) {
      //printf("Bind succeeded (pointer=0x%p, rc=0x%x, rsn=0x%x)\n", serverSocket, returnCode, reasonCode);
      int bpxrc=0, bpxrsn=0;
      
      struct TTLS_IOCTL ioc;              /* ioctl data structure          */
      memset(&ioc,0,sizeof(ioc));         /* set all unused fields to zero */

      arglen = sizeof(ioc);
      sts = tcpIOControl(serverSocket, SIOCTTLSCTL, arglen, (char*)&ioc, &bpxrc, &bpxrsn);

      int policy = ioc.TTLSi_Stat_Policy; // https://www.ibm.com/docs/en/zos/2.4.0?topic=ioctl-siocttlsctl-xc038d90b
      switch (policy) {
        case TTLS_POL_OFF:
        case TTLS_POL_NO_POLICY:
        case TTLS_POL_NOT_ENABLED:
          printf("AT-TLS is not enabled\n");
          break;
        case TTLS_POL_ENABLED:
        case TTLS_POL_APPLCNTRL:
          printf("AT-TLS is enabled\n");
          break;
      }
    } else {
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
    socketClose(serverSocket, &returnCode, &reasonCode);
    socketFree(serverSocket);
  }
 
  return status;
}
