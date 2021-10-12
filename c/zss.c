

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
#include "envService.h"
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
#include "rasService.h"
#include "certificateService.h"
#include "registerProduct.h"
#include "userInfoService.h"
#include "jwt.h"
#ifdef USE_ZOWE_TLS
#include "tls.h"
#endif // USE_ZOWE_TLS
#include "storage.h"
#include "storageApiml.h"
#include "passTicketService.h"
#include "jwk.h"

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

#define DEFAULT_TLS_CIPHERS               \
  TLS_DHE_RSA_WITH_AES_128_GCM_SHA256     \
  TLS_DHE_RSA_WITH_AES_256_GCM_SHA384     \
  TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256 \
  TLS_ECDHE_ECDSA_WITH_AES_256_GCM_SHA384 \
  TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256   \
  TLS_ECDHE_RSA_WITH_AES_256_GCM_SHA384

static int stringEndsWith(char *s, char *suffix);
static void dumpJson(Json *json);
static JsonObject *readPluginDefinition(ShortLivedHeap *slh,
                                        char *pluginIdentifier,
                                        char *resolvedPluginLocation);
static WebPluginListElt* readWebPluginDefinitions(HttpServer* server, ShortLivedHeap *slh, char *dirname,
                                                  const char *serverConfigFile, ApimlStorageSettings *apimlStorageSettings);
static JsonObject *readServerSettings(ShortLivedHeap *slh, const char *filename);
static hashtable *getServerTimeoutsHt(ShortLivedHeap *slh, Json *serverTimeouts, const char *key);
static InternalAPIMap *makeInternalAPIMap(void);
static bool readGatewaySettings(JsonObject *serverConfig, JsonObject *envConfig, char **outGatewayHost, int *outGatewayPort);
static bool isCachingServiceEnabled(JsonObject *serverConfig, JsonObject *envConfig);

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

  zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_DEBUG, "serveLoginWithSessionToken: start. response=0x%p; cookie=0x%p\n", response, response->sessionCookie);
  setResponseStatus(response,200,"OK");
  setContentType(response,"text/html");
  addStringHeader(response,"Server","jdmfws");
  addStringHeader(response,"Transfer-Encoding","chunked");
  addStringHeader(response, "Cache-control", "no-store");
  addStringHeader(response, "Pragma", "no-cache");
  writeHeader(response);

  jsonStart(p);
  {
    jsonAddString(p, "status", "OK");
  }
  jsonEnd(p);
  
  finishResponse(response);
  zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_DEBUG, "serveLoginWithSessionToken: end\n");
  return HTTP_SERVICE_SUCCESS;
}

int serveLogoutByRemovingSessionToken(HttpService *service, HttpResponse *response) {
  jsonPrinter *p = respondWithJsonPrinter(response);

  zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_DEBUG, "serveLogoutByRemovingSessionToken: start. response=0x%p; cookie=0x%p\n", response, response->sessionCookie);
  setResponseStatus(response,200,"OK");
  setContentType(response,"text/html");
  addStringHeader(response,"Server","jdmfws");
  addStringHeader(response,"Transfer-Encoding","chunked");
  addStringHeader(response, "Cache-control", "no-store");
  addStringHeader(response, "Pragma", "no-cache");
  
  /* Remove the session token when the user wants to log out */
  addStringHeader(response,"Set-Cookie","jedHTTPSession=non-token; Path=/; HttpOnly");
  response->sessionCookie = "non-token";
  writeHeader(response);

  jsonStart(p);
  {
    jsonAddString(p, "status", "OK");
  }
  jsonEnd(p);
  
  finishResponse(response);
  zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_DEBUG, "serveLogoutByRemovingSessionToken: end\n");
  return HTTP_SERVICE_SUCCESS;
}

static
void checkAndSetVariable(JsonObject *mvdSettings,
                         const char* configVariableName,
                         char* target,
                         size_t targetMax);

static
void checkAndSetVariableWithEnvOverride(JsonObject *mvdSettings,
                         const char* configVariableName,
                         JsonObject *envSettings,
                         const char* envConfigVariableName,
                         char* target,
                         size_t targetMax);

#ifdef __ZOWE_OS_ZOS
static void setPrivilegedServerName(HttpServer *server, JsonObject *mvdSettings, JsonObject *envSettings) {

  CrossMemoryServerName *privilegedServerName = (CrossMemoryServerName *)SLHAlloc(server->slh, sizeof(CrossMemoryServerName));
  char priviligedServerNameNullTerm[sizeof(privilegedServerName->nameSpacePadded) + 1];
  memset(priviligedServerNameNullTerm, 0, sizeof(priviligedServerNameNullTerm));

  checkAndSetVariableWithEnvOverride(mvdSettings, "privilegedServerName", envSettings, "ZWED_privilegedServerName", priviligedServerNameNullTerm, sizeof(priviligedServerNameNullTerm));

  if (strlen(priviligedServerNameNullTerm) != 0) {
    *privilegedServerName = cmsMakeServerName(priviligedServerNameNullTerm);
  } else {
    zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_WARNING, ZSS_LOG_PRV_SRV_NAME_MSG);
    *privilegedServerName = zisGetDefaultServerName();
  }

  setConfiguredProperty(server, HTTP_SERVER_PRIVILEGED_SERVER_PROPERTY, privilegedServerName);
}
#endif /* __ZOWE_OS_ZOS */

static void loadWebServerConfig(HttpServer *server, JsonObject *mvdSettings,
                                JsonObject *envSettings, hashtable *htUsers,
                                hashtable *htGroups, int defaultSessionTimeout){
  MVD_SETTINGS = mvdSettings;
  /* Disabled because this server is not being used by end users, but called by other servers
   * HttpService *mainService = makeGeneratedService("main", "/");
   * mainService->serviceFunction = serveMainPage;
   * mainService->authType = SERVICE_AUTH_NONE;
   */
  server->sharedServiceMem = mvdSettings;
  server->config->userTimeouts = htUsers;
  server->config->groupTimeouts = htGroups;
  server->config->defaultTimeout = defaultSessionTimeout;
  //registerHttpService(server, mainService);
  registerHttpServiceOfLastResort(server,NULL);
#ifdef __ZOWE_OS_ZOS
  setPrivilegedServerName(server, mvdSettings, envSettings);
#endif
}

#define DIR_BUFFER_SIZE 4096

