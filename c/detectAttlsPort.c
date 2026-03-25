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

#include "utils.h"
#include "bpxnet.h"
#include "le.h"

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
      return argv[aa+1];
    }
  }
  return NULL;
}

#define STATUS_OK                             0 // Generic success
#define DETECT_ATTLS_PORT_STATUS_ENABLED      0
#define DETECT_ATTLS_PORT_STATUS_DISABLED     4
#define DETECT_ATTLS_PORT_STATUS_ERROR        8

int querySocketForAttls(Socket *socket, const char *jobname, const char *username, const char *serverAddress, int serverPort) {
  struct TTLS_IOCTL ioc;              /* ioctl data structure          */
  memset(&ioc,0,sizeof(ioc));         /* set all unused fields to zero */

  ioc.TTLSi_Ver = TTLS_VERSION1;
  ioc.TTLSi_Req_Type = TTLS_QUERY_ONLY;

  int arglen = sizeof(ioc);
  int bpxrc=0, bpxrsn=0;
  int sts = tcpIOControl(socket, SIOCTTLSCTL, arglen, (char*)&ioc, &bpxrc, &bpxrsn);
  if (sts != 0 || bpxrc != 0) {
    printf("SIOCTTLSCTL failed on %s:%d (bpxrc=%d, bpxrsn=%d)\n",
           serverAddress, serverPort, bpxrc, bpxrsn);
    return DETECT_ATTLS_PORT_STATUS_ERROR;
  }
  
  int status = DETECT_ATTLS_PORT_STATUS_ERROR;
  int policy = ioc.TTLSi_Stat_Policy; // https://www.ibm.com/docs/en/zos/2.4.0?topic=ioctl-siocttlsctl-xc038d90b
  switch (policy) {
   case TTLS_POL_OFF:
   case TTLS_POL_NO_POLICY:
   case TTLS_POL_NOT_ENABLED:
     status = DETECT_ATTLS_PORT_STATUS_DISABLED;
     printf("No AT-TLS rule identified on %s:%d for user %s and jobname %s\n", serverAddress, serverPort, username, jobname);
     break;
   case TTLS_POL_ENABLED:
   case TTLS_POL_APPLCNTRL:
     status = DETECT_ATTLS_PORT_STATUS_ENABLED;
     printf("AT-TLS rule identified on %s:%d for user %s and jobname %s\n", serverAddress, serverPort, username, jobname);
     break;
   default:
     printf("Error: Unknown AT-TLS policy identified on %s:%d for user %s and jobname %s\n", serverAddress, serverPort, username, jobname);
     status = DETECT_ATTLS_PORT_STATUS_ERROR;  
  }
  return status;
}


