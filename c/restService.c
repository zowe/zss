

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
#include "metalio.h"
#include "qsam.h"
#else
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#endif
#include <sys/utsname.h>

#include "zowetypes.h"
#include "bpxnet.h"
#include "utils.h"
#include "socketmgmt.h"

#include "httpserver.h"
#include "json.h"
#include "logging.h"
#include "zssLogging.h"
#include "restService.h"
#pragma linkage (EXSMFI,OS)

#ifdef __ZOWE_OS_ZOS
static int serveStatus(HttpService *service, HttpResponse *response);
static JsonObject* serverConfig = NULL;
char productVersion[40];

typedef int EXSMFI();
EXSMFI *smf_func;

int installServerStatusService(HttpServer *server, JsonObject *serverSettings, char* productVer)
{
  HttpService *httpService = makeGeneratedService("REST_Service", "/server/agent/**");
  httpService->authType = SERVICE_AUTH_NATIVE_WITH_SESSION_TOKEN;
  httpService->serviceFunction = &serveStatus;
  httpService->runInSubtask = TRUE;
  httpService->doImpersonation = TRUE;
  registerHttpService(server, httpService);
  serverConfig = serverSettings;
  memcpy(productVersion, productVer, 40);
  return 0;
}

void respondWithServerConfig(HttpResponse *response){
  jsonPrinter *out = respondWithJsonPrinter(response);
  setResponseStatus(response, 200, "OK");
  setDefaultJSONRESTHeaders(response);
  writeHeader(response);
  jsonStart(out);
  jsonStartObject(out, "options");
  jsonPrintObject(out, serverConfig);
  jsonEndObject(out);
  jsonEnd(out);
  finishResponse(response);
}

void respondWithServerRoutes(HttpResponse *response){
  jsonPrinter *out = respondWithJsonPrinter(response);
  setResponseStatus(response, 200, "OK");
  setDefaultJSONRESTHeaders(response);
  writeHeader(response);
  jsonStart(out);
  jsonStartArray(out, "links");
  jsonStartObject(out, NULL);
  jsonAddString(out, "href", "/server/agent/config");
  jsonAddString(out, "rel", "config");
  jsonAddString(out, "type", "GET");
  jsonEndObject(out);
  jsonStartObject(out, NULL);
  jsonAddString(out, "href", "/server/agent/log");
  jsonAddString(out, "rel", "log");
  jsonAddString(out, "type", "GET");
  jsonEndObject(out);
  jsonStartObject(out, NULL);
  jsonAddString(out, "href", "/server/agent/logLevels");
  jsonAddString(out, "rel", "logLevels");
  jsonAddString(out, "type", "GET");
  jsonEndObject(out);
  jsonStartObject(out, NULL);
  jsonAddString(out, "href", "/server/agent/environment");
  jsonAddString(out, "rel", "environment");
  jsonAddString(out, "type", "GET");
  jsonEndObject(out);
  jsonEndArray(out);
  jsonEnd(out);
  finishResponse(response);
}

void respondWithLogLevels(HttpResponse *response){
  jsonPrinter *out = respondWithJsonPrinter(response);
  JsonObject *logLevels = jsonObjectGetObject(serverConfig, "logLevels");
  setResponseStatus(response, 200, "OK");
  setDefaultJSONRESTHeaders(response);
  writeHeader(response);
  jsonStart(out);
  if(logLevels){
    jsonPrintObject(out, logLevels);
  }
  jsonEnd(out);
  finishResponse(response);
}