static int setMVDTrace(int toWhat) {
  int was = traceLevel;
  if (isLogLevelValid(toWhat)) {
    traceLevel = toWhat;
    logSetLevel(NULL, LOG_COMP_ID_MVD_SERVER, toWhat);
  } else {
    zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_ALWAYS, ZSS_LOG_INC_LOG_LEVEL_MSG, toWhat);
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
  {"_zss.jwtTrace", setJwtTrace},
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
  zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_DEBUG, "reading server settings from %s\n", filename);
  mvdSettings = jsonParseFile(slh, filename, jsonErrorBuffer, jsonErrorBufferSize);
  if (mvdSettings) {
    if (jsonIsObject(mvdSettings)) {
      mvdSettingsJsonObject = jsonAsObject(mvdSettings);
    } else {
      zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_SEVERE, ZSS_LOG_FILE_EXPECTED_TOP_MSG, filename);
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
    zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_SEVERE, ZSS_LOG_PARS_ZSS_SETTING_MSG, filename, jsonErrorBuffer);
  }
  return mvdSettingsJsonObject;
}

static int getDefaultSessionTimeout(Json *serverTimeouts) {
  JsonObject *serverTimeoutsJsonObject = NULL;
  if (jsonIsObject(serverTimeouts)) {
    serverTimeoutsJsonObject = jsonAsObject(serverTimeouts);
    Json *defaultJson = jsonObjectGetPropertyValue(serverTimeoutsJsonObject, "default");
    if (defaultJson == NULL){
      return 0;
    } else if (!jsonIsNumber(defaultJson)){
      return 0;
    } else{
      return jsonAsNumber(defaultJson);
    }
  } else {
    return 0;
  }
}

static hashtable *getServerTimeoutsHt(ShortLivedHeap *slh, Json *serverTimeouts, const char *key) {
  int rc = 0;
  int rsn = 0;
  JsonObject *serverTimeoutsJsonObject = NULL;
  hashtable *ht;
  if (!strcmp(key,"groups")) {
    //int comparisons
    ht = htCreate(277, NULL, NULL, NULL, NULL);
  } else {
    ht = htCreate(277, stringHash, stringCompare, NULL, NULL);
  } 
  zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_DEBUG, "reading '%s' timeout settings\n", key);
  if (jsonIsObject(serverTimeouts)) {
    serverTimeoutsJsonObject = jsonAsObject(serverTimeouts);
  } else {
    zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_WARNING, ZSS_LOG_FILE_EXPECTED_TOP_MSG);
    return ht; // Returns empty hash table
  }
  JsonObject *users = jsonObjectGetObject(serverTimeoutsJsonObject, key);
  if (users) {
    JsonProperty *property = jsonObjectGetFirstProperty(users);
    while (property != NULL) {
      char *userKey = jsonPropertyGetKey(property);
      strupcase(userKey); /* make case insensitive */
      int timeoutValue = jsonObjectGetNumber(users, userKey);
      if (!strcmp(key, "groups")) {
        int gid = groupIdGet(userKey, &rc, &rsn);
        if (rc == 0) {
          htPut(ht, POINTER_FROM_INT(gid), (void*)timeoutValue);
        }
      } else {
        htPut(ht, userKey, (void*)timeoutValue);
      }

      property = jsonObjectGetNextProperty(property);
    }
  }
  return ht;
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

static char* resolvePluginLocation(ShortLivedHeap *slh, const char *pluginLocation, const char *relativeTo) {
  zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_DEBUG, "resolving pluginLocation '%s' relative to '%s'\n",
          pluginLocation ? pluginLocation : "<NULL>",
          relativeTo ? relativeTo : "<NULL>");
  if (!pluginLocation) {
    zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_DEBUG, "pluginLocation is <NULL>\n");
    return NULL;
  }
  const int maxPathSize = 1024;
  char *resolvedPluginLocation = SLHAlloc(slh, maxPathSize);
  if (!resolvedPluginLocation) {
    zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_DEBUG, "failed to allocate memory for resolvedPluginLocation\n");
    return NULL;
  }
  int pluginLocationLen = strlen(pluginLocation);
  int relativeToLen = relativeTo ? strlen(relativeTo) : 0;
  bool hasTrailingSlash = (pluginLocationLen > 0 && pluginLocation[pluginLocationLen - 1] == '/');
  if (relativeTo != NULL && relativeToLen > 1 && relativeTo[0] == '$') {
    char *varValue = getenv(relativeTo + 1);
    if (varValue) {
      zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_DEBUG, "relativeTo '%s' resolves to '%s'\n", relativeTo, varValue);
      relativeTo = varValue;
      relativeToLen = strlen(relativeTo);
    } else {
      zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_DEBUG, "unable to resolve relativeTo '%s', env var not set\n", relativeTo, varValue);
    }
  }
  if (!relativeTo || (pluginLocation[0] == '/')) {
    snprintf(resolvedPluginLocation, maxPathSize, "%.*s",
             hasTrailingSlash ? pluginLocationLen - 1 : pluginLocationLen,
             pluginLocation);
  } else {
    bool relativeNeedsSlash = (relativeToLen > 0 && relativeTo[relativeToLen - 1] != '/');
    snprintf(resolvedPluginLocation, maxPathSize, "%s%s%.*s",
            relativeTo, relativeNeedsSlash ? "/" : "",
            hasTrailingSlash ? pluginLocationLen - 1 : pluginLocationLen,
            pluginLocation);
  }
  zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_DEBUG, "resolved pluginLocation is '%s'\n", resolvedPluginLocation);
  return resolvedPluginLocation;
}

static JsonObject *readPluginDefinition(ShortLivedHeap *slh,
                                        char *pluginIdentifier,
                                        char *resolvedPluginLocation) {
  JsonObject *pluginDefinition = NULL;
  char path[1024];
  char errorBuffer[512];
  
  zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_DEBUG2, "%s begin identifier %s location %s\n", __FUNCTION__, pluginIdentifier, resolvedPluginLocation);
  sprintf(path, "%s/%s", resolvedPluginLocation, "pluginDefinition.json");
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
          zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_WARNING, ZSS_LOG_EXPEC_PLUGIN_ID_MSG, pluginIdentifier, identifier);
        }
      } else {
        zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_WARNING, ZSS_LOG_PLUGIN_ID_NFOUND_MSG, path);
      }
    } else {
      zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_SEVERE, ZSS_LOG_FILE_EXPECTED_TOP_MSG, path);
    }
  } else {
    zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_SEVERE, ZSS_LOG_PARS_FILE_MSG, path, errorBuffer);
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
      zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_WARNING, ZSS_LOG_WEB_CONT_NFOUND_MSG, plugin->identifier);
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
      zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_WARNING, ZSS_LOG_LIBR_VER_NFOUND_MSG, plugin->identifier);
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