int main(int argc, char **argv) {
  int status = DETECT_ATTLS_PORT_STATUS_ERROR;

  if (argc == 1 || (argc == 2 && (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0))) {
    printf("detect-attls-port - detects if AT-TLS is enabled in a live port.\n");
    printf("  Format: [_BPX_JOBNAME=jobname] detect-attls-port --serverPort tcp_port --serverHost hostname_or_ipv4"
           " --type {1=server | 2=client \n");
    printf("  Exit values: 0 AT-TLS has been enabled, 4 disabled and 8 other errors\n");
    return STATUS_OK;
  }  

  int returnCode = 0;
  int reasonCode = 0;
    
  // SERVER INFO
  char *serverAddress = getKeywordArg("--serverHost", argc, argv);
  if (!serverAddress) {
    printf("Error: Missing required argument --serverHost\n");
    return DETECT_ATTLS_PORT_STATUS_ERROR;
  }
  InetAddr *serverInetAddress = NULL;
  if (!validateAddress(serverAddress, &serverInetAddress)) {
    printf("Error: Invalid address given for --serverHost\n");
    return DETECT_ATTLS_PORT_STATUS_ERROR;
  }
  const char *portArg = getKeywordArg("--serverPort", argc, argv);
  if (!portArg) {
    printf("Error: Missing required argument --serverPort\n");
    return DETECT_ATTLS_PORT_STATUS_ERROR;
  }
  int serverPort = atoi(portArg);
  if (serverPort <= 0 || serverPort > 65535) {
    printf("Error: Invalid --serverPort value (must be 1-65535) \n");
    return DETECT_ATTLS_PORT_STATUS_ERROR;
  }

  // Type of connection this program must become: server or client
  const char *typeArg = getKeywordArg("--type", argc, argv); 
  if (!typeArg) {
    printf("Error: Missing required argument --type\n");
    return DETECT_ATTLS_PORT_STATUS_ERROR;
  }
  int connType = atoi(typeArg);
  if (connType != 1 && connType !=2) {
    printf("The connection type can either be '1' (server) or '2' (client)\n");
    return DETECT_ATTLS_PORT_STATUS_ERROR;
  }
  
  const char *jobname = getenv("_BPX_JOBNAME");
  jobname = jobname ? jobname : "<unknown>";

  const char *username = getenv("USER");
  username = username ? username : "<unknown>";
  printf("CLI on %s:%d for user %s and jobname %s connection type %d\n",
             serverAddress, serverPort, jobname, username, connType);

  if(connType == 1) {
    int tlsFlags = 0;
    // Create server
    Socket *serverSocket = tcpServer2(serverInetAddress, serverPort, tlsFlags, &returnCode, &reasonCode);
    if (serverSocket) {
      printf("Success: Server is ready\n");
      // wait for a connection request
      Socket *peerSocket = socketAccept(serverSocket,&returnCode,&reasonCode);
      if (peerSocket == NULL) {
        printf("Server: accept failed ret=%d reason=0x%x\n", returnCode, reasonCode);
        /* end server socket processing clean up and exit*/
      } else {
        printf("Server: accept succeeded now query for ATTLS\n");
        status = querySocketForAttls(peerSocket, jobname, username, serverAddress, serverPort);
        socketClose(peerSocket, &returnCode, &reasonCode);
      }
      printf("Closing server at %s:%d\n", serverAddress, serverPort);
      socketClose(serverSocket, &returnCode, &reasonCode);
      socketFree(serverSocket);
    } else {
      status = DETECT_ATTLS_PORT_STATUS_ERROR;
      printf("Error: Bind failed (rc=0x%x, rsn=0x%x)\n", returnCode, reasonCode);
      if (returnCode == EADDRINUSE) {
        printf("Error: Port %d was already occupied\n", serverPort);
      } else if (jobname) {
        if (username) {
          printf("Ensure jobname %s for the Zowe STC id (possibly the current user: %s) has permission to make TCPIP binds to %s:%d\n", jobname, username, serverAddress, serverPort);
        } else {
          printf("Ensure jobname %s for the Zowe STC id has permission to make TCPIP binds to %s:%d\n", jobname, serverAddress, serverPort);
        }
      } else {
        printf("Ensure the Zowe STC job and STC id has permission to make TCPIP binds to %s:%d\n", serverAddress, serverPort);
      }
    }
  } else if (connType == 2) {
      SocketAddress *serverSocketAddress = makeSocketAddr(serverInetAddress, serverPort);
      Socket *clientSocket = tcpClient2(serverSocketAddress, 1000 * 10, &returnCode, &reasonCode);
      if ((returnCode != 0) || (NULL == clientSocket)) {
        printf("Failed to connect to HTTP server (rc=%d, rsn=0x%x, addr=0x%08x, port=%d)\n", returnCode, reasonCode,
          serverSocketAddress->v4Address, serverSocketAddress->port);
        if (serverSocketAddress) {
           freeSocketAddr(serverSocketAddress);
        }
      } else {
        printf("Connection succeeded with server addr=0x%08x, port=%d\n", serverSocketAddress->v4Address, serverSocketAddress->port);
        status = querySocketForAttls(clientSocket, jobname, username, serverAddress, serverPort);
        if (serverSocketAddress) {
           freeSocketAddr(serverSocketAddress);
        }
        socketClose(clientSocket, &returnCode, &reasonCode);
        socketFree(clientSocket); 
      }
  }
  return status;
}
