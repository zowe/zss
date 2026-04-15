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
#include "fdpoll.h"

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

static char *getKeywordArg(const char *key, int argc, char **argv) {
  for (int aa=1; aa<argc; aa++) {
    if (!strcmp(argv[aa],key) &&
        (aa+1 < argc)) {
      return argv[aa+1];
    }
  }
  return NULL;
}

#define STATUS_OK                                   0  // Generic success
#define DETECT_ATTLS_PORT_STATUS_ENABLED            0  // ATTLS enabled
#define DETECT_ATTLS_PORT_STATUS_DISABLED           4  // ATTLS NOT enabled
#define DETECT_ATTLS_PORT_TLS_ERROR                 8  // tls errors like 'clientAuth' failure
#define DETECT_ATTLS_PORT_STATUS_ERROR              12 // generic errors

typedef struct {
  const char *jobname;
  const char *username;
  const char *serverAddress;
  int serverPort;
} AttlsContext;

static void printHelpAndExit() {
  printf("detect-attls-port - detects if AT-TLS is enabled in a live port.\n");
  printf("  Format: [_BPX_JOBNAME=jobname] detect-attls-port --serverPort tcp_port --serverHost hostname_or_ipv4"
         "--direction {1-Inbound | 2-outBound}\n");
  printf("  Exit values: 0 AT-TLS has been enabled, 4 disabled and 8 other errors\n");
}

static int validateCLIArguments(int argc, char **argv,
                                char **serverAddress, int *serverPort,
                                int *direction, InetAddr **inetAddr) {
  if (argc == 1 || (argc == 2 && (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0))) {
    printHelpAndExit();
    return -1; // Signal help requested
  }

  *serverAddress = getKeywordArg("--serverHost", argc, argv);
  if (!*serverAddress) {
    printf("Error: Missing required argument --serverHost\n");
    return DETECT_ATTLS_PORT_STATUS_ERROR;
  }
  if (!validateAddress(*serverAddress, inetAddr)) {
    printf("Error: Invalid address given for --serverHost\n");
    return DETECT_ATTLS_PORT_STATUS_ERROR;
  }

  const char *portArg = getKeywordArg("--serverPort", argc, argv);
  if (!portArg) {
    printf("Error: Missing required argument --serverPort\n");
    return DETECT_ATTLS_PORT_STATUS_ERROR;
  }
  *serverPort = atoi(portArg);
  if (*serverPort <= 0 || *serverPort > 65535) {
    printf("Error: Invalid --serverPort value (must be 1-65535)\n");
    return DETECT_ATTLS_PORT_STATUS_ERROR;
  }

  const char *directionArg = getKeywordArg("--direction", argc, argv);
  if (!directionArg) {
    printf("Error: Missing required argument --direction\n");
    return DETECT_ATTLS_PORT_STATUS_ERROR;
  }
  *direction = atoi(directionArg);
  if (*direction != 1 && *direction != 2) {
    printf("The direction can either be '1' (Inbound) or '2' (Outbound)\n");
    return DETECT_ATTLS_PORT_STATUS_ERROR;
  }

  return 0; // Success
}

static void handleBindError(int returnCode, AttlsContext *ctx) {
  printf("Error: Bind failed (rc=0x%x, rsn=0x%x)\n", returnCode, returnCode);
  if (returnCode == EADDRINUSE) {
    printf("Error: Port %d was already occupied\n", ctx->serverPort);
  } else if (ctx->jobname && strcmp(ctx->jobname, "<unknown>") != 0) {
    if (ctx->username && strcmp(ctx->username, "<unknown>") != 0) {
      printf("Ensure jobname %s for the Zowe STC id (possibly the current user: %s) has permission to make TCPIP binds to %s:%d\n",
             ctx->jobname, ctx->username, ctx->serverAddress, ctx->serverPort);
    } else {
      printf("Ensure jobname %s for the Zowe STC id has permission to make TCPIP binds to %s:%d\n",
             ctx->jobname, ctx->serverAddress, ctx->serverPort);
    }
  } else {
    printf("Ensure the Zowe STC job and STC id has permission to make TCPIP binds to %s:%d\n",
           ctx->serverAddress, ctx->serverPort);
  }
}