static int checkLoggingVerbosity(const char *serverConfigFile, char *pluginIdentifier, ShortLivedHeap *slh) {
  char errorBuffer[1024] = {0};
  
  Json *json = jsonParseFile(slh, serverConfigFile, errorBuffer, sizeof(errorBuffer));
  if (json) {
    if (jsonIsObject(json)) {
      JsonObject *jsonObject = jsonAsObject(json);
      JsonObject *logLevels = jsonObjectGetObject(jsonObject, "logLevels");
      if (logLevels != NULL) {
        JsonProperty *property = jsonObjectGetFirstProperty(logLevels);
        while (property != NULL) {
          if (!strcmp(jsonPropertyGetKey(property), pluginIdentifier)) {
            int logLevel = jsonObjectGetNumber(logLevels, pluginIdentifier);
            if (logLevel <= ZOWE_LOG_DEBUG3 || logLevel >= ZOWE_LOG_ALWAYS) {
              return logLevel;
            }
            else {
              break;
            }
          }
          property = jsonObjectGetNextProperty(property);
        }
      }
    }
  }
  
  return ZOWE_LOG_INFO;
}

static WebPluginListElt* readWebPluginDefinitions(HttpServer *server, ShortLivedHeap *slh, char *dirname,
                                                  const char *serverConfigFile,
                                                  ApimlStorageSettings *apimlStorageSettings) {
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

  unsigned int idMultiplier = 1; /* multiplier for the dynamic logging id */
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
              char *relativeTo = jsonObjectGetString(jsonObject, "relativeTo");
              char *resolvedPluginLocation = resolvePluginLocation(slh, pluginLocation, relativeTo);
              int pluginLogLevel = checkLoggingVerbosity(serverConfigFile, identifier, slh);
              if (identifier && resolvedPluginLocation) {
                JsonObject *pluginDefinition = readPluginDefinition(slh, identifier, resolvedPluginLocation);
                if (pluginDefinition) {
                  Storage *remoteStorage = NULL;
                  if (apimlStorageSettings) {
                    remoteStorage = makeApimlStorage(apimlStorageSettings, identifier);
                  }
                  WebPlugin *plugin = makeWebPlugin2(resolvedPluginLocation, pluginDefinition, internalAPIMap,
                                                    &idMultiplier, pluginLogLevel, remoteStorage);
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
                    if (server->loggingIdsByName != NULL) {
                      if (plugin->dataServiceCount > 0) {
                        for (int i = 0; i < plugin->dataServiceCount; i++) {
                          size_t keyLength = strlen(plugin->dataServices[i]->identifier);
                          char key[keyLength+1];
                          memset(&key, 0, sizeof(key));
                          strncpy(key, plugin->dataServices[i]->identifier, keyLength);
                          for (int j = 0; j < keyLength; j++) {
                            if (key[j] == '/') {
                              key[j] = ':';
                            }
                          }
                          htPut(server->loggingIdsByName,
                                key,
                                &(plugin->dataServices[i]->loggingIdentifier));
                        }
                      }
                    }
                  } else {
                    zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_WARNING, ZSS_LOG_PLUGIN_ID_NULL_MSG, identifier);
                  }
                }
              } else {
                zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_INFO, ZSS_LOG_PLUGIN_IDLOC_NFOUND_MSG, path);
              }
            } else {
              zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_SEVERE, ZSS_LOG_FILE_EXPECTED_TOP_MSG, path);
            }
          } else {
            zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_SEVERE, ZSS_LOG_PARS_GENERIC_MSG, errorBuffer);
          }
        }
        entryStart += entryLength;
      }
      memset(directoryDataBuffer, 0, sizeof (directoryDataBuffer));
    }
    directoryClose(directory, &returnCode, &reasonCode);
    directory = NULL;
  } else {
    zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_WARNING, ZSS_LOG_OPEN_DIR_FAIL_MSG, dirname, returnCode, reasonCode);
  }
  if (webPluginListHead) {
    installWebPluginDefintionsService(webPluginListHead, server);
  }
  zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_DEBUG2, "%s end result %d\n", __FUNCTION__, pluginDefinitionCount);
  return webPluginListHead;
}

static ApimlStorageSettings *readApimlStorageSettings(ShortLivedHeap *slh, JsonObject *serverConfig, JsonObject *envConfig, TlsEnvironment *tlsEnv) {
  char *host = NULL;
  int port = 0;
  bool configured = false;

  do {
    if (!isCachingServiceEnabled(serverConfig, envConfig)) {
      zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_DEBUG, "Caching Service disabled\n");
      break;
    }
    if (!readGatewaySettings(serverConfig, envConfig, &host, &port)) {
      zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_DEBUG, "Gateway settings not found\n");
      break;
    }
    if (!tlsEnv) {
      zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_DEBUG, "TLS settings not found\n");
      break;
    }
    configured = true;
  } while(0);

  if (!configured) {
    zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_INFO, ZSS_LOG_CACHE_NOT_CFG_MSG);
    return NULL;
  }
  ApimlStorageSettings *settings = (ApimlStorageSettings*)SLHAlloc(slh, sizeof(*settings));
  memset(settings, 0, sizeof(*settings));
  settings->host = host;
  settings->port = port;
  settings->tlsEnv = tlsEnv;
  zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_INFO, ZSS_LOG_CACHE_SETTINGS_MSG settings->host, settings->port);
  return settings;
}

static const char defaultConfigPath[] = "../defaults/serverConfig/server.json";

static
void checkAndSetVariable(JsonObject *mvdSettings,
                         const char* configVariableName,
                         char* target,
                         size_t targetMax)
{
  const char* tempString = jsonObjectGetString(mvdSettings, configVariableName);
  if (tempString){
    snprintf(target, targetMax, "%s", tempString);
    zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_DEBUG, "%s is '%s'\n", configVariableName, target);
  } else {
    zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_DEBUG, "%s is not specified, or is NULL.\n", configVariableName);
  }
}

