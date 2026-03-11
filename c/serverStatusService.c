

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
#include <errno.h>
#endif
#include <sys/utsname.h>

#include "zowetypes.h"
#include "bpxnet.h"
#include "utils.h"
#include "socketmgmt.h"
#include "timeutls.h"
#include "time.h"
#include "json.h"
#include "httpserver.h"
#include "logging.h"
#include "zssLogging.h"
#include "configmgr.h"
#include "serverStatusService.h"
#include "zss.h"

#ifdef __ZOWE_OS_ZOS

#include "crossmemory.h"
#include "jwk.h"

static int serveStatus(HttpService *service, HttpResponse *response);
static int serveStatusPage(HttpService *service, HttpResponse *response);

static inline bool strne(const char *a, const char *b) { 
  return a != NULL && b != NULL && strcmp(a, b) != 0;
}

void installServerStatusService(HttpServer *server, char* productVer) {
  HttpService *httpService = makeGeneratedService("Server_Status_Service", "/server/agent/**");
  httpService->authType = SERVICE_AUTH_NATIVE_WITH_SESSION_TOKEN;
  httpService->authFlags |= SERVICE_AUTH_FLAG_OPTIONAL;
  httpService->serviceFunction = &serveStatus;
  httpService->runInSubtask = TRUE;
  httpService->doImpersonation = TRUE;
  ServerAgentContext *context = (ServerAgentContext*)safeMalloc(sizeof(ServerAgentContext), "ServerAgentContext");
  if (context != NULL) {
    context->productVersion[sizeof(context->productVersion) - 1] = '\0';
    strncpy(context->productVersion, productVer, sizeof(context->productVersion) - 1);
  }
  httpService->userPointer = context;
  registerHttpService(server, httpService);
}

static int respondWithServerConfig(HttpResponse *response, JsonObject* config) {
  jsonPrinter *out = respondWithJsonPrinter(response);
  setResponseStatus(response, 200, "OK");
  setDefaultJSONRESTHeaders(response);
  writeHeader(response);
  jsonStart(out);
  jsonStartObject(out, "options");
  jsonPrintObject(out, config);
  jsonEndObject(out);
  jsonEnd(out);
  finishResponse(response);
  return 0;
}

static int respondWithServerRoutes(HttpResponse *response, bool allowFullAccess) {
  jsonPrinter *out = respondWithJsonPrinter(response);
  setResponseStatus(response, 200, "OK");
  setDefaultJSONRESTHeaders(response);
  writeHeader(response);
  jsonStart(out);
  jsonStartArray(out, "links");
  if (allowFullAccess) {
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
  }
  jsonStartObject(out, NULL);
  jsonAddString(out, "href", "/server/agent/environment");
  jsonAddString(out, "rel", "environment");
  jsonAddString(out, "type", "GET");
  jsonEndObject(out);
  jsonStartObject(out, NULL);
  jsonAddString(out, "href", "/server/agent/services");
  jsonAddString(out, "rel", "services");
  jsonAddString(out, "type", "GET");
  jsonEndObject(out);
  jsonEndArray(out);
  jsonEnd(out);
  finishResponse(response);
  return 0;
}

static int respondWithLogLevels(HttpResponse *response, ServerAgentContext *context, ConfigManager *configmgr){
  jsonPrinter *out = respondWithJsonPrinter(response);
  Json *logLevelsJson = NULL;
  int cfgGetStatus = cfgGetAnyC(configmgr,ZSS_CFGNAME,&logLevelsJson,3,"components","zss","logLevel");
  setResponseStatus(response, 200, "OK");
  setDefaultJSONRESTHeaders(response);
  writeHeader(response);
  jsonStart(out);
  if (cfgGetStatus == ZCFG_SUCCESS) {
    jsonPrintObject(out, jsonAsObject(logLevelsJson));
  }
  jsonEnd(out);
  finishResponse(response);
  return 0;
}

