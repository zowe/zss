

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
#include <metal/stdarg.h>  
#include "metalio.h"

#else
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

#endif

#include "zowetypes.h"
#include "alloc.h"
#include "utils.h"
#ifdef __ZOWE_OS_ZOS
#include "zos.h"
#endif
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
#include "httpfileservice.h"
#include "dataservice.h"
#ifdef __ZOWE_OS_ZOS
#include "zosDiscovery.h"
#endif
#include "charsets.h"
#include "plugins.h"
#ifdef __ZOWE_OS_ZOS
#include "datasetjson.h"
#include "authService.h"
#include "securityService.h"
#include "zis/client.h"
#endif

#include "zssLogging.h"
#include "serviceUtils.h"
#include "unixFileService.h"
#include "omvsService.h"
#include "datasetService.h"
#include "serverStatusService.h"

#define PRODUCT "ZLUX"
#ifndef PRODUCT_MAJOR_VERSION
#define PRODUCT_MAJOR_VERSION 0
#endif
#ifndef PRODUCT_MINOR_VERSION
#define PRODUCT_MINOR_VERSION 0
#endif
#ifndef PRODUCT_REVISION
#define PRODUCT_REVISION 0
#endif
#ifndef PRODUCT_VERSION_DATE_STAMP
#define PRODUCT_VERSION_DATE_STAMP 0
#endif
char productVersion[40];

static JsonObject *MVD_SETTINGS = NULL;
static int traceLevel = 0;

#define JSON_ERROR_BUFFER_SIZE 1024

static int stringEndsWith(char *s, char *suffix);
static void dumpJson(Json *json);
static JsonObject *readPluginDefinition(ShortLivedHeap *slh, char *pluginIdentifier, char *pluginLocation);
static WebPluginListElt* readWebPluginDefinitions(HttpServer* server, ShortLivedHeap *slh, char *dirname);
static JsonObject *readServerSettings(ShortLivedHeap *slh, const char *filename);
static InternalAPIMap *makeInternalAPIMap(void);

static int servePluginDefinitions(HttpService *service, HttpResponse *response){
  zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_DEBUG2, "begin %s\n", __FUNCTION__);
  WebPluginListElt *webPluginListElt = (WebPluginListElt*)service->userPointer;
  jsonPrinter *printer = respondWithJsonPrinter(response);
  HttpRequestParam *typeParam = getCheckedParam(response->request, "type");
  int showAllPluginTypes = TRUE;
  char *pluginType = NULL;
  int desiredType = WEB_PLUGIN_TYPE_UNKNOWN;
  if (typeParam) {
    pluginType = typeParam->stringValue;
    if (pluginType) {
      if (0 == strcmp(pluginType, "all")) {
        showAllPluginTypes = TRUE;
      } else {
        showAllPluginTypes = FALSE;
        desiredType = pluginTypeFromString(pluginType);
      }
    }
  }
  setResponseStatus(response, 200, "OK");
  setDefaultJSONRESTHeaders(response);
  writeHeader(response);
  jsonStart(printer);
  {
    jsonStartArray(printer, "pluginDefinitions");
    while (webPluginListElt) {
      WebPlugin *plugin = webPluginListElt->plugin;
      if (showAllPluginTypes || (desiredType ==  plugin->pluginType)) {
        jsonStartObject(printer, NULL);
        jsonPrintObject(printer, plugin->pluginDefinition);
        jsonEndObject(printer);
      }
      webPluginListElt = webPluginListElt->next;
    }
    jsonEndArray(printer);
  }
  jsonEnd(printer);
  finishResponse(response);
  zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_DEBUG2, "end %s\n", __FUNCTION__);
  return 0;
}

static int serveWebContent(HttpService *service, HttpResponse *response){
  zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_DEBUG2, "begin %s\n", __FUNCTION__);
  char *targetDirectory = (char*) service->userPointer;
  HttpRequest *request = response->request;
  char *fileFrag = stringListPrint(request->parsedFile, 4 /* skip PRODUCT/plugins/baseURI/web */, 1000, "/", 0);
  char *filename = stringConcatenate(response->slh, targetDirectory, fileFrag);
  respondWithUnixFileContentsWithAutocvtMode(NULL, response, filename, FALSE, 0);
  zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_DEBUG2, "end %s\n", __FUNCTION__);
  return 0;
}

static int serveLibraryContent(HttpService *service, HttpResponse *response){
  zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_DEBUG2, "begin %s\n", __FUNCTION__);
  char *targetDirectory = (char*) service->userPointer;
  HttpRequest *request = response->request;
  char *fileFrag = stringListPrint(request->parsedFile, 3 /* skip lib/identifier/libraryVersion */, 1000, "/", 0);
  char *filename = stringConcatenate(response->slh, targetDirectory, fileFrag);
  respondWithUnixFileContentsWithAutocvtMode(NULL, response, filename, FALSE, 0);
  zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_DEBUG2, "end %s\n", __FUNCTION__);
  return 0;
}