static
void checkAndSetVariableWithEnvOverride(JsonObject *mvdSettings,
                         const char* configVariableName,
                         JsonObject *envSettings,
                         const char* envConfigVariableName,
                         char* target,
                         size_t targetMax)
{
  bool override=true;
  char* tempString = jsonObjectGetString(envSettings, envConfigVariableName);
  if(tempString == NULL) {
    override=false;
    tempString = jsonObjectGetString(mvdSettings, configVariableName);
  }
  if (tempString){
    snprintf(target, targetMax, "%s", tempString);
    if(override){
      zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_DEBUG, "%s override with env %s is '%s'\n", configVariableName, envConfigVariableName, target);
    }
    else {
      zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_DEBUG, "%s is '%s'\n", configVariableName, target);
    }
  } else {
    zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_DEBUG, "%s or env %s is not specified, or is NULL.\n", configVariableName, envConfigVariableName);
  }
}

static void initLoggingComponents(void) {
  logConfigureComponent(NULL, LOG_COMP_ID_SECURITY, "ZSS Security API", LOG_DEST_PRINTF_STDOUT, ZOWE_LOG_INFO);
  logConfigureComponent(NULL, LOG_COMP_DISCOVERY, "Zowe Discovery", LOG_DEST_PRINTF_STDOUT, ZOWE_LOG_INFO);
  logConfigureComponent(NULL, LOG_COMP_RESTDATASET, "Zowe Dataset REST", LOG_DEST_PRINTF_STDOUT, ZOWE_LOG_INFO);
  logConfigureComponent(NULL, LOG_COMP_RESTFILE, "Zowe UNIX REST", LOG_DEST_PRINTF_STDOUT, ZOWE_LOG_INFO);
  logConfigureComponent(NULL, LOG_COMP_ID_UNIXFILE, "ZSS UNIX REST", LOG_DEST_PRINTF_STDOUT, ZOWE_LOG_INFO);
  logConfigureComponent(NULL, LOG_COMP_DATASERVICE, "ZSS dataservices", LOG_DEST_PRINTF_STDOUT, ZOWE_LOG_INFO);
  logConfigureComponent(NULL, LOG_COMP_ID_MVD_SERVER, "ZSS server", LOG_DEST_PRINTF_STDOUT, ZOWE_LOG_INFO);
  logConfigureComponent(NULL, LOG_COMP_ID_CTDS, "CT/DS", LOG_DEST_PRINTF_STDOUT, ZOWE_LOG_INFO);
  logConfigureComponent(NULL, LOG_COMP_ID_APIML_STORAGE, "APIML Storage", LOG_DEST_PRINTF_STDOUT, ZOWE_LOG_INFO);
  logConfigureComponent(NULL, LOG_COMP_ID_JWK, "JWT Secret", LOG_DEST_PRINTF_STDOUT, ZOWE_LOG_DEBUG2);
  zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_INFO, ZSS_LOG_ZSS_START_VER_MSG, productVersion);
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
          ZSS_LOG_ZIS_STATUS_MSG,
          shortDescription,
          zisName ? zisName->nameSpacePadded : "name not set",
          status.cmsRC,
          status.descriptionNullTerm,
          CROSS_MEMORY_SERVER_VERSION);

}

