

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
#include "jsonschema.h"
#include "configmgr.h"

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
#include "jcsi.h"
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
#include "zss.h"

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

#define LOGGING_COMPONENT_PREFIX "_zss."

static int stringEndsWith(char *s, char *suffix);
static void dumpJson(Json *json);
static JsonObject *readPluginDefinition(ShortLivedHeap *slh,
                                        char *pluginIdentifier,
                                        char *resolvedPluginLocation);
static WebPluginListElt* readWebPluginDefinitions(HttpServer* server, ShortLivedHeap *slh, char *dirname,
                                                  ConfigManager *configmgr, ApimlStorageSettings *apimlStorageSettings);
static void configureAndSetComponentLogLevel(LogComponentsMap *logComponent, JsonObject *logLevels, char *logCompPrefix);
static JsonObject *readServerSettings(ShortLivedHeap *slh, const char *filename);
static JsonObject *getDefaultServerSettings(ShortLivedHeap *slh);
static hashtable *getServerTimeoutsHt(ShortLivedHeap *slh, Json *serverTimeouts, const char *key);
static InternalAPIMap *makeInternalAPIMap(void);
static bool readGatewaySettingsV2(ConfigManager *configmgr, char **outGatewayHost, int *outGatewayPort);
static bool isMediationLayerEnabledV2(ConfigManager *configmgr);
static bool isCachingServiceEnabledV2(ConfigManager *configmgr);
static bool isJwtFallbackEnabledV2(ConfigManager *configmgr);
static bool isJwtFallbackEnabled(JsonObject *serverConfig, JsonObject *envSettings);
static void logLevelConfigurator(JsonObject *envSettings);

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
  
  char cookie[512] = {0};
  snprintf(cookie, sizeof(cookie), "%s=non-token; Path=/; HttpOnly", service->server->cookieName);
  /* Remove the session token when the user wants to log out */
  addStringHeader(response,"Set-Cookie", cookie);
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
void checkAndSetVariableWithEnvOverride(JsonObject *mvdSettings,
                         const char* configVariableName,
                         JsonObject *envSettings,
                         const char* envConfigVariableName,
                         char* target,
                         size_t targetMax);

#ifdef __ZOWE_OS_ZOS

static void setPrivilegedServerNameV2(HttpServer *server, ConfigManager *configmgr){

  CrossMemoryServerName *privilegedServerName = (CrossMemoryServerName *)SLHAlloc(server->slh, sizeof(CrossMemoryServerName));
  
  char *serverNameParm = NULL;
  int getStatus = cfgGetStringC(configmgr,ZSS_CFGNAME,&serverNameParm,3,"components","zss","crossMemoryServerName");
  if (!getStatus){
    *privilegedServerName = cmsMakeServerName(serverNameParm);
  } else {
    zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_WARNING, ZSS_LOG_PRV_SRV_NAME_MSG);
    *privilegedServerName = zisGetDefaultServerName();
  }

  setConfiguredProperty(server, HTTP_SERVER_PRIVILEGED_SERVER_PROPERTY, privilegedServerName);
}

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

static void loadWebServerConfigV2(HttpServer *server, 
                                  ConfigManager *configmgr,
                                  hashtable *htUsers,
                                  hashtable *htGroups, 
                                  int defaultSessionTimeout){
  MVD_SETTINGS = NULL; /* JOE not supportable mvdSettings; */
  /* Disabled because this server is not being used by end users, but called by other servers
   * HttpService *mainService = makeGeneratedService("main", "/");
   * mainService->serviceFunction = serveMainPage;
   * mainService->authType = SERVICE_AUTH_NONE;
   */
  server->sharedServiceMem = NULL; /* mvdSettings;  - JOE I've search everywhere and don't see this ever being used */
  server->config->userTimeouts = htUsers;
  server->config->groupTimeouts = htGroups;
  server->config->defaultTimeout = defaultSessionTimeout;
  registerHttpServiceOfLastResort(server,NULL);
#ifdef __ZOWE_OS_ZOS
  setPrivilegedServerNameV2(server, configmgr);
#endif
}

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

LOGGING_COMPONENTS_MAP(logComponents)

ZSS_LOGGING_COMPONENTS_MAP(zssLogComponents)

static void configureAndSetComponentLogLevel(LogComponentsMap *logComponent, JsonObject *logLevels, char *logCompPrefix) {
  int logLevel = ZOWE_LOG_NA;
  char *logCompName = NULL;
  int logCompLen = 0;
  while (logComponent->name != NULL) {
    logCompLen = strlen(logComponent->name) + strlen (logCompPrefix) + 1;
    logCompName = (char*) safeMalloc(logCompLen, "Log Component Name");
    memset(logCompName, '\0', logCompLen);
    strcpy(logCompName, logCompPrefix);
    strcat(logCompName, logComponent->name);
    logConfigureComponent(NULL, logComponent->compID, (char *)logComponent->name,
                      LOG_DEST_PRINTF_STDOUT, ZOWE_LOG_INFO);
    if (jsonObjectHasKey(logLevels, logCompName)) {
      logLevel = jsonObjectGetNumber(logLevels, logCompName);
      if (isLogLevelValid(logLevel)) {
        logSetLevel(NULL, logComponent->compID, logLevel);  
      }
    }
    safeFree(logCompName, logCompLen);
    ++logComponent;
  }
}

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
      LogComponentsMap *logComponent = (LogComponentsMap *)logComponents;
      configureAndSetComponentLogLevel(logComponent, logLevels, LOGGING_COMPONENT_PREFIX);
      LogComponentsMap *zssLogComponent = (LogComponentsMap *)zssLogComponents;
      configureAndSetComponentLogLevel(zssLogComponent, logLevels, LOGGING_COMPONENT_PREFIX);
    }
    dumpJson(mvdSettings);
  } else {
    zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_SEVERE, ZSS_LOG_PARS_ZSS_SETTING_MSG, filename, jsonErrorBuffer);
  }
  return mvdSettingsJsonObject;
}