static int extractAuthorizationFromJson(HttpService *service, HttpRequest *request){
  zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_DEBUG2, "begin %s\n", __FUNCTION__);
  /* should check content type */
  char *inPtr = request->contentBody;
  char *nativeBody = copyStringToNative(request->slh, inPtr, strlen(inPtr));
  int inLen = nativeBody == NULL ? 0 : strlen(nativeBody);
  char errBuf[JSON_ERROR_BUFFER_SIZE];

  Json *body = jsonParseUnterminatedString(request->slh, nativeBody, inLen, errBuf, JSON_ERROR_BUFFER_SIZE);

  if(body != NULL){
    JsonObject *inputMessage = jsonAsObject(body);
    Json *username = jsonObjectGetPropertyValue(inputMessage,"username");
    Json *password = jsonObjectGetPropertyValue(inputMessage,"password");
    if (username == NULL){
      return -1;
    } else if (!jsonIsString(username)){
      return -1;
    } else {
      request->username = jsonAsString(username);
    }
  
    if (password == NULL){
      return -1;
    } else if (!jsonIsString(password)){
      return -1;
    } else {
      request->password = jsonAsString(password);
    }
  }
  return 0;
}

static
int serveMainPage(HttpService *service, HttpResponse *response) {
  zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_DEBUG2, "begin %s\n", __FUNCTION__);

  char mainPage[2048];
  snprintf(mainPage, 2048, "/%s/plugins/com.rs.mvd/web/index.html", PRODUCT);
  setResponseStatus(response, 302, "Found");
  addStringHeader(response, "Location", mainPage);
  addStringHeader(response, "Server", "jdmfws");
  writeHeader(response);
  zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_DEBUG2, "end %s\n", __FUNCTION__);
  return 0;
}

int serveLoginWithSessionToken(HttpService *service, HttpResponse *response) {
  jsonPrinter *p = respondWithJsonPrinter(response);

  zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_INFO, "serveLoginWithSessionToken: start. response=0x%p; cookie=0x%p\n", response, response->sessionCookie);
  setResponseStatus(response,200,"OK");
  setContentType(response,"text/html");
  addStringHeader(response,"Server","jdmfws");
  addStringHeader(response,"Transfer-Encoding","chunked");
  addStringHeader(response, "Cache-control", "no-store");
  addStringHeader(response, "Pragma", "no-cache");
  /* HACK: the cookie header should be set inside serviceAuthNativeWithSessionToken, */
  /* but that is currently broken: the header doesn't get sent if it is set there */
  if (response->sessionCookie){
    addStringHeader(response,"Set-Cookie",response->sessionCookie);
  }
  writeHeader(response);

  jsonStart(p);
  {
    jsonAddString(p, "status", "OK");
  }
  jsonEnd(p);
  
  finishResponse(response);
  zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_INFO, "serveLoginWithSessionToken: end\n");
  return HTTP_SERVICE_SUCCESS;
}

int serveLogoutByRemovingSessionToken(HttpService *service, HttpResponse *response) {
  jsonPrinter *p = respondWithJsonPrinter(response);

  zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_INFO, "serveLogoutByRemovingSessionToken: start. response=0x%p; cookie=0x%p\n", response, response->sessionCookie);
  setResponseStatus(response,200,"OK");
  setContentType(response,"text/html");
  addStringHeader(response,"Server","jdmfws");
  addStringHeader(response,"Transfer-Encoding","chunked");
  addStringHeader(response, "Cache-control", "no-store");
  addStringHeader(response, "Pragma", "no-cache");
  
  /* Remove the session token when the user wants to log out */
  addStringHeader(response,"Set-Cookie","jedHTTPSession=non-token");
  response->sessionCookie = "non-token";
  writeHeader(response);

  jsonStart(p);
  {
    jsonAddString(p, "status", "OK");
  }
  jsonEnd(p);
  
  finishResponse(response);
  zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_INFO, "serveLogoutByRemovingSessionToken: end\n");
  return HTTP_SERVICE_SUCCESS;
}

static
void checkAndSetVariable(JsonObject *mvdSettings,
                         const char* configVariableName,
                         char* target,
                         size_t targetMax);

