

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
#include "httpserver.h"
#include "zssLogging.h"
#include "logging.h"
#include "rasService.h"

#ifdef __ZOWE_OS_ZOS

#include "json.h"
#include "configmgr.h"
#include "zss.h"
#include "zis/client.h"
#include "zos.h"

#define SAF_CLASS                   "ZOWE"
#define RBAC_PROFILE_ID_DEFAULT     "1"
/* ZLUX.<zowe.rbacProfileIdentifier>.COR.<METHOD>.RAS.TRACELEVEL. The format and
   the COR code for core dataservices are zlux-server-framework's; the
   identifier separates Zowe installs, and defaults.yaml ships "1". */
#define RAS_TRACELEVEL_PROFILE_FMT  "ZLUX.%s.COR.%s.RAS.TRACELEVEL"
#define RAS_TRACELEVEL_PROFILE_MAX  64

typedef enum RasAuthDecision_tag {
  RAS_AUTH_PERMITTED = 0,
  RAS_AUTH_NO_IDENTITY,
  RAS_AUTH_RBAC_OFF,
  RAS_AUTH_DENIED,
  RAS_AUTH_UNDETERMINED
} RasAuthDecision;

/*
 * TRAP: request->method is the method as it arrived on the wire, which is ASCII.
 * httpserver.c keeps the raw bytes (c/httpserver.c:2021) and converts a separate
 * copy for its own use, so comparing against methodGET works while printing or
 * concatenating it does not. Splicing it into the profile name produced a string
 * that was part EBCDIC and part ASCII, which no SAF profile can ever match: the
 * check then failed with SAF RC 4 and looked exactly like an undefined profile.
 * So the method part of the name is a literal chosen by comparison.
 */
static int traceLevelProfile(HttpServer *server, const char *method,
                             char *buf, size_t bufSize) {
  const char *methodPart;
  if (!strcmp(method, methodGET)) {
    methodPart = "GET";
  } else if (!strcmp(method, methodPUT)) {
    methodPart = "PUT";
  } else {
    return -1;
  }

  char *configured = NULL;
  int status = cfgGetStringC(httpServerConfigManager(server), ZSS_CFGNAME,
                             &configured, 2, "zowe", "rbacProfileIdentifier");
  const char *id = (status == ZCFG_SUCCESS && configured != NULL && configured[0] != '\0')
                   ? configured : RBAC_PROFILE_ID_DEFAULT;
  int len = snprintf(buf, bufSize, RAS_TRACELEVEL_PROFILE_FMT, id, methodPart);
  return (len > 0 && (size_t)len < bufSize) ? 0 : -1;
}

/* Decides only; serveRASData maps the decision to a status code. */
static RasAuthDecision checkTraceLevelAuthorization(HttpResponse *response,
                                                    const char *method) {
  HttpRequest *request = response->request;
  HttpServer *server = httpResponseServer(response);

  /* The authType already refuses an anonymous caller before we run; this keeps
     that true if SERVICE_AUTH_FLAG_OPTIONAL is ever added. */
  if (!request->authenticated) {
    return RAS_AUTH_NO_IDENTITY;
  }

  Json *dataserviceAuthJson = NULL;
  int cfgGetStatus = cfgGetAnyC(httpServerConfigManager(server), ZSS_CFGNAME,
                                &dataserviceAuthJson, 3,
                                "components", "app-server", "dataserviceAuthentication");
  JsonObject *dataserviceAuth =
      (cfgGetStatus == ZCFG_SUCCESS ? jsonAsObject(dataserviceAuthJson) : NULL);
  if (!(dataserviceAuth ? jsonObjectGetBoolean(dataserviceAuth, "rbac") : 0)) {
    return RAS_AUTH_RBAC_OFF;
  }

  char profile[RAS_TRACELEVEL_PROFILE_MAX];
  if (traceLevelProfile(server, method, profile, sizeof(profile)) != 0) {
    zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_WARNING,
            "httpserver: RAS traceLevel profile exceeds %d bytes, check zowe.rbacProfileIdentifier\n",
            (int)sizeof(profile));
    return RAS_AUTH_UNDETERMINED;
  }

  CrossMemoryServerName *privilegedServerName =
      getConfiguredProperty(server, HTTP_SERVER_PRIVILEGED_SERVER_PROPERTY);
  ZISAuthServiceStatus reqStatus = {0};
  int rc = zisCheckEntity(privilegedServerName, request->username, SAF_CLASS,
                          profile, SAF_AUTH_ATTR_READ, &reqStatus);
  if (rc == RC_ZIS_SRVC_OK) {
    return RAS_AUTH_PERMITTED;
  }

  /* SAF_ERROR means SAF ran and did not permit: SAF RC 8 is denied, 4 is the
     resource not being protected. Anything else means we could not ask, and
     then safStatus must not be read: the status union may hold abend info. */
  if (rc == RC_ZIS_SRVC_SERVICE_FAILED &&
      reqStatus.baseStatus.serviceRC == RC_ZIS_AUTHSRV_SAF_ERROR) {
    zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_WARNING,
            "httpserver: RAS traceLevel denied for %s, profile '%s', "
            "SAF RC %d, RACF RC %d, RACF reason %d\n",
            request->username, profile, reqStatus.safStatus.safRC,
            reqStatus.safStatus.racfRC, reqStatus.safStatus.racfRSN);
    return RAS_AUTH_DENIED;
  }

  zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_WARNING,
          "httpserver: RAS traceLevel authorization undetermined, profile '%s', "
          "zisCheckEntity RC %d, ZIS service RC %d\n",
          profile, rc, reqStatus.baseStatus.serviceRC);
  return RAS_AUTH_UNDETERMINED;
}