#define DEFAULT_CONFIG "                                               \
                       {                                               \
                         \"productDir\": \"../defaults\",              \
                         \"siteDir\": \"../deploy/site\",              \ 
                         \"instanceDir\": \"../deploy/instance\",      \
                         \"groupsDir\": \"../deploy/instance/groups\", \
                         \"usersDir\": \"../deploy/instance/users\",   \
                         \"pluginsDir\": \"../defaults/plugins\",      \
                         \"agent\": {                                  \
                           \"http\": {                                 \
                             \"ipAddresses\": [\"127.0.0.1\"],         \
                             \"port\": 7557                            \
                            }                                          \
                         },                                            \
                         \"dataserviceAuthentication\": {              \
                           \"rbac\": false,                            \
                           \"defaultAuthentication\": \"fallback\"     \
                         }                                             \
                       }                                               \
                       "

static JsonObject *getDefaultServerSettings(ShortLivedHeap *slh) {
  char jsonErrorBuffer[512] = { 0 };
  int jsonErrorBufferSize = sizeof(jsonErrorBuffer);
  Json *mvdSettings = NULL; 
  JsonObject *mvdSettingsJsonObject = NULL;
  zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_INFO, "fallback to default server settings\n");
  mvdSettings = jsonParseString(slh, DEFAULT_CONFIG, jsonErrorBuffer, jsonErrorBufferSize);
  if (mvdSettings) {
    dumpJson(mvdSettings);
    mvdSettingsJsonObject = jsonAsObject(mvdSettings);
  } else {
    zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_SEVERE, "Failed to parse default server settings: %s\n", jsonErrorBuffer);
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
          htPut(ht, POINTER_FROM_INT(gid), POINTER_FROM_INT(timeoutValue));
        }
      } else {
        htPut(ht, userKey, POINTER_FROM_INT(timeoutValue));
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

#define ENV_LOGLEVELS_PREFIX "ZWED_logLevels_"
static int getLogLevelsFromEnv(JsonObject *envConfig, const char *identifier) {
  int len = strlen(ENV_LOGLEVELS_PREFIX) + strlen(identifier) + 1;
  int logLevel = ZOWE_LOG_NA;
  char envKey[len];
  memset(envKey,'\0',len);
  snprintf(envKey, len, "%s%s", ENV_LOGLEVELS_PREFIX, identifier);
  if(jsonObjectHasKey(envConfig, envKey)) {
    logLevel = jsonObjectGetNumber(envConfig, envKey);
    if (logLevel > ZOWE_LOG_DEBUG3 || logLevel < ZOWE_LOG_ALWAYS) {
      logLevel = ZOWE_LOG_NA;
    } 
  }
  return logLevel;
}

static int getAndLimitLogLevel(ConfigManager *configmgr, char *identifier, bool *wasSpecified){
  int logLevel = ZOWE_LOG_INFO;
  int getStatus = cfgGetIntC(configmgr,ZSS_CFGNAME,&logLevel,4,"components","zss","logLevels",identifier);
  if (getStatus == ZCFG_SUCCESS){
    *wasSpecified = true;
    return max(ZOWE_LOG_ALWAYS,min(logLevel,ZOWE_LOG_DEBUG3));
  } else{
    *wasSpecified = false;
    return ZOWE_LOG_INFO;
  }
}

static char *formatDataServiceIdentifier(const char *unformattedIdentifier, size_t identifierLength, char idNameSeparator){
  char *dataServiceIdentifier = safeMalloc(identifierLength, "dataServiceIdentifier");
  if (dataServiceIdentifier) {
    memset(dataServiceIdentifier, 0, identifierLength);
    strncpy(dataServiceIdentifier, unformattedIdentifier, identifierLength);
    for (int i = 0; dataServiceIdentifier[i]; i++) {
      if (dataServiceIdentifier[i] == '/') {
        dataServiceIdentifier[i] = idNameSeparator;
      }
    }
  }
  return dataServiceIdentifier;
}

static WebPluginListElt* readWebPluginDefinitions(HttpServer *server, ShortLivedHeap *slh, char *dirname,
                                                  ConfigManager *configmgr,
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
              bool logLevelWasSpecified = false;
              char *identifier = jsonObjectGetString(jsonObject, "identifier");
              char *pluginLocation = jsonObjectGetString(jsonObject, "pluginLocation");
              char *relativeTo = jsonObjectGetString(jsonObject, "relativeTo");
              char *resolvedPluginLocation = resolvePluginLocation(slh, pluginLocation, relativeTo);
              int pluginLogLevel = getAndLimitLogLevel(configmgr, identifier, &logLevelWasSpecified);
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
                    /*zlux does this, so don't bother doing it twice.
                      installWebPluginFilesServices(plugin, server); */
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
                          size_t identifierLength = strlen(plugin->dataServices[i]->identifier) + 1;
                          char *key = formatDataServiceIdentifier(plugin->dataServices[i]->identifier, 
                                                                  identifierLength, ':');
                          if (key) {
                            htPut(server->loggingIdsByName,
                                  key,
                                  &(plugin->dataServices[i]->loggingIdentifier));
                          }
                          uint64_t logCompID = plugin->dataServices[i]->loggingIdentifier;
                          char *dataServiceIdentifier = formatDataServiceIdentifier(plugin->dataServices[i]->identifier, 
                                                                                    identifierLength, '.');  
                          if (dataServiceIdentifier) {
                            logLevelWasSpecified = false;
                            int dataServiceLogLevel = getAndLimitLogLevel(configmgr, dataServiceIdentifier, &logLevelWasSpecified);
                            if (logLevelWasSpecified){
                              logSetLevel(NULL, logCompID, dataServiceLogLevel);
                            }
                            safeFree(dataServiceIdentifier, identifierLength);
                          }                                                
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
    zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_WARNING, ZSS_LOG_OPEN_PLGDIR_FAIL_MSG, dirname, returnCode, reasonCode);
  }
  if (webPluginListHead) {
    installWebPluginDefintionsService(webPluginListHead, server);
  } 
  zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_DEBUG2, "%s end result %d\n", __FUNCTION__, pluginDefinitionCount);
  return webPluginListHead;
}