#ifndef HOST_NAME_MAX
#define HOST_NAME_MAX 256
#endif
static int respondWithServerEnvironment(HttpResponse *response, ServerAgentContext *context, 
					ConfigManager *configmgr, bool allowFullAccess) {
  /*Information about parameters for smf_unc: https://www.ibm.com/support/knowledgecenter/SSLTBW_2.1.0/com.ibm.zos.v2r1.erbb700/smfp.htm#smfp*/
  extern char **environ;
  struct utsname unameRet;
  char hostnameBuffer[HOST_NAME_MAX];
  if (__osname(&unameRet) < 0) {
    zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_DEBUG,
            "Falied to retrieve operating system info, errno=%d - %s",
            errno, strerror(errno));
    respondWithError(response, HTTP_STATUS_INTERNAL_SERVER_ERROR, "Falied to retrieve operating system info");
    return -1;
  }
  gethostname(hostnameBuffer, sizeof(hostnameBuffer));
  time_t ltime;
  char tstamp[50];
  time(&ltime);
  jsonPrinter *out = respondWithJsonPrinter(response);
  setResponseStatus(response, 200, "OK");
  setDefaultJSONRESTHeaders(response);
  writeHeader(response);
  jsonStart(out);
  if (allowFullAccess) {
    if (ctime_r(&ltime, tstamp) != NULL) {
      if (tstamp[strlen(tstamp) - 1] != '\0') {
        tstamp[strlen(tstamp) - 1] = '\0';
      }
      jsonAddString(out, "timestamp", tstamp);
    }
    char *logDirectory;
    int logDirStatus = cfgGetStringC(configmgr,ZSS_CFGNAME,&logDirectory,2,"zowe","logDirectory");
    if (logDirStatus == ZCFG_SUCCESS){
      jsonAddString(out, "logDirectory", logDirectory);
    } else {
      jsonAddString(out, "logDirectory", "ZWES_LOG_FILE not defined in environment variables");
    }
  }
  jsonAddString(out, "agentName", "zss");
  jsonAddString(out, "agentVersion", context->productVersion);
  jsonAddString(out, "arch", "s390x");
  jsonAddString(out, "os", "zos");
  jsonAddString(out, "osRelease", unameRet.release);
  jsonAddString(out, "osVersion", unameRet.version);
  jsonAddString(out, "hardwareIdentifier", unameRet.machine);
  if (allowFullAccess) {
    jsonAddString(out, "hostname", hostnameBuffer);
    jsonAddString(out, "nodename", unameRet.nodename);
    jsonStartObject(out, "userEnvironment");
    char *envVar = *environ;
    for (int i = 1; envVar; i++) {
      char *equalSign = strchr(envVar, '=');
      if (equalSign == NULL) {
        break;
      }
      int nameLen = strlen(envVar) - strlen(equalSign);
      int nameBufferLen = nameLen + 1;
      char *name = safeMalloc(nameBufferLen, "env_var name");
      if (name == NULL) {
        break;
      }
      memcpy(name, envVar, nameLen);
      name[nameLen] = '\0';
      jsonAddString(out, name, equalSign+1);
      safeFree(name, nameBufferLen);
      envVar = *(environ+i);
    }
    jsonEndObject(out);
    jsonAddInt(out, "PID", getpid());
    jsonAddInt(out, "PPID", getppid());
  }
  jsonEnd(out);
  finishResponse(response);
  return 0;
}

static int respondWithServices(HttpResponse *response, HttpServer *server) {
  jsonPrinter *out = respondWithJsonPrinter(response);
  HttpService *service = server->config->serviceList;
  setResponseStatus(response, 200, "OK");
  setDefaultJSONRESTHeaders(response);
  writeHeader(response);
  jsonStart(out);
  jsonStartArray(out, "services");
  while (service) {
    jsonStartObject(out, NULL);
    jsonAddString(out, "name", service->name);
    jsonAddString(out, "urlMask", service->urlMask);
    jsonAddString(out, "type", service->serviceType == SERVICE_TYPE_WEB_SOCKET ? "WebSocket" : "REST");
    jsonEndObject(out);
    service = service->next;
  }
  jsonEndArray(out);
  jsonEnd(out);
  finishResponse(response);
  return 0;
}

static bool statusEndPointRequireAuthAndRBAC(const char *endpoint) {
  return !strcmp(endpoint, "config") ||
         !strcmp(endpoint, "log") ||
         !strcmp(endpoint, "logLevels");
}

