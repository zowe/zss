

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
#else
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#endif

#include "zowetypes.h"
#include "utils.h"
#include "zssLogging.h"
#include "logging.h"
#include "rasService.h"

static bool isLoggingComponentValid(char *componentText) {

  int length = strlen(componentText);
  if (length != 18) {
    return FALSE;
  }

  if (memcmp(componentText, "0x", 2)) {
    return FALSE;
  }

  for (int i = 2; i < length; i++) {
    char c = componentText[i];
    if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))) {
      return FALSE;
    }
  }

  return TRUE;
}

static bool isClientTraceLevelValid(int level) {

  if (level < CLIENT_TRACE_LEVEL_SEVERE || level > CLIENT_TRACE_LEVEL_FINEST) {
    return FALSE;
  }

  return TRUE;
}

static int translateClientTraceLevelToLogLevel(int traceLevel) {

  int logLevel = ZOWE_LOG_NA;
  switch (traceLevel) {
  case CLIENT_TRACE_LEVEL_SEVERE:
    logLevel = ZOWE_LOG_SEVERE;
    break;
  case CLIENT_TRACE_LEVEL_WARNING:
    logLevel = ZOWE_LOG_WARNING;
    break;
  case CLIENT_TRACE_LEVEL_INFO:
    logLevel = ZOWE_LOG_INFO;
    break;
  case CLIENT_TRACE_LEVEL_FINE:
    logLevel = ZOWE_LOG_DEBUG;
    break;
  case CLIENT_TRACE_LEVEL_FINER:
    logLevel = ZOWE_LOG_DEBUG2;
    break;
  case CLIENT_TRACE_LEVEL_FINEST:
    logLevel = ZOWE_LOG_DEBUG3;
    break;
  }

  return logLevel;
}

static int translateLogLevelToClientTraceLevel(int logLevel) {

  int traceLevel = CLIENT_TRACE_LEVEL_NA;
  switch (logLevel) {
  case ZOWE_LOG_SEVERE:
    traceLevel = CLIENT_TRACE_LEVEL_SEVERE;
    break;
  case ZOWE_LOG_WARNING:
    traceLevel = CLIENT_TRACE_LEVEL_WARNING;
    break;
  case ZOWE_LOG_INFO:
    traceLevel = CLIENT_TRACE_LEVEL_INFO;
    break;
  case ZOWE_LOG_DEBUG:
    traceLevel = CLIENT_TRACE_LEVEL_FINE;
    break;
  case ZOWE_LOG_DEBUG2:
    traceLevel = CLIENT_TRACE_LEVEL_FINER;
    break;
  case ZOWE_LOG_DEBUG3:
    traceLevel = CLIENT_TRACE_LEVEL_FINEST;
    break;
  }

  return traceLevel;
}

static int serveRASData(HttpService *service, HttpResponse *response) {
  char *method = response->request->method;
  HttpRequest *request = response->request;

  char *command = stringListPrint(request->parsedFile, 1, 1, "/", 0);
  if (command == NULL) {
    zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_WARNING, "httpserver: error - RAS command not provided\n");
    respondWithError(response, HTTP_STATUS_BAD_REQUEST, "RAS command not provided");
    return 0;
  }

  if (strcmp(command, "traceLevel")) {
    zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_WARNING, "httpserver: error - unsupported RAS command, %s\n", command);
    respondWithError(response, HTTP_STATUS_BAD_REQUEST, "unsupported RAS command");
    return 0;
  }

  HttpRequestParam *componentParam = getCheckedParam(request, "component");
  if (componentParam == NULL) {
    respondWithError(response, HTTP_STATUS_BAD_REQUEST, "component not provided");
    return 0;
  }

  if (!isLoggingComponentValid(componentParam->stringValue)) {
    respondWithError(response, HTTP_STATUS_BAD_REQUEST, "not a valid 4-byte hex component ID");
    return 0;
  }

  uint64 componentID = 0;
  int sscanfRC = sscanf(componentParam->stringValue, "%llX", &componentID);
  if (sscanfRC != 1) {
    respondWithError(response, HTTP_STATUS_BAD_REQUEST, "component ID parsing error");
    return 0;
  }

  zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_INFO, "%s: componentID=0x%016llX\n", __FUNCTION__, componentID);

  if (!strcmp(request->method, methodPUT)) {

    /* TODO: the only way to tell if the component is out of range right now,
     * should be enhanced to tell if it was configured in the first place */
    if (logGetLevel(NULL, componentID) == ZOWE_LOG_NA) {
      respondWithError(response, HTTP_STATUS_BAD_REQUEST, "component ID not configured");
      return 0;
    }

    HttpRequestParam *levelParam = getCheckedParam(request, "level");
    if (levelParam == NULL) {
      respondWithError(response, HTTP_STATUS_BAD_REQUEST, "level not provided");
      return 0;
    }

    if (!isClientTraceLevelValid(levelParam->intValue)) {
      respondWithError(response, HTTP_STATUS_BAD_REQUEST, "trace level out of range");
      return 0;
    }

    int logLevel = translateClientTraceLevelToLogLevel(levelParam->intValue);
    if (logLevel != ZOWE_LOG_NA) {
      logSetLevel(NULL, componentID, logLevel);
    }

    setContentType(response, "text/plain");
    addIntHeader(response, "Content-Length", 0);
    addStringHeader(response, "Server", "jdmfws");
    writeHeader(response);
    finishResponse(response);

  }
  else if (!strcmp(request->method, methodGET)) {

    int logLevel = logGetLevel(NULL, componentID);
    if (logLevel == ZOWE_LOG_NA) {
      respondWithError(response, HTTP_STATUS_BAD_REQUEST, "component ID out of range");
      return 0;
    }

    int traceLevel = translateLogLevelToClientTraceLevel(logLevel);
    /* TODO: Make it equal to: CLIENT_TRACE_LEVEL_NA */
    if (traceLevel == -1) {
      char errorMessage[128];
      snprintf(errorMessage, sizeof(errorMessage), "log level out of range, level = %d, component ID = %d", logLevel, componentID);
      respondWithError(response, HTTP_STATUS_BAD_REQUEST, errorMessage);
      return 0;
    }

    jsonPrinter *p = respondWithJsonPrinter(response);
    setResponseStatus(response, HTTP_STATUS_OK, "OK");
    setDefaultJSONRESTHeaders(response);
    writeHeader(response);

    jsonStart(p);
    {
      jsonAddInt(p, "level", traceLevel);
    }
    jsonEnd(p);

    finishResponse(response);

  }
  else {
    respondWithError(response, HTTP_STATUS_METHOD_NOT_FOUND, "bad method, PUT and GET allowed only");
  }

  return 0;
}

int installRASService(HttpServer *server) {
  zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_DEBUG2, "begin %s\n", __FUNCTION__);
  HttpService *httpService = makeGeneratedService("RAS service", "/ras/**");
  httpService->serviceFunction = serveRASData;
  httpService->runInSubtask = FALSE;
  httpService->authType = SERVICE_AUTH_NATIVE_WITH_SESSION_TOKEN;
  httpService->paramSpecList =
          makeStringParamSpec("component", SERVICE_ARG_OPTIONAL,
                              makeIntParamSpec("level", SERVICE_ARG_OPTIONAL, 0, 0, 0, 0,
                                               NULL));
  registerHttpService(server, httpService);
  zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_DEBUG2, "end %s\n", __FUNCTION__);
  return 0;
}


/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