static ApimlStorageSettings *readApimlStorageSettingsV2(ShortLivedHeap *slh, ConfigManager *configmgr, TlsEnvironment *tlsEnv) {
  char *host = NULL;
  int port = 0;
  bool configured = false;

  do {
    if (!isCachingServiceEnabledV2(configmgr)){
      zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_DEBUG, "Caching Service disabled\n");
      break;
    }
    if (!readGatewaySettingsV2(configmgr, &host, &port)) {
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

static JwkSettings *readJwkSettingsV2(ShortLivedHeap *slh, ConfigManager *configmgr, TlsEnvironment *tlsEnv) {
  char *host = NULL;
  int port = 0;
  bool configured = false;
  bool fallback = false;

  do {
    if (!isMediationLayerEnabledV2(configmgr)){
      zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_DEBUG, "APIML disabled\n");
      break;
    }
    if (!readGatewaySettingsV2(configmgr, &host, &port)) {
      zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_DEBUG, "Gateway settings not found\n");
      break;
    }
    if (!tlsEnv) {
      zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_DEBUG, "TLS settings not found\n");
      break;
    }
    fallback = isJwtFallbackEnabledV2(configmgr);
    configured = true;
  } while(0);

  if (!configured) {
    zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_DEBUG, "JWK URL not configured");
    return NULL;
  }
  JwkSettings *settings = (JwkSettings*)SLHAlloc(slh, sizeof(*settings));
  memset(settings, 0, sizeof(*settings));
  settings->host = host; 
  settings->port = port;
  settings->tlsEnv = tlsEnv;
  settings->retryIntervalSeconds=getJwtRetryIntervalSeconds(configmgr);
  settings->timeoutSeconds = 10;
  settings->path = "/gateway/api/v1/auth/keys/public/current";
  settings->fallback = fallback;
  return settings;
}

/* static const char defaultConfigPath[] = "../defaults/serverConfig/server.json"; */

static void checkAndSetVariableWithEnvOverride(JsonObject *mvdSettings,
                                               const char* configVariableName,
                                               JsonObject *envSettings,
                                               const char* envConfigVariableName,
                                               char* target,
                                               size_t targetMax) {
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

static bool checkAndSetVariableV2(ConfigManager *configmgr,
                                  const char *configVariableName,
                                  char *target,
                                  size_t targetMax){
  /* here */
  char *value;
  int getStatus = cfgGetStringC(configmgr,ZSS_CFGNAME,&value,3,"components","zss",configVariableName);
  if (!getStatus && (strlen(value)+1 < targetMax)){
    int len = strlen(value);
    memcpy(target,value,len);
    target[len] = 0;
    return true;
  } else{
    zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_SEVERE, 
            "internal error accessing .components.zss.'%s', err=%d\n", configVariableName,getStatus);
    return false;
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
  logConfigureComponent(NULL, LOG_COMP_ID_JWK, "JWK", LOG_DEST_PRINTF_STDOUT, ZOWE_LOG_INFO);
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

/* This is the non-TLS version */
static void readAgentAddressAndPortV2(ConfigManager *configmgr, char **addressHandle, int *port) {
  char *address = NULL;
  int getStatus = cfgGetStringC(configmgr,ZSS_CFGNAME,&address,6,"components","zss","agent","http","ipAddresses",0);
  if (getStatus == ZCFG_SUCCESS){
    *addressHandle = address;
  } else{
    zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_SEVERE, 
            "internal error accessing .components.zss.agent.http.ipAddresses, err=%d\n", getStatus);
    *addressHandle = "127.0.0.1";  /* Non-ATTLS default */
  }
  /* no defaulting logic for port, and validation of schema should do good things */
  getStatus = cfgGetIntC(configmgr,ZSS_CFGNAME,port,3,"components","zss","port");
  if (getStatus){
    zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_SEVERE, "internal error accessing .components.zss.port, err=%d\n", getStatus);
  }
}

static char* generateCookieNameV2(ConfigManager *configmgr, int port) {
  int cookieLength=256;
  char *cookieName = safeMalloc(cookieLength+1, "CookieName");
  char *zoweInstanceId = getenv("ZOWE_INSTANCE");
  char *haInstanceCountStr = getenv("ZWE_HA_INSTANCES_COUNT");
  int haInstanceCount=0;
  if (haInstanceCountStr != NULL) {
    haInstanceCount = atoi(haInstanceCountStr);
  }
  if (haInstanceCount > 1 && zoweInstanceId != NULL) {
    snprintf(cookieName, cookieLength, "%s.%s", SESSION_TOKEN_COOKIE_NAME, zoweInstanceId);
  } else {
    snprintf(cookieName, cookieLength, "%s.%d", SESSION_TOKEN_COOKIE_NAME, port);
  }
  zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_DEBUG, "Cookie name set as %s\n",cookieName);
  return cookieName;
}