static int serveStatus(HttpService *service, HttpResponse *response) {
  HttpRequest *request = response->request;
  HttpServer *server = httpResponseServer(response);
  ConfigManager *configmgr = httpServerConfigManager(server);

  ServerAgentContext *context = service->userPointer;
  //This service is conditional on RBAC being enabled because it is a 
  //sensitive URL that only RBAC authorized users should be able to get full access
  Json *dataserviceAuthJson = NULL;
  int cfgGetStatus = cfgGetAnyC(configmgr,ZSS_CFGNAME,&dataserviceAuthJson,3,"components", "app-server", "dataserviceAuthentication");
  JsonObject *dataserviceAuth = (cfgGetStatus == ZCFG_SUCCESS ? jsonAsObject(dataserviceAuthJson) : NULL);
  int rbacParm = dataserviceAuth ? jsonObjectGetBoolean(dataserviceAuth, "rbac") : 0;
  int isAuthenticated = response->request->authenticated;
  bool allowFullAccess = isAuthenticated && rbacParm;
  if (!strcmp(request->method, methodGET)) {
    char *l1 = stringListPrint(request->parsedFile, 2, 1, "/", 0);
    if (!allowFullAccess && statusEndPointRequireAuthAndRBAC(l1)) {
      if (!isAuthenticated) {
        respondWithError(response, HTTP_STATUS_UNAUTHORIZED, "Not Authorized");
        return -1;
      }
      if (!rbacParm) {
        respondWithError(response, HTTP_STATUS_BAD_REQUEST, "Set dataserviceAuthentication.rbac to true in server configuration");
        return -1;
      }
    }
    if (!strcmp(l1, "")) {
      return respondWithServerRoutes(response, allowFullAccess);
    } else if (!strcmp(l1, "config")) {
      Json *config = cfgGetConfigData(configmgr,ZSS_CFGNAME);
      return respondWithServerConfig(response, (config ? jsonAsObject(config) : NULL));
    } else if (!strcmp(l1, "log")) {
      char *logDir;
      int logDirStatus = cfgGetStringC(configmgr,ZSS_CFGNAME,&logDir,2,"zowe","logDirectory");
      if (logDirStatus == ZCFG_SUCCESS){
        respondWithUnixFile2(NULL, response, logDir, 0, 0, false);
        return 0;
      } else {
         respondWithError(response, HTTP_STATUS_NOT_FOUND, "Log not found");
         return -1;
      }
    } else if (!strcmp(l1, "logLevels")) {
      return respondWithLogLevels(response, context, configmgr);
    } else if (!strcmp(l1, "environment")) {
      return respondWithServerEnvironment(response, context, configmgr, allowFullAccess);
    } else if (!strcmp(l1, "services")) {
      return respondWithServices(response, server);
    } else {
      respondWithJsonError(response, "Invalid path", 400, "Bad Request");
      return -1;
    }
  } else {
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
    return -1;
  }
  return 0;
}

/* --- /status HTML page --------------------------------------------------- */

static void writeStatusItem(ChunkedOutputStream *stream,
                            const char *color,
                            const char *label,
                            const char *value) {
  char buf[512];
  snprintf(buf, sizeof(buf),
           "      <li>"
           "<div class=\"dot %s\"></div>"
           "<span class=\"label\">%s</span>"
           "<span class=\"value\">%s</span>"
           "</li>\n",
           color, label, value);
  writeString(stream, buf);
}