void respondWithServerEnvironment(HttpResponse *response){
  /*Information about parameters for smc_func: https://www.ibm.com/support/knowledgecenter/SSLTBW_2.1.0/com.ibm.zos.v2r1.erbb700/smfp.htm#smfp*/
  jsonPrinter *out = respondWithJsonPrinter(response);
  struct utsname unameRet;
  uname(&unameRet);
  char pid[64];
  char ppid[64];
  char dp[64];
  char cpu_u[64];
  char mvs_u[64];
  char ziip_u[64];
  char zaap_u[64];
  char* buffer;
  int rc = 0;
  int reqtype = 0x00000005; //fullword. request type
  int rectype = 0x0000004F; //SMF record type, only type 79 is supported
  int subtype = 0x00000009; //SMF record subtype
  int bufferlen = 247488; /* length of SMF record buffer */
  int cpu_util = 0x00000000;
  int demand_paging = 0x00000000;
  int options = 0x00000000;
  int mvs_srm = 0x00000000;
  int zaap_util = 0x00000000;
  int ziip_util = 0x00000000;
  buffer = (char *)safeMalloc(bufferlen, "buffer");
  memset(buffer, 0, bufferlen);
  smf_func = (EXSMFI *)fetch("ERBSMFI");
  rc = (*smf_func)(&reqtype,
               &rectype,
               &subtype,
               buffer,
               &bufferlen,
               &cpu_util,
               &demand_paging,
               &options,
               &mvs_srm,
               &zaap_util,
               &ziip_util);
  sprintf(pid, "%d", getpid());
  sprintf(ppid, "%d", getppid());
  sprintf(dp, "%d", demand_paging);
  sprintf(cpu_u, "%d%", cpu_util);
  sprintf(mvs_u, "%d%", mvs_srm);
  sprintf(zaap_u, "%d%", zaap_util);
  sprintf(ziip_u, "%d%", ziip_util);
  setResponseStatus(response, 200, "OK");
  setDefaultJSONRESTHeaders(response);
  writeHeader(response);
  jsonStart(out);
  jsonAddString(out, "log directory", getenv("ZSS_LOG_FILE"));
  jsonAddString(out, "agent name", "zss");
  jsonAddString(out, "agent version", productVersion);
  jsonAddString(out, "arch", unameRet.sysname);
  jsonAddString(out, "OS release", unameRet.release);
  jsonAddString(out, "hardware identifier", unameRet.machine);
  jsonAddString(out, "hostname", unameRet.nodename);
  jsonAddString(out, "Demand Paging Rate", dp); 
  jsonAddString(out, "Standard CP CPU Utilization", cpu_u);
  jsonAddString(out, "Standard CP MVS/SRM CPU Utilization", mvs_u);
  jsonAddString(out, "ZAAP CPU Utilization", zaap_u);
  jsonAddString(out, "ZIIP CPU Utilization", ziip_u);
  jsonAddString(out, "PID", pid);
  jsonAddString(out, "PPID", ppid);
  jsonEnd(out);
  finishResponse(response);
}

static int serveStatus(HttpService *service, HttpResponse *response) {
  HttpRequest *request = response->request;
  if (!strcmp(request->method, methodGET)) {
    char *l1 = stringListPrint(request->parsedFile, 2, 1, "/", 0);
    if(!strcmp(l1, "")){
      respondWithServerRoutes(response);
    }
    else if (!strcmp(l1, "config")){
      respondWithServerConfig(response);
    }
    else if (!strcmp(l1, "log")) {
      if(strcmp(getenv("ZSS_LOG_FILE"), "")){
        respondWithUnixFile2(NULL, response, getenv("ZSS_LOG_FILE"), 0, 0, false);
      } else {
         respondWithError(response, HTTP_STATUS_NO_CONTENT, "Log not found");
      }
    }
    else if (!strcmp(l1, "logLevels")) {
      respondWithLogLevels(response);
    }
    else if (!strcmp(l1, "environment")) {
      respondWithServerEnvironment(response);
    }
    else {
      respondWithJsonError(response, "Invalid path", 400, "Bad Request");
    }
  }
  else{
    jsonPrinter *out = respondWithJsonPrinter(response);

    setContentType(response, "text/json");
    setResponseStatus(response, 405, "Method Not Allowed");
    addStringHeader(response, "Server", "jdmfws");
    addStringHeader(response, "Transfer-Encoding", "chunked");
    addStringHeader(response, "Allow", "GET");
    writeHeader(response);

    jsonStart(out);
    jsonEnd(out);

    finishResponse(response);
  }

  return 0;
}

#endif /* __ZOWE_OS_ZOS */


/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