static char* generateCookieName(JsonObject *envConfig, int port) {
  int cookieLength=256;
  char *cookieName = safeMalloc(cookieLength+1, "CookieName");
  char *zoweInstanceId = getenv("ZWE_zowe_cookieIdentifier");
  if (zoweInstanceId != NULL) {
    snprintf(cookieName, cookieLength, "%s.%s", SESSION_TOKEN_COOKIE_NAME, zoweInstanceId);
  } else {
    snprintf(cookieName, cookieLength, "%s.%d", SESSION_TOKEN_COOKIE_NAME, port);
  }
  zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_DEBUG, "Cookie name set as %s\n",cookieName);
  return cookieName;
}


#define PORT_KEY         "port"
#define IP_ADDRESSES_KEY "ipAddresses"
#define KEYRING_KEY      "keyring"
#define LABEL_KEY        "label"
#define STASH_KEY        "stash"
#define PASSWORD_KEY     "password"

#define AGENT_HTTPS_PREFIX       "ZWED_agent_https_"
#define ENV_AGENT_HTTPS_KEY(key) AGENT_HTTPS_PREFIX key

static bool readAgentHttpsSettingsV2(ShortLivedHeap *slh,
                                     ConfigManager *configmgr,
                                     char **outAddress,
                                     int *outPort,
                                     TlsEnvironment **outTlsEnv){  
  Json *httpsConfig = NULL;
  int httpsGetStatus = cfgGetAnyC(configmgr,ZSS_CFGNAME,&httpsConfig,4,"components","zss","agent","https");
  if (httpsGetStatus){
    zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_INFO, "https is NOT configured for this ZSS\n");
    return false;
  }
  JsonObject *httpsConfigObject = jsonAsObject(httpsConfig);
  TlsSettings *settings = (TlsSettings*)SLHAlloc(slh, sizeof(*settings));
  settings->ciphers = DEFAULT_TLS_CIPHERS;
  settings->keyring = jsonObjectGetString(httpsConfigObject, "keyring");
  settings->label = jsonObjectGetString(httpsConfigObject, "label");
  /*  settings->stash = jsonObjectGetString(httpsConfigObject, "stash"); - this is obsolete */
  settings->password = jsonObjectGetString(httpsConfigObject, "password");
  JsonArray *addressArray = jsonObjectGetArray(httpsConfigObject,"ipAddresses");
  *outAddress = jsonArrayGetString(addressArray,0);
  int getStatus = cfgGetIntC(configmgr,ZSS_CFGNAME,outPort,3,"components","zss","port");
  if (getStatus){
    zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_SEVERE, "internal error accessing .components.zss.port, err=%d\n", getStatus);
  }
  bool useTls = false;
  cfgGetBooleanC(configmgr,ZSS_CFGNAME,&useTls,3,"components","zss","tls");
  bool isHttpsConfigured = useTls && settings->keyring;
  if (isHttpsConfigured){
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
  return isHttpsConfigured;
}

#define NODE_MEDIATION_LAYER_SERVER_PREFIX   "ZWED_node_mediationLayer_server_"
#define ENV_NODE_MEDIATION_LAYER_SERVER_KEY(key) NODE_MEDIATION_LAYER_SERVER_PREFIX key

/* Returns false if gateway host and port not found */
static bool readGatewaySettingsV2(ConfigManager *configmgr,
                                  char **outGatewayHost,
                                  int *outGatewayPort
                                  ) {
  Json *gatewaySettings = NULL;
  int gatewayGetStatus = cfgGetAnyC(configmgr,ZSS_CFGNAME,&gatewaySettings,5,"components","zss","agent","mediationLayer","server");
  if (gatewayGetStatus){
    zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_INFO, "gateway is NOT configured for this ZSS\n");
    return false;
  }
  JsonObject *gatewaySettingsObject = jsonAsObject(gatewaySettings);
  *outGatewayHost = jsonObjectGetString(gatewaySettingsObject,"gatewayHostname");
  *outGatewayPort = jsonObjectGetNumber(gatewaySettingsObject,"gatewayPort");
  return true;
}

#define NODE_MEDIATION_LAYER_PREFIX   "ZWED_node_mediationLayer_"
#define ENV_NODE_MEDIATION_LAYER_KEY(key) NODE_MEDIATION_LAYER_PREFIX key
#define NODE_MEDIATION_LAYER_CACHING_SERVICE_PREFIX   NODE_MEDIATION_LAYER_PREFIX "cachingService_"
#define NODE_MEDIATION_LAYER_CACHING_SERVICE_KEY(key) NODE_MEDIATION_LAYER_CACHING_SERVICE_PREFIX key
#define ENABLED_KEY "enabled"