#ifdef __ZOWE_OS_ZOS
static void setPrivilegedServerName(HttpServer *server, JsonObject *mvdSettings) {

  CrossMemoryServerName *privilegedServerName = (CrossMemoryServerName *)SLHAlloc(server->slh, sizeof(CrossMemoryServerName));
  char priviligedServerNameNullTerm[sizeof(privilegedServerName->nameSpacePadded) + 1];
  memset(priviligedServerNameNullTerm, 0, sizeof(priviligedServerNameNullTerm));

  checkAndSetVariable(mvdSettings, "privilegedServerName", priviligedServerNameNullTerm, sizeof(priviligedServerNameNullTerm));

  if (strlen(priviligedServerNameNullTerm) != 0) {
    *privilegedServerName = cmsMakeServerName(priviligedServerNameNullTerm);
  } else {
    zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_WARNING, "warning: privileged server name not provided, falling back to default\n");
    *privilegedServerName = zisGetDefaultServerName();
  }

  setConfiguredProperty(server, HTTP_SERVER_PRIVILEGED_SERVER_PROPERTY, privilegedServerName);
}
#endif /* __ZOWE_OS_ZOS */

static void loadWebServerConfig(HttpServer *server, JsonObject *mvdSettings){
  MVD_SETTINGS = mvdSettings;
  /* Disabled because this server is not being used by end users, but called by other servers
   * HttpService *mainService = makeGeneratedService("main", "/");
   * mainService->serviceFunction = serveMainPage;
   * mainService->authType = SERVICE_AUTH_NONE;
   */
  server->sharedServiceMem = mvdSettings;
  //registerHttpService(server, mainService);
  registerHttpServiceOfLastResort(server,NULL);
#ifdef __ZOWE_OS_ZOS
  setPrivilegedServerName(server, mvdSettings);
#endif
}

#define DIR_BUFFER_SIZE 4096

static int setMVDTrace(int toWhat) {
  int was = traceLevel;
  if (isLogLevelValid(toWhat)) {
    traceLevel = toWhat;
    logSetLevel(NULL, LOG_COMP_ID_MVD_SERVER, toWhat);
  } else {
    zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_ALWAYS, "error: log level %d is incorrect\n", toWhat);
  }
  return was;
}

typedef int (*TraceSetFunction)(int toWhat);

typedef struct TraceDefinition_tag {
  const char* name;
  TraceSetFunction function;
} TraceDefinition;

static
TraceDefinition traceDefs[] = {
  {"_zss.traceLevel", setMVDTrace},
  {"_zss.fileTrace", setFileTrace},
  {"_zss.socketTrace", setSocketTrace},
  {"_zss.httpParseTrace", setHttpParseTrace},
  {"_zss.httpDispatchTrace", setHttpDispatchTrace},
  {"_zss.httpHeadersTrace", setHttpHeadersTrace},
  {"_zss.httpSocketTrace", setHttpSocketTrace},
  {"_zss.httpCloseConversationTrace", setHttpCloseConversationTrace},
  {"_zss.httpAuthTrace", setHttpAuthTrace},
#ifdef __ZOWE_OS_LINUX
  /* TODO: move this somewhere else... no impact for z/OS Zowe currently. */
  {"DefaultCCSID", setFileInfoCCSID}, /* not a trace setting */
#endif

  {0,0}
};

static JsonObject *readServerSettings(ShortLivedHeap *slh, const char *filename) {

  char jsonErrorBuffer[512] = { 0 };
  int jsonErrorBufferSize = sizeof(jsonErrorBuffer);
  Json *mvdSettings = NULL; 
  JsonObject *mvdSettingsJsonObject = NULL;
  zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_INFO, "reading server settings from %s\n", filename);
  mvdSettings = jsonParseFile(slh, filename, jsonErrorBuffer, jsonErrorBufferSize);
  if (mvdSettings) {
    if (jsonIsObject(mvdSettings)) {
      mvdSettingsJsonObject = jsonAsObject(mvdSettings);
    } else {
      zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_WARNING, "error in file %s: expected top level object\n", filename);
    }
    JsonObject *logLevels = jsonObjectGetObject(mvdSettingsJsonObject, "logLevels");
    if (logLevels) {
      TraceDefinition *traceDef = traceDefs;
      while (traceDef->name != 0) {
	int traceLevel = jsonObjectGetNumber(logLevels, (char*) traceDef->name);
	if (traceLevel > 0) {
	  traceDef->function(traceLevel);
	}
	++traceDef;
      }
    }
    dumpJson(mvdSettings);
  } else {
    zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_WARNING, "error while parsing ZSS settings from file %s: %s\n", filename, jsonErrorBuffer);
  }
  return mvdSettingsJsonObject;
}

static int stringEndsWith(char *s, char *suffix) {
  int suffixLen = strlen(suffix);
  int len = strlen(s);
  int success = FALSE;
  if (len >= suffixLen) {
    if (0 == strncmp(s + len - suffixLen, suffix, suffixLen)) {
      success = TRUE;
    }
  }
  return success;
}

static void writeJsonMethod(struct jsonPrinter_tag *unusedPrinter, char *text, int len) {
  zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_DEBUG, "%.*s", len, text);
}