#endif /* __ZOWE_OS_ZOS */

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

  /* Before the gate and before any parameter is read: an unsupported method is
     refused here so that nothing downstream runs for a request we will not
     serve, and so the gate never has to pick a profile for a method it has
     none for. */
  if (strcmp(method, methodGET) && strcmp(method, methodPUT)) {
    respondWithError(response, HTTP_STATUS_METHOD_NOT_FOUND, "bad method, PUT and GET allowed only");
    return 0;
  }

#ifdef __ZOWE_OS_ZOS
  switch (checkTraceLevelAuthorization(response, method)) {
  case RAS_AUTH_PERMITTED:
    break;
  case RAS_AUTH_NO_IDENTITY:
    respondWithError(response, HTTP_STATUS_UNAUTHORIZED, "Not Authorized");
    return 0;
  case RAS_AUTH_RBAC_OFF:
    respondWithError(response, HTTP_STATUS_BAD_REQUEST,
                     "Set dataserviceAuthentication.rbac to true in server configuration");
    return 0;
  case RAS_AUTH_DENIED:
    respondWithError(response, HTTP_STATUS_FORBIDDEN,
                     "Forbidden - insufficient RBAC authorization");
    return 0;
  default:
    respondWithError(response, HTTP_STATUS_INTERNAL_SERVER_ERROR,
                     "Could not determine RBAC authorization");
    return 0;
  }
#endif

  uint64 componentID = 0;

  HttpRequestParam *componentNameParam = getCheckedParam(request, "componentName");
  HttpRequestParam *componentIDParam = getCheckedParam(request, "componentID");

  if (componentIDParam == NULL && componentNameParam == NULL) {
    respondWithError(response, HTTP_STATUS_BAD_REQUEST, "component not provided");
    return 0;
  }

  if (componentIDParam != NULL && componentNameParam != NULL) {
    respondWithError(response, HTTP_STATUS_BAD_REQUEST, "too many arguments provided");
    return 0;
  }

  if (componentIDParam != NULL) {
    if (!isLoggingComponentValid(componentIDParam->stringValue)) {
      respondWithError(response, HTTP_STATUS_BAD_REQUEST, "not a valid 4-byte hex component ID");
      return 0;
    }

    int sscanfRC = sscanf(componentIDParam->stringValue, "%llX", &componentID);
    if (sscanfRC != 1) {
      respondWithError(response, HTTP_STATUS_BAD_REQUEST, "component ID parsing error");
      return 0;
    }
  }

  if (componentNameParam != NULL) {
    /* htGet misses for any name no plugin data service registered, which on a
       ZSS with no plugins is every name. */
    const uint64 *loggingID = htGet(service->server->loggingIdsByName, componentNameParam->stringValue);
    if (loggingID == NULL) {
      respondWithError(response, HTTP_STATUS_BAD_REQUEST, "unknown componentName");
      return 0;
    }
    componentID = *loggingID;
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

    setResponseStatus(response, HTTP_STATUS_OK, "OK");
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
          makeStringParamSpec("componentID", SERVICE_ARG_OPTIONAL,
                              makeIntParamSpec("level", SERVICE_ARG_OPTIONAL, 0, 0, 0, 0,
                                              makeStringParamSpec("componentName", SERVICE_ARG_OPTIONAL,
                                                                  NULL)));
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