static int testConnectionAndQueryAttls(Socket *serverSocket, InetAddr *inetAddr, int serverPort,
                                       AttlsContext *ctx, int direction,
                                       SocketAddress **outSocketAddr) {
  int returnCode = 0;
  int reasonCode = 0;
  int status = DETECT_ATTLS_PORT_STATUS_ERROR;

  printf("Success: Server is ready\n");
  *outSocketAddr = makeSocketAddr(inetAddr, serverPort);
  Socket *clientSocket = tcpClient2(*outSocketAddr, 1000 * 10, &returnCode, &reasonCode);

  if (returnCode != 0 || clientSocket == NULL) {
    printf("Failed to connect to server (rc=%d, rsn=0x%x, addr=0x%08x, port=%d)\n",
           returnCode, reasonCode, (*outSocketAddr)->v4Address, (*outSocketAddr)->port);
    return DETECT_ATTLS_PORT_STATUS_ERROR;
  }

  // Poll on listening socket
  #define POLL_TIME 200
  PollItem item = {0};
  item.fd = serverSocket->sd;
  item.events = POLLRIN;
  int pollStatus = fdPoll(&item, 0, 1, POLL_TIME, &returnCode, &reasonCode);
  printf("BPXPOL: returnValue = %d, ret: %d, rsn: %d\n", pollStatus, returnCode, reasonCode);

  if (pollStatus == -1) {
    printf("Waited out full duration.\n");
    socketClose(clientSocket, &returnCode, &reasonCode);
    socketFree(clientSocket);
    return DETECT_ATTLS_PORT_STATUS_ERROR;
  }

  if (!(item.revents & POLLRIN)) {
    socketClose(clientSocket, &returnCode, &reasonCode);
    socketFree(clientSocket);
    return DETECT_ATTLS_PORT_STATUS_ERROR;
  }

  printf("Listening Socket polled: OK.\n");
  Socket *peerSocket = socketAccept(serverSocket, &returnCode, &reasonCode);

  if (peerSocket == NULL) {
    printf("Server: accept failed ret=%d reason=0x%x\n", returnCode, reasonCode);
    socketClose(clientSocket, &returnCode, &reasonCode);
    socketFree(clientSocket);
    return DETECT_ATTLS_PORT_STATUS_ERROR;
  }

  printf("Connection succeeded with server addr=0x%08x, port=%d\n",
         (*outSocketAddr)->v4Address, (*outSocketAddr)->port);

  char writeBuffer[50] = "ATTLS detect binary";
  char readBuffer[50] = {0};
  int writeReturn = socketWrite(clientSocket, writeBuffer, strlen(writeBuffer), &returnCode, &reasonCode);

  if (writeReturn < 0) {
    printf("write failed ret=%d reason=0x%x\n", returnCode, reasonCode);
    status = DETECT_ATTLS_PORT_TLS_ERROR;
  } else {
    int bytesRead = socketRead(peerSocket, readBuffer, sizeof(readBuffer), &returnCode, &reasonCode);
    if (bytesRead == -1 || bytesRead == 0) {
      printf("socket read error or no bytes read, errno = %d\n", returnCode);
      status = DETECT_ATTLS_PORT_TLS_ERROR;
    } else {
      printf("Client sent message: %s\n", readBuffer);
      if (direction == 1) {
        printf("...Now query for Inbound ATTLS state\n");
        status = querySocketForAttls(peerSocket, ctx);
      } else {
        printf("...Now query for Outbound ATTLS state\n");
        status = querySocketForAttls(clientSocket, ctx);
      }
    }
  }

  socketClose(clientSocket, &returnCode, &reasonCode);
  socketFree(clientSocket);
  socketClose(peerSocket, &returnCode, &reasonCode);
  return status;
}