static void dumpJson(Json *json) {
  jsonPrinter *printer = makeCustomJsonPrinter(writeJsonMethod, NULL);
  jsonEnablePrettyPrint(printer);
  jsonPrint(printer, json);
  freeJsonPrinter(printer);
  zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_DEBUG, "\n");
}

typedef struct {
  const char* name;
  ExternalAPI * entryPoint;
} MapItem;

static MapItem mapItems[] ={
#ifdef __ZOWE_OS_ZOS
  {"zosDiscoveryServiceInstaller", zosDiscoveryServiceInstaller},
#endif
  {0,0}
};

static InternalAPIMap *makeInternalAPIMap(void) {
  /* allocate a single chunk for the struct and all the pointers. */
  const int entryCount = ((sizeof mapItems)/(sizeof(mapItems[0])))-1;
  const int pointerArraySize = entryCount*(sizeof (void*));
  const int structSize = sizeof (struct InternalAPIMap_tag);
  const int totalMapSize =
    structSize +               /* size of the struct itself */ 
    2*pointerArraySize;        /* two arrays of pointers */
  char* mapBase = (char*) safeMalloc(totalMapSize, "InternalAPIMap");
  memset(mapBase, 0, totalMapSize);

  InternalAPIMap *map = (InternalAPIMap*) mapBase;
  map->length = entryCount;
  map->names = (char**) (mapBase+structSize);
  map->entryPoints = (ExternalAPI**) (mapBase+structSize+pointerArraySize);
  MapItem* item = mapItems;
  for (int i = 0; i < entryCount; ++i){
    map->names[i] = (char*) item->name;
    map->entryPoints[i] = item->entryPoint;
    ++item;
  }
  return map;
}

static JsonObject *readPluginDefinition(ShortLivedHeap *slh, char *pluginIdentifier, char *pluginLocation) {
  JsonObject *pluginDefinition = NULL;
  char path[1024];
  char errorBuffer[512];
  int pluginLocationLen = strlen(pluginLocation);
  int needsSlash = (pluginLocation[pluginLocationLen - 1] != '/');
  
  zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_DEBUG2, "%s begin identifier %s location %s\n", __FUNCTION__, pluginIdentifier, pluginLocation);
  sprintf(path, "%s%s%s", pluginLocation, needsSlash ? "/" : "", "pluginDefinition.json");
  Json *pluginDefinitionJson = jsonParseFile(slh, path, errorBuffer, sizeof (errorBuffer));
  if (pluginDefinitionJson) {
    dumpJson(pluginDefinitionJson);
    JsonObject *pluginDefinitionJsonObject = jsonAsObject(pluginDefinitionJson);
    if (pluginDefinitionJsonObject) {
      char *identifier = jsonObjectGetString(pluginDefinitionJsonObject, "identifier");
      if (identifier) {
        if (0 == strcmp(pluginIdentifier, identifier)) {
          pluginDefinition = pluginDefinitionJsonObject;
        } else {
          zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_INFO, "expected plugin identifier %s, got %s\n", pluginIdentifier, identifier);
        }
      } else {
        zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_INFO, "plugin identifier was not found in %s\n", path);
      }
    } else {
      zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_INFO, "error in file %s: expected top level object\n", path);
    }
  } else {
    zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_INFO, "error while parsing file %s: %s\n", path, errorBuffer);
  }
  zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_DEBUG2, "%s end result is %s\n", __FUNCTION__, pluginDefinition ? "not null" : "null");
  return pluginDefinition;
}