static bool isMediationLayerEnabledV2(ConfigManager *configmgr) {
  /* JOE: Sean is this /comp/zss/node/mediationLayer/enabled */
  bool enabled = false;
  int getStatus = cfgGetBooleanC(configmgr,ZSS_CFGNAME,&enabled,5,"components","zss","agent","mediationLayer","enabled");
  if (getStatus == ZCFG_SUCCESS){
    return enabled;
  } else{
    zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_SEVERE, 
            "internal error accessing .components.zss.agent.mediationLayer.enabled, err=%d\n", getStatus);
    return false;
  }
  /*
    is this "/components/gatweway/(port|enabled..)
    what about mediation layer enabled? - roughly, this means gateway enabled, *AND* with schema being stonger 
                                                       "enabled" implies that host/port defined (etc)

    where is gateway host in the the yaml ?   zowe/externalDomains/[0]  - ask Jack about converter
                                              haInstance/hostname - this is probably synthetic 
   */
}

#define AGENT_JWT_PREFIX       "ZWED_agent_jwt_"
#define ENV_AGENT_JWT_KEY(key) AGENT_JWT_PREFIX key

static bool isJwtFallbackEnabledV2(ConfigManager *configmgr){
  bool enabled = false;
  int getStatus = cfgGetBooleanC(configmgr,ZSS_CFGNAME,&enabled,5,"components","zss","agent","jwt","fallback");
  if (getStatus == ZCFG_SUCCESS){
    return enabled;
  } else{    
    zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_SEVERE, 
            "internal error accessing .components.zss.agent.mediationLayer.cachingService.enabled, err=%d\n", getStatus);
    return false;
  }
}

static int getJwtRetryIntervalSeconds(ConfigManager *configmgr) {
  int retryVal = 0;
  int retryDefault = 10;
  int getStatus = cfgGetIntC(configmgr, ZSS_CFGNAME, &retryVal, 5, "components", "zss", "agent", "jwt", "retryIntervalSeconds");
  if (getStatus != ZCFG_SUCCESS) {
    zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_WARNING, "Error loading components.zss.agent.jwt.retryIntervalSeconds, defaulting to %d\n", retryDefault);
    return retryDefault;
  } else{
    return retryVal;
  }
}


static bool isJwtFallbackEnabled(JsonObject *serverConfig, JsonObject *envSettings) {
  Json *fallbackJson = jsonObjectGetPropertyValue(envSettings, ENV_AGENT_JWT_KEY("fallback"));
  if (fallbackJson) {
    return jsonIsBoolean(fallbackJson) ? jsonAsBoolean(fallbackJson) : false;
  }
  JsonObject *const agentSettings = jsonObjectGetObject(serverConfig, "agent");
  if (!agentSettings) {
    return false;
  }
  JsonObject *jwtSettings = jsonObjectGetObject(agentSettings, "jwt");
  if (!jwtSettings) {
    return false;
  }
  fallbackJson = jsonObjectGetPropertyValue(jwtSettings, "fallback");
  if (fallbackJson) {
    return jsonIsBoolean(fallbackJson) ? jsonAsBoolean(fallbackJson) : false;
  }
  return false;
}

static bool isCachingServiceEnabledV2(ConfigManager *configmgr){
  /* JOE: Sean, this seems to be under zss/agent in V1, was Tuesday discussion correct ? */
  bool enabled = false;
  int getStatus = cfgGetBooleanC(configmgr,ZSS_CFGNAME,&enabled,6,"components","zss","agent","mediationLayer","cachingService","enabled");
  if (getStatus == ZCFG_SUCCESS){
    return enabled;
  } else{    
    zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_SEVERE, 
            "internal error accessing .components.zss.agent.mediationLayer.cachingService.enabled, err=%d\n", getStatus);
    return false;
  }
}

/* returns valid */
static int validateAddress(char *address, InetAddr **inetAddress, int *requiredTLSFlag) {
  *inetAddress = getAddressByName(address);
  if (strcmp(address,"127.0.0.1") && strcmp(address,"localhost")) {
#ifndef USE_ZOWE_TLS
    zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_WARNING, 
      ZSS_LOG_HTTPS_NO_IMPLEM_MSG,
      address);
#endif /* USE_ZOWE_TLS */
    *requiredTLSFlag = RS_TLS_WANT_TLS;
  }
  if (!strcmp(address,"0.0.0.0")) {
    return TRUE;      
  }
  if (!(*inetAddress && (*inetAddress)->data.data4.addrBytes)) {
    printf("address resolution problems\n");
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
  } 
  /* else if (((stat.fileType == BPXSTA_FILETYPE_DIRECTORY) && (stat.flags3 & FORBIDDEN_GROUP_DIR_PERMISSION)) 
      || ((stat.fileType != BPXSTA_FILETYPE_DIRECTORY) && (stat.flags3 & FORBIDDEN_GROUP_FILE_PERMISSION))
      || (stat.flags3 & FORBIDDEN_OTHER_PERMISSION)) {
    zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_SEVERE,
      ZSS_LOG_CONFIG_OPEN_PERMISS_MSG,path);
    zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_SEVERE,
      ZSS_LOG_ENSURE_PERMISS_MSG);
    return 8;
  }
  */
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

static void readAndConfigureLogLevelFromConfig(LogComponentsMap *logComponent, ConfigManager *configmgr, char *logCompPrefix) {
  int logLevel = ZOWE_LOG_NA;
  char *logCompName = NULL;
  int logCompLen = 0;
  while (logComponent->name != NULL) {
    logCompLen = strlen(logComponent->name) + strlen (logCompPrefix) + 1;
    logCompName = (char*) safeMalloc(logCompLen, "Log Component Name");
    memset(logCompName, '\0', logCompLen);
    strcpy(logCompName, logCompPrefix);
    strcat(logCompName, logComponent->name);
    logConfigureComponent(NULL, logComponent->compID, (char *)logComponent->name,
                          LOG_DEST_PRINTF_STDOUT, ZOWE_LOG_INFO);
    if (true){
      int cfgStatus = cfgGetIntC(configmgr,ZSS_CFGNAME,&logLevel,4,"components","zss","logLevels",logCompName);
      if (!cfgStatus && isLogLevelValid(logLevel)) {
        logSetLevel(NULL, logComponent->compID, logLevel);  
      }
    }
    safeFree(logCompName, logCompLen);
    ++logComponent;
  }
}