static void readAgentAddressAndPort(JsonObject *serverConfig, JsonObject *envConfig, char **address, int *port) {
  *port = jsonObjectGetNumber(envConfig, "ZWED_agent_http_port");
  *address = jsonObjectGetString(envConfig, "ZWED_agent_http_ipAddresses");

  JsonObject *agentSettings = jsonObjectGetObject(serverConfig, "agent");
  if (agentSettings){
    JsonObject *agentHttp = jsonObjectGetObject(agentSettings, "http");
    if (agentHttp) {
      if (!(*port)) {
      *port = jsonObjectGetNumber(agentHttp, "port");
      }
      JsonArray *ipAddresses = jsonObjectGetArray(agentHttp, "ipAddresses");
      if (ipAddresses) {
        if (jsonArrayGetCount(ipAddresses) > 0) {
          Json *firstAddressItem = jsonArrayGetItem(ipAddresses, 0);
          if (jsonIsString(firstAddressItem)) {
            if (!(*address)) {
              *address = jsonAsString(firstAddressItem);
            }
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

#define PORT_KEY         "port"
#define IP_ADDRESSES_KEY "ipAddresses"
#define KEYRING_KEY      "keyring"
#define LABEL_KEY        "label"
#define STASH_KEY        "stash"
#define PASSWORD_KEY     "password"

#define AGENT_HTTPS_PREFIX       "ZWED_agent_https_"
#define ENV_AGENT_HTTPS_KEY(key) AGENT_HTTPS_PREFIX key

static bool readAgentHttpsSettings(ShortLivedHeap *slh,
                                   JsonObject *serverConfig,
                                   JsonObject *envConfig,
                                   char **outAddress,
                                   int *outPort,
                                   TlsEnvironment **outTlsEnv
                                  ) {
  int port = jsonObjectGetNumber(envConfig, ENV_AGENT_HTTPS_KEY(PORT_KEY));
  char *address = jsonObjectGetString(envConfig, ENV_AGENT_HTTPS_KEY(IP_ADDRESSES_KEY));

  TlsSettings *settings = (TlsSettings*)SLHAlloc(slh, sizeof(*settings));
  settings->ciphers = DEFAULT_TLS_CIPHERS;
  settings->keyring = jsonObjectGetString(envConfig, ENV_AGENT_HTTPS_KEY(KEYRING_KEY));
  settings->label = jsonObjectGetString(envConfig, ENV_AGENT_HTTPS_KEY(LABEL_KEY));
  settings->stash = jsonObjectGetString(envConfig, ENV_AGENT_HTTPS_KEY(STASH_KEY));
  settings->password = jsonObjectGetString(envConfig, ENV_AGENT_HTTPS_KEY(PASSWORD_KEY));

  JsonObject *agentSettings = jsonObjectGetObject(serverConfig, "agent");
  if (agentSettings) {
    JsonObject *agentHttps = jsonObjectGetObject(agentSettings, "https");
    if (agentHttps) {
      if (!port) {
        port = jsonObjectGetNumber(agentHttps, PORT_KEY);
      }
      if (!address) {
        JsonArray *ipAddresses = jsonObjectGetArray(agentHttps, IP_ADDRESSES_KEY);
        if (ipAddresses && jsonArrayGetCount(ipAddresses) > 0) {
          Json *firstAddressItem = jsonArrayGetItem(ipAddresses, 0);
          if (jsonIsString(firstAddressItem)) {
            address = jsonAsString(firstAddressItem);
          }
        }
      }
      if (!settings->keyring) {
        settings->keyring = jsonObjectGetString(agentHttps, KEYRING_KEY);
      }
      if (!settings->label) {
        settings->label = jsonObjectGetString(agentHttps, LABEL_KEY);
      }
      if (!settings->stash) {
        settings->stash = jsonObjectGetString(agentHttps, STASH_KEY);
      }
      if (!settings->password) {
        settings->password = jsonObjectGetString(agentHttps, PASSWORD_KEY);
      }
    }
  }
  if (!address) {
    address = "127.0.0.1";
  }

  const char *useTlsParam = getenv("ZOWE_ZSS_SERVER_TLS");
  zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_DEBUG, "Environment variable ZOWE_ZSS_SERVER_TLS is %s\n",
          useTlsParam ? useTlsParam : "not set");

  bool forceHttp = useTlsParam && (0 == strcmp(useTlsParam, "false"));
  bool isHttpsConfigured = !forceHttp && port && settings->keyring;
  if (settings->keyring) {
      zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_INFO, ZSS_LOG_TLS_SETTINGS_MSG,
              settings->keyring,
              settings->label ? settings->label : "(no label)",
              settings->password ? "****" : "(no password)",
              settings->stash ? settings->stash : "(no stash)");
    TlsEnvironment *tlsEnv = NULL;
    int rc = tlsInit(&tlsEnv, settings);
    if (rc != 0) {
      zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_WARNING, ZSS_LOG_TLS_INIT_MSG, rc, tlsStrError(rc));
    } else {
      *outTlsEnv = tlsEnv;
    }
  }
  if (isHttpsConfigured) {
    *outPort = port;
    *outAddress = address;
  }
  return isHttpsConfigured;
}

#define NODE_MEDIATION_LAYER_SERVER_PREFIX   "ZWED_node_mediationLayer_server_"
#define ENV_NODE_MEDIATION_LAYER_SERVER_KEY(key) NODE_MEDIATION_LAYER_SERVER_PREFIX key

// Returns false if gateway host and port not found
static bool readGatewaySettings(JsonObject *serverConfig,
                                JsonObject *envConfig,
                                char **outGatewayHost,
                                int *outGatewayPort
                               ) {
  char *gatewayHost = jsonObjectGetString(envConfig, ENV_NODE_MEDIATION_LAYER_SERVER_KEY("gatewayHostname"));
  char *hostname = jsonObjectGetString(envConfig, ENV_NODE_MEDIATION_LAYER_SERVER_KEY("hostname"));
  int gatewayPort = jsonObjectGetNumber(envConfig, ENV_NODE_MEDIATION_LAYER_SERVER_KEY("gatewayPort"));
  if (gatewayHost && gatewayPort) {
    *outGatewayHost = gatewayHost;
    *outGatewayPort = gatewayPort;
    return true;
  }

  JsonObject *nodeSettings = jsonObjectGetObject(serverConfig, "node");
  if (!nodeSettings) {
    return false;
  }
  JsonObject *mediationLayerSettings = jsonObjectGetObject(nodeSettings, "mediationLayer");
  if (!mediationLayerSettings) {
    return false;
  }
  JsonObject *serverSettings = jsonObjectGetObject(mediationLayerSettings, "server");
  if (!serverSettings) {
    return false;
  }
  if (!gatewayHost) {
    gatewayHost = jsonObjectGetString(serverSettings, "gatewayHostname");
  }
  if (!gatewayHost) {
    gatewayHost = hostname;
  }
  if (!gatewayHost) {
    gatewayHost = jsonObjectGetString(serverSettings, "hostname");
  }
  if (!gatewayPort) {
    gatewayPort = jsonObjectGetNumber(serverSettings, "gatewayPort");
  }
  if (!gatewayHost || !gatewayPort) {
    return false;
  }
  *outGatewayHost = gatewayHost;
  *outGatewayPort = gatewayPort;
  return true;
}

#define NODE_MEDIATION_LAYER_PREFIX   "ZWED_node_mediationLayer_"
#define ENV_NODE_MEDIATION_LAYER_KEY(key) NODE_MEDIATION_LAYER_PREFIX key
#define NODE_MEDIATION_LAYER_CACHING_SERVICE_PREFIX   NODE_MEDIATION_LAYER_PREFIX "cachingService_"
#define NODE_MEDIATION_LAYER_CACHING_SERVICE_KEY(key) NODE_MEDIATION_LAYER_CACHING_SERVICE_PREFIX key
#define ENABLED_KEY "enabled"

static bool isCachingServiceEnabled(JsonObject *serverConfig, JsonObject *envConfig) {
  bool mediationLayerEnabledFound = jsonObjectHasKey(envConfig, ENV_NODE_MEDIATION_LAYER_KEY(ENABLED_KEY));
  bool mediationLayerEnabled = false;
  if (mediationLayerEnabledFound) {
    mediationLayerEnabled = jsonObjectGetBoolean(envConfig, ENV_NODE_MEDIATION_LAYER_KEY(ENABLED_KEY));
    if (!mediationLayerEnabled) {
      return false;
    }
  }
  JsonObject *nodeSettings = jsonObjectGetObject(serverConfig, "node");
  JsonObject *mediationLayerSettings = NULL;
  JsonObject *cachingServiceSettings = NULL;
  if (nodeSettings) {
    mediationLayerSettings = jsonObjectGetObject(nodeSettings, "mediationLayer");
  }
  if (mediationLayerSettings) {
    cachingServiceSettings = jsonObjectGetObject(mediationLayerSettings, "cachingService");
  }
  if (!mediationLayerEnabled) {
    if (!mediationLayerSettings) {
      return false;
    }
    mediationLayerEnabled = jsonObjectGetBoolean(mediationLayerSettings, ENABLED_KEY);
    if (!mediationLayerEnabled) {
      return false;
    }
  }
  bool cachingServiceEnabledFound = jsonObjectGetBoolean(envConfig, NODE_MEDIATION_LAYER_CACHING_SERVICE_KEY(ENABLED_KEY));
  if (cachingServiceEnabledFound) {
    return jsonObjectGetBoolean(envConfig, NODE_MEDIATION_LAYER_CACHING_SERVICE_KEY(ENABLED_KEY));
  }
  if (!cachingServiceSettings) {
    return false;
  }
  return jsonObjectGetBoolean(cachingServiceSettings, ENABLED_KEY);
}

static int validateAddress(char *address, InetAddr **inetAddress, int *requiredTLSFlag) {
  *inetAddress = getAddressByName(address);
  if (strcmp(address,"127.0.0.1") && strcmp(address,"localhost")) {
#ifndef USE_ZOWE_TLS
    zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_WARNING, 
      ZSS_LOG_HTTPS_NO_IMPLEM_MSG,
      address);
#endif // USE_ZOWE_TLS
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
      ZSS_LOG_CANT_VAL_PERMISS_MSG);
    return 8;
  }

  BPXYSTAT stat = {0};
  int returnCode = 0;
  int reasonCode = 0;
  int returnValue = fileInfo(path, &stat, &returnCode, &reasonCode);
  if (returnValue != 0) {
    zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_SEVERE,
      ZSS_LOG_CANT_STAT_CONFIG_MSG,path,returnCode, reasonCode);
    return 8;
  } else if (((stat.fileType == BPXSTA_FILETYPE_DIRECTORY) && (stat.flags3 & FORBIDDEN_GROUP_DIR_PERMISSION)) 
      || ((stat.fileType != BPXSTA_FILETYPE_DIRECTORY) && (stat.flags3 & FORBIDDEN_GROUP_FILE_PERMISSION))
      || (stat.flags3 & FORBIDDEN_OTHER_PERMISSION)) {
    zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_SEVERE,
      ZSS_LOG_CONFIG_OPEN_PERMISS_MSG,path);
    zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_SEVERE,
      ZSS_LOG_ENSURE_PERMISS_MSG);
    return 8;
  }
  return 0;
}