static void installWebPluginFilesServices(WebPlugin* plugin, HttpServer *server) {
  zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_DEBUG2, "begin %s\n", __FUNCTION__);
  int i = 0;
  JsonObject *pluginDefintion = plugin->pluginDefinition;
  int pluginLocationLen = strlen(plugin->pluginLocation);
  int pluginLocationNeedsSlash = (plugin->pluginLocation[pluginLocationLen - 1] != '/');
  if (WEB_PLUGIN_TYPE_APPLICATION == plugin->pluginType) {
    JsonObject *webContent = jsonObjectGetObject(pluginDefintion, "webContent");
    if (webContent) {
      char *webDirectory = "web/";
      char *identifier = SLHAlloc(server->slh, 1024);
      char *uriMask = SLHAlloc(server->slh, 1024);
      int webDirectoryLen = strlen(webDirectory);

      snprintf(uriMask, 1024, "/%s/plugins/%s/web/**", PRODUCT, plugin->identifier);
      snprintf(identifier, 1024, "%s.webcontent.web", plugin->identifier);

      char *targetDirectory = SLHAlloc(server->slh, pluginLocationLen + webDirectoryLen + pluginLocationNeedsSlash + 1);
      sprintf(targetDirectory, "%s%s%s", plugin->pluginLocation, pluginLocationNeedsSlash ? "/" : "", webDirectory);

      zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_INFO, "%s\n", uriMask);
      zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_INFO, "%s\n", identifier);
      zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_INFO, "%s\n", targetDirectory);

      HttpService *httpService = makeGeneratedService(identifier, uriMask);
      httpService->userPointer = targetDirectory;
      httpService->authType = SERVICE_AUTH_NONE;
      httpService->serviceFunction = serveWebContent;
      registerHttpService(server, httpService);
    } else {
      zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_INFO, "webContent wasn't found in plugin defintion for %s\n", plugin->identifier);
    }
  } else if (WEB_PLUGIN_TYPE_LIBRARY == plugin->pluginType) {
    char *libraryVersion = jsonObjectGetString(pluginDefintion, "libraryVersion");
    if (libraryVersion) {
      char *baseURI = "lib";
      char *webDirectory = "web/";
      char *identifier = SLHAlloc(server->slh, 1024);
      char *uriMask = SLHAlloc(server->slh, 1024);
      int webDirectoryLen = strlen(webDirectory);
      
      snprintf(uriMask, 1024, "/%s/%s/%s/**", "lib", plugin->identifier, libraryVersion);
      snprintf(identifier, 1024, "%s.librarycontent", plugin->identifier);

      char *targetDirectory = SLHAlloc(server->slh, pluginLocationLen + pluginLocationNeedsSlash + 1);
      sprintf(targetDirectory, "%s%s", plugin->pluginLocation, pluginLocationNeedsSlash ? "/" : "");

      zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_INFO, "%s\n", uriMask);
      zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_INFO, "%s\n", identifier);
      zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_INFO, "%s\n", targetDirectory);

      HttpService *httpService = makeGeneratedService(identifier, uriMask);
      httpService->userPointer = targetDirectory;
      httpService->authType = SERVICE_AUTH_NONE;
      httpService->serviceFunction = serveLibraryContent;
      registerHttpService(server, httpService);
    } else {
      zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_INFO, "libraryVersion wasn't found in plugin defintion for %s\n", plugin->identifier);
    }
  }
  zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_DEBUG2, "end %s\n", __FUNCTION__);
}

static void installLoginService(HttpServer *server) {
  zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_DEBUG2, "begin %s\n", __FUNCTION__);

  HttpService *httpService = makeGeneratedService("com.rs.mvd.login", "/login/**");
  httpService->authType = SERVICE_AUTH_NATIVE_WITH_SESSION_TOKEN;
  httpService->serviceFunction = serveLoginWithSessionToken;
  httpService->authExtractionFunction = extractAuthorizationFromJson;
  registerHttpService(server, httpService);
}

static void installLogoutService(HttpServer *server) {
  zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_DEBUG2, "begin %s\n", __FUNCTION__);

  HttpService *httpService = makeGeneratedService("com.rs.mvd.logout", "/logout/**");
  httpService->authType = SERVICE_AUTH_NATIVE_WITH_SESSION_TOKEN;
  httpService->serviceFunction = serveLogoutByRemovingSessionToken;
  registerHttpService(server, httpService);
}

static void installWebPluginDefintionsService(WebPluginListElt *webPlugins, HttpServer *server) {
  zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_DEBUG2, "begin %s\n", __FUNCTION__);
  zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_DEBUG, "installing plugin definitions service\n");
  HttpService *httpService = makeGeneratedService("plugin definitions service", "/plugins");
  httpService->serviceFunction = servePluginDefinitions;
  httpService->userPointer = webPlugins;
  httpService->paramSpecList = makeStringParamSpec("type", SERVICE_ARG_OPTIONAL, NULL);
  httpService->authType = SERVICE_AUTH_NONE;
  registerHttpService(server, httpService);
  zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_DEBUG2, "end %s\n", __FUNCTION__);
}