static int serveStatusPage(HttpService *service, HttpResponse *response) {
  HttpRequest *request = response->request;
  HttpServer *server = httpResponseServer(response);
  ConfigManager *configmgr = httpServerConfigManager(server);
  ServerAgentContext *context = (ServerAgentContext *)service->userPointer;

  if (strcmp(request->method, methodGET) != 0) {
    setResponseStatus(response, 405, "Method Not Allowed");
    addStringHeader(response, "Allow", "GET");
    writeHeader(response);
    finishResponse(response);
    return -1;
  }

  /* APIML enabled */
  bool apimlEnabled = false;
  cfgGetBooleanC(configmgr, ZSS_CFGNAME, &apimlEnabled, 5,
                 "components", "zss", "agent", "mediationLayer", "enabled");
  const char *apimlColor = apimlEnabled ? "green" : "red";
  const char *apimlVal   = apimlEnabled ? "Yes"   : "No";

  /* JWT process complete */
  bool jwtReady = jwkIsJwtReady(server);
  const char *jwtColor, *jwtVal;
  if (!apimlEnabled) {
    jwtColor = "yellow";
    jwtVal   = "N/A";
  } else if (jwtReady) {
    jwtColor = "green";
    jwtVal   = "Complete";
  } else {
    jwtColor = "yellow";
    jwtVal   = "Pending";
  }

  /* Plugin count */
  char pluginCountBuf[32];
  snprintf(pluginCountBuf, sizeof(pluginCountBuf), "%d", context->pluginCount);

  /* ZIS connection */
  CrossMemoryServerName *zisName =
      getConfiguredProperty(server, HTTP_SERVER_PRIVILEGED_SERVER_PROPERTY);
  CrossMemoryServerStatus zisStatus = {0};
  bool zisOk = false;
  if (zisName != NULL) {
    zisStatus = cmsGetStatus(zisName);
    zisOk = (zisStatus.cmsRC == RC_CMS_OK);
  }
  const char *zisColor = zisOk ? "green" : "red";
  const char *zisVal   = zisOk ? "Connected" : "Unavailable";

  /* Build HTML response */
  ChunkedOutputStream *stream = respondWithChunkedOutputStream(response);
  setResponseStatus(response, 200, "OK");
  setContentType(response, "text/html");
  addStringHeader(response, "Server", "jdmfws");
  addStringHeader(response, "Transfer-Encoding", "chunked");
  writeHeader(response);

  writeString(stream,
    "<!DOCTYPE html>\n"
    "<html lang=\"en\">\n"
    "<head>\n"
    "  <meta charset=\"UTF-8\">\n"
    "  <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\n"
    "  <title>ZSS Status</title>\n"
    "  <style>\n"
    "    * { box-sizing: border-box; margin: 0; padding: 0; }\n"
    "    body {\n"
    "      font-family: Arial, Helvetica, sans-serif;\n"
    "      background: #f0f2f5;\n"
    "      min-height: 100vh;\n"
    "      display: flex;\n"
    "      align-items: center;\n"
    "      justify-content: center;\n"
    "    }\n"
    "    .card {\n"
    "      background: #ffffff;\n"
    "      border-radius: 10px;\n"
    "      box-shadow: 0 4px 20px rgba(0,0,0,0.10);\n"
    "      padding: 40px 56px;\n"
    "      min-width: 380px;\n"
    "    }\n"
    "    h1 {\n"
    "      text-align: center;\n"
    "      font-size: 1.5em;\n"
    "      color: #1a202c;\n"
    "      margin-bottom: 28px;\n"
    "    }\n"
    "    ul { list-style: none; }\n"
    "    li {\n"
    "      display: flex;\n"
    "      align-items: center;\n"
    "      padding: 12px 0;\n"
    "      border-bottom: 1px solid #edf2f7;\n"
    "      font-size: 0.95em;\n"
    "    }\n"
    "    li:last-child { border-bottom: none; }\n"
    "    .dot {\n"
    "      width: 14px;\n"
    "      height: 14px;\n"
    "      border-radius: 50%;\n"
    "      margin-right: 14px;\n"
    "      flex-shrink: 0;\n"
    "    }\n"
    "    .green  { background: #38a169; }\n"
    "    .yellow { background: #d69e2e; }\n"
    "    .red    { background: #e53e3e; }\n"
    "    .label { flex: 1; color: #4a5568; }\n"
    "    .value { font-weight: 600; color: #2d3748; }\n"
    "  </style>\n"
    "</head>\n"
    "<body>\n"
    "  <div class=\"card\">\n"
    "    <h1>ZSS Status</h1>\n"
    "    <ul>\n");

  writeStatusItem(stream, apimlColor, "APIML Enabled",  apimlVal);
  writeStatusItem(stream, jwtColor,   "JWT Process",    jwtVal);
  writeStatusItem(stream, "green",    "Plugins Loaded", pluginCountBuf);
  writeStatusItem(stream, "green",    "ZSS Version",    context->productVersion);
  writeStatusItem(stream, zisColor,   "ZIS Connection", zisVal);

  writeString(stream,
    "    </ul>\n"
    "  </div>\n"
    "</body>\n"
    "</html>\n");

  finishResponse(response);
  return 0;
}

void installStatusPageService(HttpServer *server, char *productVer, int pluginCount) {
  HttpService *httpService = makeGeneratedService("ZSS_Status_Page", "/status");
  httpService->authType = SERVICE_AUTH_NONE;
  httpService->serviceFunction = serveStatusPage;
  httpService->runInSubtask = TRUE;
  httpService->doImpersonation = FALSE;
  ServerAgentContext *context =
      (ServerAgentContext *)safeMalloc(sizeof(ServerAgentContext), "ServerAgentContext");
  if (context != NULL) {
    context->productVersion[sizeof(context->productVersion) - 1] = '\0';
    strncpy(context->productVersion, productVer, sizeof(context->productVersion) - 1);
    context->pluginCount = pluginCount;
  }
  httpService->userPointer = context;
  registerHttpService(server, httpService);
}

#endif /* __ZOWE_OS_ZOS */


/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