#ifdef ZSS_IGNORE_PERMISSION_PROBLEMS

static int validateFilePermissions(const char *filePath) {
  zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_SEVERE,
    ZSS_LOG_SKIP_PERMISS_MSG, filePath);
  return 0;
}

#else /* ZSS_IGNORE_PERMISSION_PROBLEMS */

/* Validates that both file AND parent folder meet requirements */
static int validateFilePermissions(const char *filePath) {
  if (!filePath) {
    zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_SEVERE,
      ZSS_LOG_PATH_UNDEF_MSG);
    return 8;
  }
  int filePathLen = strlen(filePath);
  int lastSlashPos = lastIndexOf(filePath, filePathLen, '/');
  if (lastSlashPos == filePathLen-1) {
    zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_SEVERE,
      ZSS_LOG_PATH_DIR_MSG);
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

/*
    "agent": { 
      ...,      
      "jwt": {
        "enabled": true,
        "fallback": true,
        "key": {
          "type": "pkcs11",     //optional, since only one type is supported
          "token": "ZOWE.ZSS.APIMLQA",
          "label": "KEY_RS256"
        }
      }
    }
 
  If `jwtKeystore` is present in the config, this will initialize the server
  to use the specified keystore and public key, with or without fallback to
  session tokens, as specified.

  Otherwise we would just use session tokens.
 */
int initializeJwtKeystoreIfConfigured(JsonObject *const serverConfig,
                                      HttpServer *const httpServer, JsonObject *const envSettings) {
  JsonObject *const agentSettings = jsonObjectGetObject(serverConfig, "agent");
  if (agentSettings == NULL) {
    zowelog(NULL,
        LOG_COMP_ID_MVD_SERVER,
        ZOWE_LOG_WARNING,
        ZSS_LOG_NO_JWT_AGENT_MSG);
    return 0;
  }

  JsonObject *const jwtSettings = jsonObjectGetObject(agentSettings, "jwt");
  char *envTokenName = jsonObjectGetString(envSettings, "ZWED_agent_jwt_token_name");
  char *envTokenLabel = jsonObjectGetString(envSettings, "ZWED_agent_jwt_token_label");
  Json *envFallbackJsonVal = jsonObjectGetPropertyValue(envSettings, "ZWED_agent_jwt_fallback");
  int envFallback = (envFallbackJsonVal && jsonIsBoolean(envFallbackJsonVal)) ?
                     jsonAsBoolean(envFallbackJsonVal) : TRUE;
  bool envIsSet = (envTokenName != NULL
                      && envTokenLabel != NULL);

  if(envIsSet){
    int initTokenRc, p11rc, p11Rsn;
    const int contextInitRc = httpServerInitJwtContext(httpServer,
        envFallback,
        envTokenName,
        envTokenLabel,
        CKO_PUBLIC_KEY,
        &initTokenRc, &p11rc, &p11Rsn);
    if (contextInitRc != 0) {
      zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_SEVERE,
          ZSS_LOG_NO_LOAD_JWT_MSG,
          envTokenName,
          envTokenLabel,
          initTokenRc, p11rc, p11Rsn);
      return 1;
    }
    zowelog(NULL,
      LOG_COMP_ID_MVD_SERVER,
      ZOWE_LOG_INFO,
      ZSS_LOG_JWT_TOKEN_FALLBK_MSG,
      envTokenName,
      envTokenLabel,
      envFallback ? "with" : "without");
    return 0;
  }

  if (jwtSettings == NULL) {
    zowelog(NULL,
        LOG_COMP_ID_MVD_SERVER,
        ZOWE_LOG_WARNING,
        ZSS_LOG_NO_JWT_CONFIG_MSG);
    return 0;
  }

  if (!jsonObjectGetBoolean(jwtSettings, "enabled")) {
    zowelog(NULL,
        LOG_COMP_ID_MVD_SERVER,
        ZOWE_LOG_INFO,
        ZSS_LOG_NO_JWT_DISABLED_MSG);
    return 0;
  }

  const int fallback = jsonObjectGetBoolean(jwtSettings, "fallback");

  JsonObject *const jwtKeyConfig = jsonObjectGetObject(jwtSettings, "key");
  if (jwtKeyConfig == NULL) {
    zowelog(NULL,
        LOG_COMP_ID_MVD_SERVER,
        ZOWE_LOG_SEVERE,
        ZSS_LOG_JWT_CONFIG_MISSING_MSG);
    return 1;
  }

  const char *const keystoreType = jsonObjectGetString(jwtKeyConfig, "type");
  const char *const keystoreToken = jsonObjectGetString(jwtKeyConfig, "token");
  const char *const tokenLabel = jsonObjectGetString(jwtKeyConfig, "label");

  if (keystoreType != NULL && strcmp(keystoreType, "pkcs11") != 0) {
    zowelog(NULL,
        LOG_COMP_ID_MVD_SERVER,
        ZOWE_LOG_SEVERE,
        ZSS_LOG_JWT_KEYSTORE_UNKN_MSG, keystoreType);
    return 1;
  } else if (keystoreToken == NULL) {
    zowelog(NULL,
        LOG_COMP_ID_MVD_SERVER,
        ZOWE_LOG_SEVERE,
        ZSS_LOG_JWT_KEYSTORE_NAME_MSG);
    return 1;
  } else if(tokenLabel == NULL){
    zowelog(NULL,
        LOG_COMP_ID_MVD_SERVER,
        ZOWE_LOG_SEVERE,
        "Invalid JWT configuration: token label missing\n");
    return 1;
  } else {
    zowelog(NULL,
        LOG_COMP_ID_MVD_SERVER,
        ZOWE_LOG_INFO,
        ZSS_LOG_JWT_TOKEN_FALLBK_MSG,
        keystoreToken,
        tokenLabel,
        fallback? "with" : "without");
  }


  int initTokenRc, p11rc, p11Rsn;
  const int contextInitRc = httpServerInitJwtContext(httpServer,
      fallback,
      keystoreToken,
      tokenLabel,
      CKO_PUBLIC_KEY,
      &initTokenRc, &p11rc, &p11Rsn);
  if (contextInitRc != 0) {
    zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_SEVERE,
        ZSS_LOG_NO_LOAD_JWT_MSG,
        tokenLabel,
        keystoreToken,
        initTokenRc, p11rc, p11Rsn);
    return 1;
  }

  return 0;
}