static WebPluginListElt* readWebPluginDefinitions(HttpServer *server, ShortLivedHeap *slh, char *dirname) {
  int pluginDefinitionCount = 0;
  int returnCode;
  int reasonCode;
  char path[1025];
  char directoryDataBuffer[DIR_BUFFER_SIZE] = {0};
  int entriesRead = 0;
  char *jsonExt = ".json";
  int dirnameLen = strlen(dirname);
  int needsSlash = (dirname[dirnameLen - 1] != '/');
  char errorBuffer[512];
  InternalAPIMap *internalAPIMap = makeInternalAPIMap();
  UnixFile *directory = directoryOpen(dirname, &returnCode, &reasonCode);
  WebPluginListElt *webPluginListHead = NULL;
  WebPluginListElt *webPluginListTail = NULL;

  zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_DEBUG2, "%s begin dirname %s\n", __FUNCTION__, dirname);
  if (directory) {
    while ((entriesRead = directoryRead(directory, directoryDataBuffer, DIR_BUFFER_SIZE, &returnCode, &reasonCode)) > 0) {
      char *entryStart = directoryDataBuffer;
      for (int e = 0; e < entriesRead; e++) {
        int entryLength = ((short*) entryStart)[0];
        char *name = entryStart + 4;
        int isJsonFile = stringEndsWith(name, jsonExt);
        if (isJsonFile) {
          zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_DEBUG, "found JSON file %s\n", name);
          memset(path, 0, sizeof(path));
          sprintf(path, "%s%s%s", dirname, needsSlash ? "/" : "", name);
          Json *json = jsonParseFile(slh, path, errorBuffer, sizeof (errorBuffer));
          if (json) {
            dumpJson(json);
            JsonObject *jsonObject = jsonAsObject(json);
            if (jsonObject) {
              char *identifier = jsonObjectGetString(jsonObject, "identifier");
              char *pluginLocation = jsonObjectGetString(jsonObject, "pluginLocation");
              if (identifier && pluginLocation) {
                JsonObject *pluginDefinition = readPluginDefinition(slh, identifier, pluginLocation);
                if (pluginDefinition) {
                  WebPlugin *plugin = makeWebPlugin(pluginLocation, pluginDefinition, internalAPIMap);
                  if (plugin != NULL) {
                    initalizeWebPlugin(plugin, server);
                    //zlux does this, so don't bother doing it twice.
                    //installWebPluginFilesServices(plugin, server);
                    WebPluginListElt *pluginListElt = (WebPluginListElt*) safeMalloc(sizeof (WebPluginListElt), "WebPluginListElt");
                    pluginListElt->plugin = plugin;
                    if (webPluginListTail) {
                      webPluginListTail->next = pluginListElt;
                      webPluginListTail = pluginListElt;
                    } else {
                      webPluginListHead = pluginListElt;
                      webPluginListTail = pluginListElt;
                    }
                  } else {
                    zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_WARNING, "plugin id=%s is NULL and cannot be loaded.\n", identifier);
                  }
                }
              } else {
                zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_INFO, "plugin identifier or/and pluginLocation was not found in %s\n", path);
              }
            } else {
              zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_WARNING, "error in file %s: expected top level object\n", path);
            }
          } else {
            zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_WARNING, "error while parsing %s: %s\n", errorBuffer);
          }
        }
        entryStart += entryLength;
      }
      memset(directoryDataBuffer, 0, sizeof (directoryDataBuffer));
    }
    directoryClose(directory, &returnCode, &reasonCode);
    directory = NULL;
  } else {
    zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_WARNING, "couldn't open directory '%s' returnCode=%d reasonCode=0x%x\n", dirname, returnCode, reasonCode);
  }
  if (webPluginListHead) {
    installWebPluginDefintionsService(webPluginListHead, server);
  }
  zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_DEBUG2, "%s end result %d\n", __FUNCTION__, pluginDefinitionCount);
  return webPluginListHead;
}

static const char defaultConfigPath[] = "../deploy/instance/ZLUX/serverConfig/zluxserver.json";

static
void checkAndSetVariable(JsonObject *mvdSettings,
                         const char* configVariableName,
                         char* target,
                         size_t targetMax)
{
  const char* tempString = jsonObjectGetString(mvdSettings, configVariableName);
  if (tempString){
    snprintf(target, targetMax, "%s", tempString);
    zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_INFO, "%s is '%s'\n", configVariableName, target);
  } else {
    zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_INFO, "%s is not specified, or is NULL.\n", configVariableName);
  }
}

static void initLoggingComponents(void) {
  logConfigureComponent(NULL, LOG_COMP_ID_MVD_SERVER, "ZSS server", LOG_DEST_PRINTF_STDOUT, ZOWE_LOG_INFO);
  logConfigureComponent(NULL, LOG_COMP_ID_CTDS, "CT/DS", LOG_DEST_PRINTF_STDOUT, ZOWE_LOG_INFO);
  zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_INFO, "zssServer startup, version %s\n", productVersion);
}

static void initVersionComponents(void){
  sprintf(productVersion,"%d.%d.%d+%d",PRODUCT_MAJOR_VERSION,PRODUCT_MINOR_VERSION,PRODUCT_REVISION,PRODUCT_VERSION_DATE_STAMP);
}

static void printZISStatus(HttpServer *server) {

  CrossMemoryServerName *zisName =
      getConfiguredProperty(server, HTTP_SERVER_PRIVILEGED_SERVER_PROPERTY);

  CrossMemoryServerStatus status = cmsGetStatus(zisName);

  const char *shortDescription = NULL;

  if (status.cmsRC == RC_CMS_OK) {
    shortDescription = "Ok";
  } else {
    shortDescription = "Failure";
  }

  zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_ALWAYS,
          "ZIS status - %s (name='%.16s', cmsRC=%d, description='%s', "
          "clientVersion=%d)\n",
          shortDescription,
          zisName ? zisName->nameSpacePadded : "name not set",
          status.cmsRC,
          status.descriptionNullTerm,
          CROSS_MEMORY_SERVER_VERSION);

}