static void logLevelConfiguratorV2(ConfigManager *configmgr){
  LogComponentsMap *logComponent = (LogComponentsMap *)logComponents;
  readAndConfigureLogLevelFromConfig(logComponent, configmgr, LOGGING_COMPONENT_PREFIX);
  LogComponentsMap *zssLogComponent = (LogComponentsMap *)zssLogComponents;
  readAndConfigureLogLevelFromConfig(zssLogComponent, configmgr, LOGGING_COMPONENT_PREFIX);

  /* special (old!) ones that arent real loggers, just trace conditionals */  
  TraceDefinition *traceDef = traceDefs;
  int logLevel;

  while (traceDef->name != 0){
    int cfgStatus = cfgGetIntC(configmgr,ZSS_CFGNAME,&logLevel,4,"components","zss","logLevels",(char*) traceDef->name);
    if (!cfgStatus && isLogLevelValid(logLevel)) {
      traceDef->function(logLevel);
    }
    ++traceDef;
  }
}

#define NOT_INTEGER 8
#define DOUBLE_COMMA 12

static int parseIntPair(char *s, int *i1Ptr, int *i2Ptr){
  int commaPos = 0;
  int len = strlen(s);
  int i1 = 0;
  int i2 = 0;
  for (int i=0; i<len; i++){
    char c = s[i];
    if (c == ','){
      if (commaPos){
        return DOUBLE_COMMA;
      }
      commaPos = i;
    } else if (c < '0' || c > '9'){
      return NOT_INTEGER;
    } else{
      printf("commaPos %d c %c\n",commaPos,c);
      if (commaPos){
        i2 = (i2*10)+(c-'0');
      } else{
        i1 = (i1*10)+(c-'0');
      }
    }
  }
  *i1Ptr = i1;
  *i2Ptr = i2;
  return 0;
}

#define STDOUT_FILENO 1
#define STDERR_FILENO 2

static int forceLogToFile(char *logFileName){
  printf("ZSS Logging To File\n");
  FILE *logOut = fopen(logFileName,"w");
  if (logOut){
    dup2(fileno(logOut),STDOUT_FILENO);
    dup2(fileno(logOut),STDERR_FILENO);
    return 0;
  } else{
    printf("Logging to file failed! '%s' error=%d\n",logFileName,errno);
    return 8;
  }
}

static char *getKeywordArg(char *key, int argc, char **argv){
  for (int aa=1; aa<argc; aa++){
    if (!strcmp(argv[aa],key) &&
        (aa+1 < argc)){
      return argv[aa+1];
    }
  }
  return NULL;

}

static void displayValidityException(FILE *out, int depth, ValidityException *exception){
  for (int i=0; i<depth; i++){
    fprintf(out,"  ");
  }
  fprintf(out,"%s\n",exception->message);
  ValidityException *child = exception->firstChild;
  while (child){
    displayValidityException(out,depth+1,child);
    child = child->nextSibling;
  }
}


static bool validateConfiguration(ConfigManager *cmgr, FILE *out){
  bool ok = false;
  JsonValidator *validator = makeJsonValidator();
  validator->traceLevel = cmgr->traceLevel;
  int validateStatus = cfgValidate(cmgr,validator,ZSS_CFGNAME);

  switch (validateStatus){
  case JSON_VALIDATOR_NO_EXCEPTIONS:
    zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_INFO, "Configuration is Valid\n");
    ok = true;
    break;
  case JSON_VALIDATOR_HAS_EXCEPTIONS:
    zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_INFO, "Configuration has validity exceptions:\n");
    displayValidityException(out,0,validator->topValidityException);
  break;
  case JSON_VALIDATOR_INTERNAL_FAILURE:
    zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_INFO, "Internal failure during validation, please contact support\n");
    break;
  }
  freeJsonValidator(validator);
  return ok;
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


  /* TODO consider moving this to stcBaseInit */
#ifndef METTLE
  int sigignoreRC = sigignore(SIGPIPE);
  if (sigignoreRC == -1) {
    /* actually sigignore(SIGPIPE) never fails because we pass valid signum SIGPIPE */
    zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_DEBUG, "Failed to ignore SIGPIPE, errno='%d' - %s\n", errno, strerror(errno));
  }