/* djb2 */
static int hashPluginID(void *key) {

  char *str = (char*)key;

  unsigned long hash = 5381;
  int c;

  while (c = *str++) {
    hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
  }

  return hash;
}

static int comparePluginID(void *key1, void *key2) {

  if (!strcmp(key1, key2)) {
    return true;
  }

  return false;
}

#define PLUGIN_ID_HT_BACKBONE_SIZE 29

static void initializePluginIDHashTable(HttpServer *server) {

  server->loggingIdsByName = htCreate(PLUGIN_ID_HT_BACKBONE_SIZE, hashPluginID, comparePluginID, NULL, NULL);
}

#define ZSS_STATUS_OK     0
#define ZSS_STATUS_ERROR  8

int main(int argc, char **argv){
  int zssStatus = ZSS_STATUS_OK;

  STCBase *base = (STCBase*) safeMalloc31(sizeof(STCBase), "stcbase");
  memset(base, 0x00, sizeof(STCBase));
  stcBaseInit(base); /* inits RLEAnchor, workQueue, socketSet, logContext */
  initVersionComponents();
  initLoggingComponents();

  if (argc == 1) {
    zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_WARNING, ZSS_LOG_PATH_TO_SERVER_MSG);
    zssStatus = ZSS_STATUS_ERROR;
    goto out_term_stcbase;
  }

  /* TODO consider moving this to stcBaseInit */
#ifndef METTLE
  int sigignoreRC = sigignore(SIGPIPE);
  if (sigignoreRC == -1) {
    zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_WARNING, ZSS_LOG_SIG_IGNORE_MSG, sigignoreRC, errno);
  }