static void readAgentAddressAndPort(JsonObject *serverConfig, char **address, int *port) {
  JsonObject *agentSettings = jsonObjectGetObject(serverConfig, "agent");
  if (agentSettings){
    JsonObject *agentHttp = jsonObjectGetObject(agentSettings, "http");
    if (agentHttp) {
      *port = jsonObjectGetNumber(agentHttp, "port");
      JsonArray *ipAddresses = jsonObjectGetArray(agentHttp, "ipAddresses");
      if (ipAddresses) {
        if (jsonArrayGetCount(ipAddresses) > 0) {
	  Json *firstAddressItem = jsonArrayGetItem(ipAddresses, 0);
	  if (jsonIsString(firstAddressItem)) {
	    *address = jsonAsString(firstAddressItem);
	  }
	}
      }
    }
  }
  if (!(*port)) {
    *port = jsonObjectGetNumber(serverConfig, "zssPort");
  }
  if (!(*address)) {
    *address = "127.0.0.1";
  }
}

static int validateAddress(char *address, InetAddr **inetAddress, int *requiredTLSFlag) {
  *inetAddress = getAddressByName(address);
  if (strcmp(address,"127.0.0.1") && strcmp(address,"localhost")) {
    zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_WARNING, 
      "*** WARNING: Server doesn't implement HTTPS! ***\n*** In production only use localhost or 127.0.0.1! Server using: %s ***\n",
      address);
    *requiredTLSFlag = RS_TLS_WANT_TLS;
  }
  if (!strcmp(address,"0.0.0.0")) {
    return TRUE;      
  }
  if (!(*inetAddress && (*inetAddress)->data.data4.addrBytes)) {
    return FALSE;
  }

  /* TODO: No ipv6 resolution in getAddressByName yet, so nothing here either */
  return TRUE;
}

static const int FORBIDDEN_GROUP_FILE_PERMISSION = 0x18;
static const int FORBIDDEN_GROUP_DIR_PERMISSION = 0x10;
static const int FORBIDDEN_OTHER_PERMISSION = 0x07;

/* validates permissions for a given path */
static int validateConfigPermissionsInner(const char *path) {
  if (!path) {
    zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_SEVERE,
      "Cannot validate file permission, path is not defined.\n");
    return 8;
  }

  BPXYSTAT stat = {0};
  int returnCode = 0;
  int reasonCode = 0;
  int returnValue = fileInfo(path, &stat, &returnCode, &reasonCode);
  if (returnValue != 0) {
    zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_SEVERE,
      "Couldnt stat config path=%s. return=%d, reason=%d\n",path,returnCode, reasonCode);
    return 8;
  } else if (((stat.fileType == BPXSTA_FILETYPE_DIRECTORY) && (stat.flags3 & FORBIDDEN_GROUP_DIR_PERMISSION)) 
      || ((stat.fileType != BPXSTA_FILETYPE_DIRECTORY) && (stat.flags3 & FORBIDDEN_GROUP_FILE_PERMISSION))
      || (stat.flags3 & FORBIDDEN_OTHER_PERMISSION)) {
    zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_SEVERE,
      "Config path=%s has group & other permissions that are too open! Refusing to start.\n",path);
    zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_SEVERE,
      "Ensure group has no write (or execute, if file) permission. Ensure other has no permissions. Then, restart zssServer to retry.\n");
    return 8;
  }
  return 0;
}

#ifdef ZSS_IGNORE_PERMISSION_PROBLEMS

static int validateFilePermissions(const char *filePath) {
  zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_SEVERE,
    "Skipping validation of file permissions: disabled during compilation, "
    "file %s.\n", filePath);
  return 0;
}

#else /* ZSS_IGNORE_PERMISSION_PROBLEMS */

/* Validates that both file AND parent folder meet requirements */
static int validateFilePermissions(const char *filePath) {
  if (!filePath) {
    zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_SEVERE,
      "Cannot validate file permission, path is not defined.\n");
    return 8;
  }
  int filePathLen = strlen(filePath);
  int lastSlashPos = lastIndexOf(filePath, filePathLen, '/');
  if (lastSlashPos == filePathLen-1) {
    zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_SEVERE,
      "Cannot validate file permission, path was for a directory not a file.\n");
    return 8;
  }
  char *fileDirPath = safeMalloc31(filePathLen, "fileDirPath");
  memset(fileDirPath, 0, filePathLen);

  if (lastSlashPos == -1) {
    fileDirPath[0] = '.';
  } else if (lastSlashPos == 0) {
    fileDirPath[0] = '/';
  } else {
    snprintf(fileDirPath, lastSlashPos+1, "%s",filePath);
  }
  int result = validateConfigPermissionsInner(fileDirPath);
  safeFree31(fileDirPath, filePathLen);
  if (result) {
    return result;
  } else {
    return validateConfigPermissionsInner(filePath);
  }
}  