#endif

  const char *serverConfigFile = NULL;
  int returnCode = 0;
  int reasonCode = 0;
  char productDir[COMMON_PATH_MAX] = {0};
  char siteDir[COMMON_PATH_MAX] = {0};
  char instanceDir[COMMON_PATH_MAX] = {0};
  char groupsDir[COMMON_PATH_MAX] = {0};
  char usersDir[COMMON_PATH_MAX] = {0};
  char pluginsDir[COMMON_PATH_MAX] = {0};
  char productReg[COMMON_PATH_MAX] = {0};
  char productPID[COMMON_PATH_MAX] = {0};
  char productVer[COMMON_PATH_MAX] = {0};
  char productOwner[COMMON_PATH_MAX] = {0};
  char productName[COMMON_PATH_MAX] = {0};
  char *tempString;
  hashtable *htUsers = NULL;
  hashtable *htGroups = NULL;
  char *schemas = getKeywordArg("--schemas",argc,argv);
  char *configs = getKeywordArg("--configs",argc,argv);
  char *configmgrTraceLevelString = getKeywordArg("--configTrace",argc,argv);
  if (schemas == NULL || configs == NULL){
    zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_INFO, "ZSS 2.x requires schemas and config\n");
    zssStatus = ZSS_STATUS_ERROR;
    goto out_term_stcbase;
  }
  int configmgrTraceLevel = 0;
  if (configmgrTraceLevelString != NULL) {
    long int configmgrTraceConverted =  strtol(configmgrTraceLevelString, NULL, 10);
    if (configmgrTraceConverted > 0 && configmgrTraceConverted < 6) {
      configmgrTraceLevel = (int)configmgrTraceConverted;
    }
  }
  ConfigManager *configmgr
    /* HERE set up a directory from things from Sean */
    = makeConfigManager(); /* configs,schemas,1,stderr); */
  CFGConfig *theConfig = addConfig(configmgr,ZSS_CFGNAME);
  cfgSetTraceStream(configmgr,stderr);
  cfgSetTraceLevel(configmgr, configmgrTraceLevel);
  cfgSetConfigPath(configmgr,ZSS_CFGNAME,configs);
  int schemaLoadStatus = cfgLoadSchemas(configmgr,ZSS_CFGNAME,schemas);
  if (schemaLoadStatus){
    zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_INFO, "ZSS Could not load schemas, status=%d\n", schemaLoadStatus);
    zssStatus = ZSS_STATUS_ERROR;
    goto out_term_stcbase;
  }

  if (cfgLoadConfiguration(configmgr,ZSS_CFGNAME) != 0){
    zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_INFO, "ZSS Could not load configurations\n");
    zssStatus = ZSS_STATUS_ERROR;
    goto out_term_stcbase;
  }
  if (!validateConfiguration(configmgr,stdout)){
    zssStatus = ZSS_STATUS_ERROR;
    goto out_term_stcbase;
  }
  
  char *pipeFDArg = getKeywordArg("-pipes",argc,argv);
  char *authBlobB64 = getKeywordArg("-authBlob",argc,argv);
  char *logfile = getKeywordArg("-log",argc,argv);
  if ((pipeFDArg || authBlobB64 || logfile) &&
      !(pipeFDArg && authBlobB64 && logfile) ){
    zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_INFO, 
            "ZSS Piped/SingleUser mode requires 3 args, '-pipes', 'authBlob', and '-log'\n");
    zssStatus = ZSS_STATUS_ERROR;
    goto out_term_stcbase;
  }
  if (authBlobB64 && (strlen(authBlobB64) < 23)) {  /* MD5 with B64 inflation */
    zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_INFO, "Auth blob is too short\n");
    zssStatus = ZSS_STATUS_ERROR;
    goto out_term_stcbase;
  }
  

  ShortLivedHeap *slh = makeShortLivedHeap(0x40000, 0x40);

  logLevelConfiguratorV2(configmgr);
  JsonObject *mvdSettings = NULL;
  JsonObject *envSettings = NULL;
  /*
    JOE: More legacy stuff
    if (serverConfigFile) {
      mvdSettings = readServerSettings(slh, serverConfigFile);
    } else {
      mvdSettings = getDefaultServerSettings(slh);
    }
    */

  bool hasProductReg = false;
  if (configmgr) { /* mvdSettings */
    int missingDirs = 0;
    if (!checkAndSetVariableV2(configmgr, "pluginsDir", pluginsDir, COMMON_PATH_MAX)) missingDirs++;
    if (!checkAndSetVariableV2(configmgr, "productDir", productDir, COMMON_PATH_MAX)) missingDirs++;
    if (!checkAndSetVariableV2(configmgr, "instanceDir", instanceDir, COMMON_PATH_MAX)) missingDirs++;
    /*     ZWED_productReg=enable
           ZWED_productVer=1.0
           ZWED_productPID= 8 characters
           ZWED_productOwner= 16 characters
           ZWED_productName= 16 characters (edited) 
           */
    if (checkAndSetVariableV2(configmgr, "productReg", productReg, COMMON_PATH_MAX) &&
        checkAndSetVariableV2(configmgr, "productVer", productVer, COMMON_PATH_MAX) &&
        checkAndSetVariableV2(configmgr, "productPID", productPID, COMMON_PATH_MAX) &&
        checkAndSetVariableV2(configmgr, "productOwner", productOwner, COMMON_PATH_MAX) &&
        checkAndSetVariableV2(configmgr, "productName",  productName, COMMON_PATH_MAX)){
      hasProductReg = true;
    }
    if (missingDirs){
      zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_SEVERE, "at least one of required app-server dir or product info missing");
      zssStatus = ZSS_STATUS_ERROR;
      goto out_term_stcbase;
    }
 
    char *serverTimeoutsDirSuffix;
    int instanceDirLen = strlen(instanceDir);
    if (instanceDirLen == 0 || instanceDir[instanceDirLen-1] == '/') {
      serverTimeoutsDirSuffix = "serverConfig/timeouts.json";
    } else {
      serverTimeoutsDirSuffix = "/serverConfig/timeouts.json";
    }
    int serverTimeoutsDirSize = strlen(instanceDir) + strlen(serverTimeoutsDirSuffix) + 1;
    char serverTimeoutsDir[serverTimeoutsDirSize];
    strcpy(serverTimeoutsDir, instanceDir);
    strcat(serverTimeoutsDir, serverTimeoutsDirSuffix);

    zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_DEBUG, "reading timeout file '%s'\n", serverTimeoutsDir);
    char jsonErrorBuffer[512] = { 0 };
    int jsonErrorBufferSize = sizeof(jsonErrorBuffer);
    Json *serverTimeouts = NULL; /* JOE */ jsonParseFile(slh, serverTimeoutsDir, jsonErrorBuffer, jsonErrorBufferSize);
    int defaultSeconds = 0;
    if (serverTimeouts) {
      dumpJson(serverTimeouts);
      defaultSeconds = getDefaultSessionTimeout(serverTimeouts);
      htUsers = getServerTimeoutsHt(slh, serverTimeouts, "users");
      htGroups = getServerTimeoutsHt(slh, serverTimeouts, "groups");
    } else {
      zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_INFO, ZSS_LOG_PARS_ZSS_TIMEOUT_MSG, serverTimeoutsDir);
    }
   
    /* This one IS used*/

    HttpServer *server = NULL;
    int port = 0;
    char *address = NULL;
    TlsSettings *tlsSettings = NULL;
    TlsEnvironment *tlsEnv = NULL;
    InetAddr *inetAddress = NULL;
    int requiredTLSFlag = 0;
    bool isHttpsConfigured = false;
    
    if (pipeFDArg){ /* The tunnel case */
      char *cookieName = generateCookieNameV2(configmgr, HTTP_DISABLE_TCP_PORT);
      server = makeHttpServer2(base, NULL, HTTP_DISABLE_TCP_PORT, requiredTLSFlag, &returnCode, &reasonCode);
      int fromDispatcherFD = 0;
      int toDispatcherFD = 0;
      int i;
      char *authBlob = safeMalloc(strlen(authBlobB64),"AuthBlob");
      char *authBlob16 = safeMalloc(16,"AuthBlob16"); /* Assume MD5 for now */
      int decodedBlobLength = decodeBase64(authBlobB64,authBlob);
      memcpy(authBlob16,authBlob,16);
      printf("authBlob\n");
      dumpbuffer(authBlob,16);
      if (parseIntPair(pipeFDArg,&fromDispatcherFD,&toDispatcherFD)){
        zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_SEVERE, "Failed to parse pipeFD argument: '%s'", pipeFDArg);
        zssStatus = ZSS_STATUS_ERROR;
        goto out_term_stcbase;
      } else{
        httpServerEnablePipes(server,fromDispatcherFD,toDispatcherFD);
        server->singleUserAuthBlob = authBlob16;
      }
    } else {
      isHttpsConfigured = readAgentHttpsSettingsV2(slh, configmgr, &address, &port, &tlsEnv);
      if (isHttpsConfigured && !tlsEnv) {
        zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_SEVERE, ZSS_LOG_HTTPS_INVALID_MSG);
        zssStatus = ZSS_STATUS_ERROR;
        goto out_term_stcbase;
      }
      if (!isHttpsConfigured) {
        readAgentAddressAndPortV2(configmgr, &address, &port);
      }
      if (!validateAddress(address, &inetAddress, &requiredTLSFlag)) {
        zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_SEVERE, ZSS_LOG_SERVER_STARTUP_MSG, address);
        zssStatus = ZSS_STATUS_ERROR;
        goto out_term_stcbase;
      } 
      zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_INFO, ZSS_LOG_ZSS_SETTINGS_MSG, address, port,  isHttpsConfigured ? "https" : "http");
      
      char *cookieName = generateCookieNameV2(configmgr, port);
      if (isHttpsConfigured) {
        server = makeSecureHttpServer2(base, inetAddress, port, tlsEnv, requiredTLSFlag,
                                       cookieName, &returnCode, &reasonCode);
      } else {
        server = makeHttpServer3(base, inetAddress, port, requiredTLSFlag,
                                 cookieName, &returnCode, &reasonCode);
      }
    }
    if (hasProductReg){ 
      registerProduct(productReg, productPID, productVer, productOwner, productName);
    }
    
    zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_INFO, "made http(s) server at 0x%p\n",server);
    if (server){
      httpServerConfigManager(server) = configmgr;
      ApimlStorageSettings *apimlStorageSettings = readApimlStorageSettingsV2(slh, configmgr, tlsEnv);
      JwkSettings *jwkSettings = readJwkSettingsV2(slh, configmgr, tlsEnv);
      server->defaultProductURLPrefix = PRODUCT;
      initializePluginIDHashTable(server);
      loadWebServerConfigV2(server, configmgr, htUsers, htGroups, defaultSeconds);
      readWebPluginDefinitions(server, slh, pluginsDir, configmgr, apimlStorageSettings);
      configureJwt(server, jwkSettings);
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
      loadCsi();
      installJesService(server);
      installVSAMDatasetContentsService(server);
      installDatasetMetadataService(server);
      installDatasetContentsService(server);
      installAuthCheckService(server);
      installSecurityManagementServices(server);
      installOMVSService(server);
      installServerStatusService(server, productVersion);
      installZosPasswordService(server);
      installRASService(server);
      installUserInfoService(server);
      installPassTicketService(server);
      installSAFIdtTokenService(server);
#endif
      installLoginService(server);
      installLogoutService(server);
      printZISStatus(server);
      mainHttpLoop(server);

    } else{
      zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_SEVERE, ZSS_LOG_ZSS_STARTUP_MSG, returnCode, reasonCode);
      if (returnCode==EADDRINUSE) {
        zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_SEVERE, ZSS_LOG_PORT_OCCUP_MSG, port);
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