#endif

  const char *serverConfigFile;
  int returnCode = 0;
  int reasonCode = 0;
  char productDir[COMMON_PATH_MAX];
  char siteDir[COMMON_PATH_MAX];
  char instanceDir[COMMON_PATH_MAX];
  char groupsDir[COMMON_PATH_MAX];
  char usersDir[COMMON_PATH_MAX];
  char pluginsDir[COMMON_PATH_MAX];
  char productReg[COMMON_PATH_MAX];
  char productPID[COMMON_PATH_MAX];
  char productVer[COMMON_PATH_MAX];
  char productOwner[COMMON_PATH_MAX];
  char productName[COMMON_PATH_MAX];
  char *tempString;
  hashtable *htUsers;
  hashtable *htGroups;
  
  if (argc >= 1){
    if (0 == strcmp("default", argv[1])) {
      serverConfigFile = defaultConfigPath;
    } else {
      serverConfigFile = argv[1];
    }
  }
  zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_INFO, ZSS_LOG_SERVER_CONFIG_MSG, serverConfigFile);
  int invalid = validateFilePermissions(serverConfigFile);
  if (invalid) {
    zssStatus = ZSS_STATUS_ERROR;
    goto out_term_stcbase;
  } 
  ShortLivedHeap *slh = makeShortLivedHeap(0x40000, 0x40);
  JsonObject *envSettings = readEnvSettings("ZWED");
  if (envSettings == NULL) {
    zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_SEVERE, ZSS_LOG_ENV_SETTINGS_MSG);
    zssStatus = ZSS_STATUS_ERROR;
    goto out_term_stcbase;
  }
  JsonObject *mvdSettings = readServerSettings(slh, serverConfigFile);

  if (mvdSettings) {
    /* Hmm - most of these aren't used, at least here. */
    checkAndSetVariable(mvdSettings, "productDir", productDir, COMMON_PATH_MAX);
    checkAndSetVariable(mvdSettings, "siteDir", siteDir, COMMON_PATH_MAX);
    checkAndSetVariable(mvdSettings, "instanceDir", instanceDir, COMMON_PATH_MAX);
    checkAndSetVariable(mvdSettings, "groupsDir", groupsDir, COMMON_PATH_MAX);
    checkAndSetVariable(mvdSettings, "usersDir", usersDir, COMMON_PATH_MAX);
    checkAndSetVariable(mvdSettings, "productReg", productReg, COMMON_PATH_MAX);
    checkAndSetVariable(mvdSettings, "productVer", productVer, COMMON_PATH_MAX);
    checkAndSetVariable(mvdSettings, "productPID", productPID, COMMON_PATH_MAX);
    checkAndSetVariable(mvdSettings, "productOwner", productOwner, COMMON_PATH_MAX);
    checkAndSetVariable(mvdSettings, "productName", productName, COMMON_PATH_MAX);
    
    char *serverTimeoutsDir;
    char *serverTimeoutsDirSuffix;
    if (instanceDir[strlen(instanceDir)-1] == '/') {
      serverTimeoutsDirSuffix = "serverConfig/timeouts.json";
    } else {
      serverTimeoutsDirSuffix = "/serverConfig/timeouts.json";
    }
    int serverTimeoutsDirSize = strlen(instanceDir) + strlen(serverTimeoutsDirSuffix) + 1;
    serverTimeoutsDir = safeMalloc(serverTimeoutsDirSize, "serverTimeoutsDir"); // +1 for the null-terminator
    strcpy(serverTimeoutsDir, instanceDir);
    strcat(serverTimeoutsDir, serverTimeoutsDirSuffix);

    zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_DEBUG, "reading timeout file '%s'\n", serverTimeoutsDir);
    char jsonErrorBuffer[512] = { 0 };
    int jsonErrorBufferSize = sizeof(jsonErrorBuffer);
    Json *serverTimeouts = jsonParseFile(slh, serverTimeoutsDir, jsonErrorBuffer, jsonErrorBufferSize);
    int defaultSeconds = 0;
    if (serverTimeouts) {
      dumpJson(serverTimeouts);
      defaultSeconds = getDefaultSessionTimeout(serverTimeouts);
      htUsers = getServerTimeoutsHt(slh, serverTimeouts, "users");
      htGroups = getServerTimeoutsHt(slh, serverTimeouts, "groups");
    } else {
      zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_INFO, ZSS_LOG_PARS_ZSS_TIMEOUT_MSG, serverTimeoutsDir);
    }
    safeFree(serverTimeoutsDir, serverTimeoutsDirSize);
   
    /* This one IS used*/
    checkAndSetVariableWithEnvOverride(mvdSettings, "pluginsDir", envSettings, "ZWED_pluginsDir", pluginsDir, COMMON_PATH_MAX);
        /* and these 5 are also used */
    checkAndSetVariableWithEnvOverride(mvdSettings, "productReg", envSettings, "ZWED_productReg", productReg, COMMON_PATH_MAX);
    checkAndSetVariableWithEnvOverride(mvdSettings, "productVer", envSettings, "ZWED_productVer", productVer, COMMON_PATH_MAX);
    checkAndSetVariableWithEnvOverride(mvdSettings, "productPID", envSettings, "ZWED_productPID", productPID, COMMON_PATH_MAX);
    checkAndSetVariableWithEnvOverride(mvdSettings, "productOwner", envSettings, "ZWED_productOwner", productOwner, COMMON_PATH_MAX);
    checkAndSetVariableWithEnvOverride(mvdSettings, "productName", envSettings, "ZWED_productName", productName, COMMON_PATH_MAX);

    HttpServer *server = NULL;
    int port = 0;
    char *address = NULL;
    TlsSettings *tlsSettings = NULL;
    TlsEnvironment *tlsEnv = NULL;
    bool isHttpsConfigured = readAgentHttpsSettings(slh, mvdSettings, envSettings, &address, &port, &tlsEnv);
    if (isHttpsConfigured && !tlsEnv) {
      zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_SEVERE, ZSS_LOG_HTTPS_INVALID_MSG);
      zssStatus = ZSS_STATUS_ERROR;
      goto out_term_stcbase;
    }
    if (!isHttpsConfigured) {
      readAgentAddressAndPort(mvdSettings, envSettings, &address, &port);
    }
    InetAddr *inetAddress = NULL;
    int requiredTLSFlag = 0;
    if (!validateAddress(address, &inetAddress, &requiredTLSFlag)) {
      zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_SEVERE, ZSS_LOG_SERVER_STARTUP_MSG, address);
      zssStatus = ZSS_STATUS_ERROR;
      goto out_term_stcbase;
    }
    registerProduct(productReg, productPID, productVer, productOwner, productName);

    zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_INFO, ZSS_LOG_ZSS_SETTINGS_MSG, address, port,  isHttpsConfigured ? "https" : "http");
    if (isHttpsConfigured) {
      server = makeSecureHttpServer(base, inetAddress, port, tlsEnv, requiredTLSFlag, &returnCode, &reasonCode);
    } else {
      server = makeHttpServer2(base, inetAddress, port, requiredTLSFlag, &returnCode, &reasonCode);
    }
    if (server){
      if (0 != initializeJwtKeystoreIfConfigured(mvdSettings, server, envSettings)) {
        zssStatus = ZSS_STATUS_ERROR;
        goto out_term_stcbase;
      }
      ApimlStorageSettings *apimlStorageSettings = readApimlStorageSettings(slh, mvdSettings, envSettings, tlsEnv);
      server->defaultProductURLPrefix = PRODUCT;
      initializePluginIDHashTable(server);
      loadWebServerConfig(server, mvdSettings, envSettings, htUsers, htGroups, defaultSeconds);
      readWebPluginDefinitions(server, slh, pluginsDir, serverConfigFile, apimlStorageSettings);
      installCertificateService(server);
      installUnixFileContentsService(server);
      installUnixFileRenameService(server);
      installUnixFileCopyService(server);
      installUnixFileMakeDirectoryService(server);
      installUnixFileTouchService(server);
      installUnixFileMetadataService(server);
      installUnixFileChangeOwnerService(server);
#ifdef __ZOWE_OS_ZOS
      installUnixFileChangeTagService(server);
#endif
      installUnixFileChangeModeService(server);
      installUnixFileTableOfContentsService(server); /* This needs to be registered last */
#ifdef __ZOWE_OS_ZOS
      installVSAMDatasetContentsService(server);
      installDatasetMetadataService(server);
      installDatasetContentsService(server);
      installAuthCheckService(server);
      installSecurityManagementServices(server);
      installOMVSService(server);
      installServerStatusService(server, MVD_SETTINGS, productVersion);
      installZosPasswordService(server);
      installRASService(server);
      installUserInfoService(server);
      installPassTicketService(server);
#endif
      installLoginService(server);
      installLogoutService(server);
      printZISStatus(server);
      JwkSettings jwtSettings = {0};
      jwtSettings.port = 56564;
      jwtSettings.host = "rs28.rocketsoftware.com";
      jwtSettings.path = "/gateway/api/v1/auth/keys/public/current";
      jwtSettings.timeoutSeconds = 10;
      jwtSettings.tlsEnv = tlsEnv;
      for (;;) {
        Jwk *jwk = obtainJwk(&jwtSettings);
        if (jwk) {
          fprintf (stdout, "jwk received\n");
          int rc = 0;
          httpServerInitJwtContextCustom(server, true, checkJwtSignature, jwk, &rc);
          break;
        } else {
          fprintf (stdout, "failed to obtain jwk, repeat again\n");
          sleep(2000);
        }
      }
      mainHttpLoop(server);

    } else{
      zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_SEVERE, ZSS_LOG_ZSS_STARTUP_MSG, returnCode, reasonCode);
      if (returnCode==EADDRINUSE) {
        zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_WARNING, ZSS_LOG_PORT_OCCUP_MSG, port);
      }
    }
  }

out_term_stcbase:
  stcBaseTerm(base);
  safeFree31((char *)base, sizeof(STCBase));
  base = NULL;

  return zssStatus;
}


/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