#endif /* ZSS_IGNORE_PERMISSION_PROBLEMS */

int main(int argc, char **argv){
  if (argc == 1) { 
    printf("Usage: zssServer <path to zssServer.json or zluxServer.json file>\n");
    return 8;
  }

  /* TODO consider moving this to stcBaseInit */
#ifndef METTLE
  int sigignoreRC = sigignore(SIGPIPE);
  if (sigignoreRC == -1) {
    printf("warning: sigignore(SIGPIPE)=%d, errno=%d\n", sigignoreRC, errno);
  }
#endif

  STCBase *base = (STCBase*) safeMalloc31(sizeof(STCBase), "stcbase");
  memset(base, 0x00, sizeof(STCBase));
  stcBaseInit(base); /* inits RLEAnchor, workQueue, socketSet, logContext */
  initVersionComponents();
  initLoggingComponents();

  const char *serverConfigFile;
  int returnCode = 0;
  int reasonCode = 0;
  char rootDir[COMMON_PATH_MAX];
  char productDir[COMMON_PATH_MAX];
  char siteDir[COMMON_PATH_MAX];
  char instanceDir[COMMON_PATH_MAX];
  char groupsDir[COMMON_PATH_MAX];
  char usersDir[COMMON_PATH_MAX];
  char pluginsDir[COMMON_PATH_MAX];
  char *tempString;
  
  if (argc >= 1){
    if (0 == strcmp("default", argv[1])) {
      serverConfigFile = defaultConfigPath;
    } else {
      serverConfigFile = argv[1];
    }
  }
  zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_INFO, "Server config file=%s\n", serverConfigFile);
  int invalid = validateFilePermissions(serverConfigFile);
  if (invalid) {
    return invalid;
  } 
  ShortLivedHeap *slh = makeShortLivedHeap(0x40000, 0x40);
  JsonObject *mvdSettings = readServerSettings(slh, serverConfigFile);
  if (mvdSettings) {
    /* Hmm - most of these aren't used, at least here. */
    checkAndSetVariable(mvdSettings, "rootDir", rootDir, COMMON_PATH_MAX);
    checkAndSetVariable(mvdSettings, "productDir", productDir, COMMON_PATH_MAX);
    checkAndSetVariable(mvdSettings, "siteDir", siteDir, COMMON_PATH_MAX);
    checkAndSetVariable(mvdSettings, "instanceDir", instanceDir, COMMON_PATH_MAX);
    checkAndSetVariable(mvdSettings, "groupsDir", groupsDir, COMMON_PATH_MAX);
    checkAndSetVariable(mvdSettings, "usersDir", usersDir, COMMON_PATH_MAX);

    /* This one IS used*/
    checkAndSetVariable(mvdSettings, "pluginsDir", pluginsDir, COMMON_PATH_MAX);

    HttpServer *server = NULL;
    int port = 0;
    char *address = NULL;
    readAgentAddressAndPort(mvdSettings, &address, &port);
    InetAddr *inetAddress = NULL;
    int requiredTLSFlag = 0;
    if (!validateAddress(address, &inetAddress, &requiredTLSFlag)) {
      zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_WARNING, "server startup problem, address %s not valid\n", address);
      return 8;
    }

    zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_INFO, "ZSS server settings: address=%s, port=%d\n", address, port);
    server = makeHttpServer2(base,inetAddress,port,requiredTLSFlag,&returnCode,&reasonCode);
    if (server){
      server->defaultProductURLPrefix = PRODUCT;
      loadWebServerConfig(server, mvdSettings);
      readWebPluginDefinitions(server, slh, pluginsDir);
      installUnixFileContentsService(server);
      installUnixFileRenameService(server);
      installUnixFileCopyService(server);
      installUnixFileMakeDirectoryService(server);
      installUnixFileTouchService(server);
      installUnixFileMetadataService(server);
      installUnixFileTableOfContentsService(server);
#ifdef __ZOWE_OS_ZOS
      installVSAMDatasetContentsService(server);
      installDatasetMetadataService(server);
      installDatasetContentsService(server);
      installAuthCheckService(server);
      installSecurityManagementServices(server);
      installOMVSService(server);
      installServerStatusService(server, MVD_SETTINGS, productVersion);
#endif
      installLoginService(server);
      installLogoutService(server);
      printZISStatus(server);
      mainHttpLoop(server);

    } else{
      zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_WARNING, "server startup problem ret=%d reason=0x%x\n", returnCode, reasonCode);
    }
  }

  stcBaseTerm(base);
  safeFree31((char *)base, sizeof(STCBase));
  base = NULL;

  return 0;
}


/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

