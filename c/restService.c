

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

#include "zowetypes.h"
#include "bpxnet.h"
#include "utils.h"
#include "socketmgmt.h"

#include "httpserver.h"
#include "json.h"
#include "logging.h"
#include "zssLogging.h"
#include "restService.h"

#ifdef __ZOWE_OS_ZOS
static int serveStatus(HttpService *service, HttpResponse *response);
static JsonObject* serverConfig = NULL;

int installServerStatusService(HttpServer *server, JsonObject *serverSettings)
{
  HttpService *httpService = makeGeneratedService("REST_Service", "/server/agent/**");
  httpService->authType = SERVICE_AUTH_NATIVE_WITH_SESSION_TOKEN;
  httpService->serviceFunction = &serveStatus;
  httpService->runInSubtask = TRUE;
  httpService->doImpersonation = TRUE;
  registerHttpService(server, httpService);
  serverConfig = serverSettings;
  return 0;
}

void respondWithServerConfig(HttpResponse *response){
  jsonPrinter *out = respondWithJsonPrinter(response);
  setResponseStatus(response, 200, "OK");
  setDefaultJSONRESTHeaders(response);
  writeHeader(response);
  jsonStart(out);
  jsonPrintObject(out, serverConfig);
  jsonEnd(out);
  finishResponse(response);
}

#ifndef MAX_LOG_LINE_LENGTH
#define MAX_LOG_LINE_LENGTH 1024
#endif
void respondWithServerLog(HttpResponse *response){
  jsonPrinter *out = respondWithJsonPrinter(response);
  FILE *log;
  log = fopen(getenv("ZSS_LOG_FILE"), "rb");
  char buffer[MAX_LOG_LINE_LENGTH];
  setResponseStatus(response, 200, "OK");
  setDefaultJSONRESTHeaders(response);
  writeHeader(response);
  jsonStart(out);
  jsonStartArray(out, NULL);
  if(log){
    while(fgets(buffer, sizeof(buffer), log)) {
      if(strlen(buffer) > 0){
        buffer[strlen(buffer)-1] = 0;
      }
      jsonAddString(out, NULL, buffer);
    }
    fclose(log);
  }
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
  jsonPrinter *out = respondWithJsonPrinter(response);
  setResponseStatus(response, 200, "OK");
  setDefaultJSONRESTHeaders(response);
  writeHeader(response);
  jsonStart(out);
  jsonAddString(out, "server args", "args here");
  jsonAddString(out, "platform", "platform here");
  jsonAddString(out, "OS", "blah");
  jsonAddString(out, "arch", "blah");
  jsonAddString(out, "cpus", "blah");
  jsonAddString(out, "total memory", "blah");
  jsonAddString(out, "PID", "blah");
  jsonEnd(out);
  finishResponse(response);
}

static int serveStatus(HttpService *service, HttpResponse *response) {
  HttpRequest *request = response->request;
  if (!strcmp(request->method, methodGET)) {
    char *l1 = stringListPrint(request->parsedFile, 2, 1, "/", 0);
    if (!strcmp(l1, "config")){
      respondWithServerConfig(response);
    }
    else if (!strcmp(l1, "log")) {
      respondWithServerLog(response);
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