int querySocketForAttls(Socket *socket, AttlsContext *ctx) {
  struct TTLS_IOCTL ioc;              /* ioctl data structure          */
  memset(&ioc,0,sizeof(ioc));         /* set all unused fields to zero */

  ioc.TTLSi_Ver = TTLS_VERSION1;
  ioc.TTLSi_Req_Type = TTLS_QUERY_ONLY;

  int arglen = sizeof(ioc);
  int bpxrc=0;
  int bpxrsn=0;
  int sts = tcpIOControl(socket, SIOCTTLSCTL, arglen, (char*)&ioc, &bpxrc, &bpxrsn);
  if (sts != 0 || bpxrc != 0) {
    printf("SIOCTTLSCTL failed on %s:%d (bpxrc=%d, bpxrsn=%d)\n",
           ctx->serverAddress, ctx->serverPort, bpxrc, bpxrsn);
    return DETECT_ATTLS_PORT_STATUS_ERROR;
  }
  
  int status = DETECT_ATTLS_PORT_STATUS_ERROR;
  int policy = ioc.TTLSi_Stat_Policy; // https://www.ibm.com/docs/en/zos/2.4.0?topic=ioctl-siocttlsctl-xc038d90b
  switch (policy) {
   case TTLS_POL_OFF:
   case TTLS_POL_NO_POLICY:
   case TTLS_POL_NOT_ENABLED:
     status = DETECT_ATTLS_PORT_STATUS_DISABLED;
     printf("No AT-TLS rule identified on %s:%d for user %s and jobname %s\n", ctx->serverAddress, ctx->serverPort, ctx->username, ctx->jobname);
     break;
   case TTLS_POL_ENABLED:
   case TTLS_POL_APPLCNTRL:
     status = DETECT_ATTLS_PORT_STATUS_ENABLED;
     printf("AT-TLS rule identified on %s:%d for user %s and jobname %s\n", ctx->serverAddress, ctx->serverPort, ctx->username, ctx->jobname);
     break;
   default:
     printf("Error: Unknown AT-TLS policy identified on %s:%d for user %s and jobname %s\n", ctx->serverAddress, ctx->serverPort, ctx->username, ctx->jobname);
     status = DETECT_ATTLS_PORT_STATUS_ERROR;  
  }
  return status;
}


int main(int argc, char **argv) {
  char *serverAddress = NULL;
  int serverPort = 0;
  int direction = 0;
  InetAddr *serverInetAddress = NULL;
  int returnCode = 0;
  int reasonCode = 0;
  int status = DETECT_ATTLS_PORT_STATUS_ERROR;

  // Validate CLI arguments
  int argValidation = validateCLIArguments(argc, argv, &serverAddress, &serverPort, &direction, &serverInetAddress);
  if (argValidation == -1) {
    return STATUS_OK; // Help was requested
  }
  if (argValidation != 0) {
    return argValidation; // Validation failed
  }

  // Create AttlsContext
  AttlsContext ctx = {0};
  ctx.jobname = getenv("_BPX_JOBNAME");
  ctx.jobname = ctx.jobname ? ctx.jobname : "<unknown>";
  ctx.username = getenv("USER");
  ctx.username = ctx.username ? ctx.username : "<unknown>";
  ctx.serverAddress = serverAddress;
  ctx.serverPort = serverPort;

  printf("CLI on %s:%d for user %s and jobname %s\n", ctx.serverAddress, ctx.serverPort, ctx.username, ctx.jobname);

  // Create server socket
  int tlsFlags = 0;
  Socket *serverSocket = tcpServer2(serverInetAddress, serverPort, tlsFlags, &returnCode, &reasonCode);

  if (serverSocket == NULL) {
    status = DETECT_ATTLS_PORT_STATUS_ERROR;
    handleBindError(returnCode, &ctx);
    return status;
  }

  // Test connection and query ATTLS
  SocketAddress *serverSocketAddress = NULL;
  status = testConnectionAndQueryAttls(serverSocket, serverInetAddress, serverPort,
                                       &ctx, direction,
                                       &serverSocketAddress);

  // Cleanup
  if (serverSocketAddress) {
    freeSocketAddr(serverSocketAddress);
  }
  socketClose(serverSocket, &returnCode, &reasonCode);
  socketFree(serverSocket);

  return status;
}

